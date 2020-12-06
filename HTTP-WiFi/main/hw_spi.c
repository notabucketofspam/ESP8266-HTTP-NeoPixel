#include "hw_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sets up the GPIO pin as a falling-edge interrupt with the pin number as a parameter
 * Change to GPIO_INTR_POSEDGE, maybe
 */
#define HW_GPIO_CONFIG_DEFAULT(x) { \
  .pin_bit_mask = BIT(x), \
  .mode = GPIO_MODE_INPUT, \
  .pull_up_en = GPIO_PULLUP_DISABLE, \
  .pull_down_en = GPIO_PULLDOWN_DISABLE, \
  .intr_type = GPIO_INTR_NEGEDGE \
}
/*
 * Sets up SPI as master with clk_div selected as an input
 */
#define HW_SPI_CONFIG_DEFAULT(x) { \
  .interface.val = SPI_DEFAULT_INTERFACE, \
  .intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE, \
  .mode = SPI_MASTER_MODE, \
  .clk_div = x, \
  .event_cb = NULL \
}
/*
 * Max size of a single SPI transfer on the ESP8266
 */
#define HW_DATA_CHUNK_SIZE (64)
/*
 * Useful for getting the minimum number of data chunks needed to transmit a struct, for example
 */
#define HW_DATA_CHUNK_COUNT(x) ((sizeof(x) + (HW_DATA_CHUNK_SIZE - 1)) / HW_DATA_CHUNK_SIZE)
/*
 * Direction of SPI communication.
 * Pretty much only used for spi_master_transmit()
 */
enum spi_master_mode {
  SPI_NULL = 0,
  SPI_WRITE,
  SPI_READ
};
/*
 * Send or receive data as the master
 * Assumes that the data length is 64-byte (the maximum allowed by the ESP8266 in a single transmission),
 * thus data can only be sixteen uint32_t's long
 */
static void IRAM_ATTR spi_master_transmit(enum spi_master_mode direction, uint32_t *data);
/*
 * Send the length of the subsequent transmission
 * I don't know how this is useful since spi_master_transmit() assumes a 64-byte length every time
 */
static void IRAM_ATTR spi_master_send_length(uint32_t length);
/*
 * Deals with situations when the handshake pin is raised
 */
static void IRAM_ATTR gpio_isr_handler(void *arg);
/*
 * A debug function for printing the contents of a pattern data struct
 */
static void print_pattern_data(struct hw_pattern_data *pattern_data);

