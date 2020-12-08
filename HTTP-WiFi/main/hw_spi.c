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
//static void print_pattern_data(struct hw_pattern_data *pattern_data);

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
//static void print_pattern_data(struct hw_pattern_data *pattern_data) {
//  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
//  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
//  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
//}
void hw_setup_spi(void) {
  gpio_config_t io_config = HW_GPIO_CONFIG_DEFAULT(CONFIG_HW_SPI_HANDSHAKE);
  gpio_config(&io_config);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(CONFIG_HW_SPI_HANDSHAKE, gpio_isr_handler, NULL);
  spi_config_t spi_config = HW_SPI_CONFIG_DEFAULT(CONFIG_HW_SPI_CLK_DIV);
  spi_init(HSPI_HOST, &spi_config);
}
void IRAM_ATTR hw_spi_master_write_task(void *arg) {
//  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  // Divide by four because a uint32_t is 4-bytes, and 64-bytes is the max supported by ESP8266 SPI
  static uint32_t data_chunk[HW_DATA_CHUNK_SIZE / 4];
  memset(data_chunk, 0x00, sizeof(data_chunk));
  static struct hw_message *message_buffer;
  const uint32_t data_chunk_count_metadata = HW_DATA_CHUNK_COUNT(sizeof(struct hw_message_metadata));
  for (;;) {
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(__ESP_FILE__, "Task resume");
    while (uxQueueMessagesWaiting(http_to_spi_queue_handle) > 0) {
      memset(&message_buffer, 0x00, sizeof(struct hw_message *));
      xQueueReceive(http_to_spi_queue_handle, &message_buffer, portMAX_DELAY);
//      ESP_EARLY_LOGI(__ESP_FILE__, "type %"PRIu32, message_buffer->metadata->type);
      if (message_buffer->metadata->type == STATIC_PATTERN_DATA) {
        // Classic way of transferring data, uncompressed with only one static pattern per transmission
        #if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
          ulTaskNotifyTake(pdTRUE, 0);
          // Below: number of chunks to send, in case the relevant struct is larger than 64-bytes
          const uint32_t data_chunk_count_pattern = HW_DATA_CHUNK_COUNT(sizeof(struct hw_pattern_data));
          spi_master_send_length((data_chunk_count_metadata + data_chunk_count_pattern) * sizeof(data_chunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          // Send metadata, letting the NeoPixel device what else it's about to receive
          for (uint32_t data_index_metadata = 0; data_index_metadata < data_chunk_count_metadata;
            ++data_index_metadata) {
            // Explanation of all this is further down, as it's almost identical
            memcpy(data_chunk, (void *) (message_buffer->metadata + (data_index_metadata * sizeof(data_chunk))),
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
            memcpy(data_chunk, (void *) (message_buffer->pattern_data + (data_index_pattern * sizeof(data_chunk))),
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
        // Compressed transfer, hopefully more efficient
        #if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
          ulTaskNotifyTake(pdTRUE, 0);
          spi_master_send_length(HW_DATA_CHUNK_COUNT((sizeof(struct hw_pattern_data)) +
            (message_buffer->pattern_data_array_length * sizeof(struct hw_pattern_data))) * sizeof(data_chunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          uint32_t data_chunk_position = 0;
          // Send metadata or otherwise transfer it into data_chunk
          for (uint32_t data_chunk_index = 0; data_chunk_index < data_chunk_count_metadata; ++data_chunk_index) {
            // Get the minimum possible amount of data to transmit
            uint32_t data_transmission_amount = (uint32_t) fminf(sizeof(data_chunk),
              (sizeof(struct hw_message_metadata)) - (data_chunk_index * sizeof(data_chunk)));
            // Copy the struct into data_chunk, using data_chunk_index as an offset on metadata
            memcpy(data_chunk, (void *) ((message_buffer->metadata) + (data_chunk_index * sizeof(data_chunk))),
              data_transmission_amount);
            data_chunk_position += data_transmission_amount;
            // If data_chunk is full, send it and subsequently reset the position,
            // otherwise continue onto the next the loop
            if (data_transmission_amount == sizeof(data_chunk)) {
              spi_master_transmit(SPI_WRITE, data_chunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(data_chunk, 0x00, sizeof(data_chunk));
              data_chunk_position = 0;
            }
          }
          // Send pattern data, pretty much the exact same thing
          const uint32_t data_chunk_count_pattern_array =
            HW_DATA_CHUNK_COUNT(message_buffer->pattern_data_array_length * sizeof(struct hw_pattern_data));
          for (uint32_t data_chunk_index = 0; data_chunk_index < data_chunk_count_pattern_array; ++data_chunk_index) {
            uint32_t data_transmission_amount = (uint32_t) fminf(sizeof(data_chunk),
              (message_buffer->pattern_data_array_length * sizeof(struct hw_pattern_data)) - (data_chunk_index *
               sizeof(data_chunk)));
            memcpy(data_chunk, (void *) ((message_buffer->pattern_data_array) +
              (data_chunk_index * sizeof(data_chunk))), data_transmission_amount);
            data_chunk_position += data_transmission_amount;
            if (data_transmission_amount == sizeof(data_chunk)) {
              spi_master_transmit(SPI_WRITE, data_chunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(data_chunk, 0x00, sizeof(data_chunk));
              data_chunk_position = 0;
            }
          }
          // New idea: iterate over the array by each element. Will need a sub-loop within it for it to work,
          // utilizing another position variable sized to sizeof(pattern_data)
          for (uint32_t pda_index = 0; pda_index < message_buffer->pattern_data_array_length; ++pda_index) {

          }
          // In case there's a remaining chunk that isn't full, send it
          if (data_chunk_position > 0) {
            spi_master_transmit(SPI_WRITE, data_chunk);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(data_chunk, 0x00, sizeof(data_chunk));
            data_chunk_position = 0;
          }
          spi_master_send_length((uint32_t) 0);
          for (uint32_t pda_index = 0; pda_index < message_buffer->pattern_data_array_length; ++pda_index) {
            print_pattern_data(message_buffer->pattern_data_array[pda_index]);
          }
          xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END);
          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
        #endif
      } else if (message_buffer->metadata->type == DYNAMIC_PATTERN_DATA) {
        #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
          ulTaskNotifyTake(pdTRUE, 0);
          const uint32_t ulInitialStreamBufferBytes =
            xStreamBufferBytesAvailable(*message_buffer->xStreamBufferHandle);
          uint32_t ulDataChunkCountStreamBuffer = HW_DATA_CHUNK_COUNT(ulInitialStreamBufferBytes);
//          ESP_LOGI(__ESP_FILE__, "length: %"PRIu32, HW_DATA_CHUNK_COUNT(sizeof(struct hw_pattern_data) +
//            ulInitialStreamBufferBytes) * sizeof(data_chunk));
          spi_master_send_length(HW_DATA_CHUNK_COUNT(sizeof(struct hw_pattern_data) + ulInitialStreamBufferBytes) *
            sizeof(data_chunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          // Hold the current index within data_chunk of where the data stops; rolls over to zero every 64-bytes
          uint32_t ulDataChunkPosition = 0;
          // Load metadata into data_chunk, send it if need be and continue loading
          for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < data_chunk_count_metadata; ++ulDataChunkIndex) {
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkIndex: %"PRIu32, ulDataChunkIndex);
            // Get the minimum possible amount of data to transmit
            uint32_t ulDataTransmitAmount = (uint32_t) fminf(sizeof(data_chunk),
              (sizeof(struct hw_message_metadata)) - (ulDataChunkIndex * sizeof(data_chunk)));
            // Copy metadata to the data_chunk, shifting over by 64-bytes every loop if necessary
            memcpy(data_chunk, (void *) ((message_buffer->metadata) + (ulDataChunkIndex * sizeof(data_chunk))),
              ulDataTransmitAmount);
            ulDataChunkPosition += (ulDataTransmitAmount - ulDataChunkPosition);
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkPosition: %"PRIu32, ulDataChunkPosition);
            if (ulDataTransmitAmount == sizeof(data_chunk)) {
              spi_master_transmit(SPI_WRITE, data_chunk);
//              ESP_LOGI(__ESP_FILE__, "A metadata data chunk has been sent");
//              vTaskDelay(1000 / portTICK_PERIOD_MS);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(data_chunk, 0x00, sizeof(data_chunk));
              ulDataChunkPosition = 0;
            }
          }
          uint32_t ulRemainingStreamBufferBytes = ulInitialStreamBufferBytes;
          // Send the dynamic pattern data itself
          for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountStreamBuffer; ++ulDataChunkIndex) {
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkIndex: %"PRIu32, ulDataChunkIndex);
            // Minimum data to transmit; will max out at sizeof(data_chunk)
            uint32_t ulDataTransmitAmount = (uint32_t) fminf(sizeof(data_chunk),
              ulRemainingStreamBufferBytes);
            // Fill data_chunk
            xStreamBufferReceive(*message_buffer->xStreamBufferHandle, (void *) (data_chunk + ulDataChunkPosition),
              ulDataTransmitAmount - ulDataChunkPosition, portMAX_DELAY);
            ulDataChunkPosition += (ulDataTransmitAmount - ulDataChunkPosition);
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkPosition: %"PRIu32, ulDataChunkPosition);
            // If data_chunk is all the way full, send it
            if (ulDataTransmitAmount == sizeof(data_chunk)) {
//              ESP_LOGI(__ESP_FILE__, "type: %d", ((struct hw_message *) data_chunk)->metadata->type);
//              ESP_LOGI(__ESP_FILE__, (char *) data_chunk);
//              for (int x = 0; x < 64; ++x) {
//                printf((char *) &(data_chunk[x]));
//              }
//              printf("\n");
              spi_master_transmit(SPI_WRITE, data_chunk);
//              ESP_LOGI(__ESP_FILE__, "A dynamic data chunk has been sent");
//              vTaskDelay(1000 / portTICK_PERIOD_MS);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(data_chunk, 0x00, sizeof(data_chunk));
              ulDataChunkPosition = 0;
              ulRemainingStreamBufferBytes = xStreamBufferBytesAvailable(*message_buffer->xStreamBufferHandle);
            }
          }
          // Send the straggler data_chunk, if it exists
          if (ulDataChunkPosition > 0) {
            spi_master_transmit(SPI_WRITE, data_chunk);
//            ESP_LOGI(__ESP_FILE__, "A straggler data chunk has been sent");
//            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(data_chunk, 0x00, sizeof(data_chunk));
          }
          // End-of-transmission cleanup
          spi_master_send_length((uint32_t) 0);
          xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END);
          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
          vTaskDelay(1000 / portTICK_PERIOD_MS);
        #endif
      } else {
        // Something has gone wrong, but I don't know how to fix it at the moment
      }
    } // while loop
  }
}

#ifdef __cplusplus
}
#endif