static void IRAM_ATTR spi_master_transmit(enum spi_master_mode direction, uint32_t *data) {
  spi_trans_t transmission;
  uint16_t cmd;
  uint32_t addr = 0x00; // Zero because this is a data command, not a status command
  memset(&transmission, 0x00, sizeof(transmission)); // Make sure transmission is clear
  transmission.bits.val = 0;
  switch (direction) {
    case SPI_WRITE:
      cmd = SPI_MASTER_WRITE_DATA_TO_SLAVE_CMD;
      transmission.bits.mosi = 8 * HW_DATA_CHUNK_SIZE; // 8-bit by 64-byte
      transmission.mosi = data; // Not &data because data is already a pointer
      break;
    case SPI_READ:
      cmd = SPI_MASTER_READ_DATA_FROM_SLAVE_CMD;
      transmission.bits.miso = 8 * HW_DATA_CHUNK_SIZE;
      transmission.miso = data;
      break;
    default:
      break;
  }
  transmission.bits.cmd = 8 * 1; // Single byte command
  transmission.bits.addr = 8 * 1;
  transmission.cmd = &cmd; // spi_trans_t expects a pointer here
  transmission.addr = &addr;
  spi_trans(HSPI_HOST, &transmission);
}
static void IRAM_ATTR spi_master_send_length(uint32_t length) {
  spi_trans_t transmission;
  uint16_t cmd = SPI_MASTER_WRITE_STATUS_TO_SLAVE_CMD;
  memset(&transmission, 0x00, sizeof(transmission));
  transmission.bits.val = 0;
  transmission.bits.cmd = 8 * 1;
  transmission.bits.addr = 0; // Zero because this is a status command
  transmission.bits.mosi = 8 * 4; // The status registers are 32-bit
  transmission.cmd = &cmd;
  transmission.addr = NULL; // NULL because again status commands don't use the address
  transmission.mosi = &length;
  spi_trans(HSPI_HOST, &transmission);
}
static void IRAM_ATTR gpio_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(spi_write_task_handle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}
static void print_pattern_data(struct hw_pattern_data *pattern_data) {
  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
}
void hw_setup_spi(void) {
//  spi_semaphore_handle = xSemaphoreCreateBinary();
  gpio_config_t io_config = HW_GPIO_CONFIG_DEFAULT(CONFIG_HW_SPI_HANDSHAKE);
  gpio_config(&io_config);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(CONFIG_HW_SPI_HANDSHAKE, gpio_isr_handler, NULL);
  spi_config_t spi_config = HW_SPI_CONFIG_DEFAULT(CONFIG_HW_SPI_CLK_DIV);
  spi_init(HSPI_HOST, &spi_config);
}
void IRAM_ATTR hw_spi_master_write_task(void *arg) {
  // Divide by four because a uint32_t is 4-bytes, and 64-bytes is the max supported by ESP8266 SPI
  static uint32_t data_chunk[HW_DATA_CHUNK_SIZE / 4];
  memset(data_chunk, 0x00, sizeof(data_chunk));
  static struct hw_message *message_buffer;
  for (;;) {
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(__ESP_FILE__, "Task resume");
    while (uxQueueMessagesWaiting(http_to_spi_queue_handle) > 0) {
      memset(&message_buffer, 0x00, sizeof(struct hw_message *));
      xQueueReceive(http_to_spi_queue_handle, &message_buffer, portMAX_DELAY);
      if (message_buffer->metadata->type == BASIC_PATTERN_DATA) {
        #if CONFIG_HW_ENABLE_BASIC_PATTERN
          ulTaskNotifyTake(pdTRUE, 0);
          // Below: number of chunks to send, in case the relevant struct is larger than 64-bytes
          static uint32_t data_chunk_count_metadata = HW_DATA_CHUNK_COUNT(struct hw_message_metadata);
          static uint32_t data_chunk_count_pattern = HW_DATA_CHUNK_COUNT(struct hw_pattern_data);
          spi_master_send_length((data_chunk_count_metadata + data_chunk_count_pattern) * sizeof(data_chunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          // Send metadata, letting the NeoPixel device what else it's about to receive
          for (uint32_t data_index_metadata = 0; data_index_metadata < data_chunk_count_metadata;
            ++data_index_metadata) {
            // Explanation of all this is further down, as it's almost identical
            memcpy(data_chunk, message_buffer->metadata + (data_index_metadata * sizeof(data_chunk)),
              (size_t) fminf(sizeof(data_chunk), sizeof(struct hw_message_metadata)) -
              (data_index_metadata * sizeof(data_chunk)));
            spi_master_transmit(SPI_WRITE, data_chunk);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(data_chunk, 0x00, sizeof(data_chunk));
          }
          // Send the actual pattern data
          for (uint32_t data_index_pattern = 0; data_index_pattern < data_chunk_count_pattern; ++data_index_pattern) {
            // Copy the minimum necessary data to data_chunk using fminf(),
            // important both when the pattern data struct is greater than or less than 64-bytes
            memcpy(data_chunk, message_buffer->pattern_data + (data_index_pattern * sizeof(data_chunk)),
              (size_t) fminf(sizeof(data_chunk), sizeof(struct hw_pattern_data)) -
              (data_index_pattern * sizeof(data_chunk)));
            spi_master_transmit(SPI_WRITE, data_chunk); // Write the actual data in small snippets
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(data_chunk, 0x00, sizeof(data_chunk)); // Clear data_chunk
          }
          spi_master_send_length((uint32_t) 0); // Clear the status register on the other device
          print_pattern_data(message_buffer->pattern_data);
          xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END);
          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
        #endif
      } else if (message_buffer->metadata->type == DYNAMIC_PATTERN_DATA) {
        #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
          // TODO: actually implement this... eventually
          // Design: send header telling NeoPixel that it's about to get some dynamic pattern data,
          // then read from the stream buffer until it's empty
        #else
          // Do nothing for now I guess, I didn't really think this far ahead
        #endif
      } else {
        // Something has gone wrong, but I don't know how to fix it at the moment
      }
    } // end while loop
  }
}

#ifdef __cplusplus
}
#endif
