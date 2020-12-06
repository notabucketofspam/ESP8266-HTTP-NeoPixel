#include "np_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sets up the GPIO pin as an output with the pin number as a parameter
 */
#define NP_GPIO_CONFIG_DEFAULT(x) { \
  .pin_bit_mask = BIT(x), \
  .mode = GPIO_MODE_OUTPUT, \
  .pull_up_en = GPIO_PULLUP_DISABLE, \
  .pull_down_en = GPIO_PULLDOWN_DISABLE, \
  .intr_type = GPIO_INTR_DISABLE \
}
/*
 * Sets up SPI as master with clk_div selected as an input
 */
#define NP_SPI_CONFIG_DEFAULT(x) { \
  .interface.val = SPI_DEFAULT_INTERFACE, \
  .intr_enable.val = SPI_SLAVE_DEFAULT_INTR_ENABLE, \
  .mode = SPI_SLAVE_MODE, \
  .event_cb = x \
}
/*
 * Max size of a single SPI transfer on the ESP8266
 */
#define NP_DATA_CHUNK_SIZE (64)
/*
 * Useful for getting the minimum number of data chunks needed to receive a struct, for example.
 * Data chunks are always sixteen uint32_t's, thus 64-bytes long
 */
#define NP_DATA_CHUNK_COUNT(x) ((sizeof(x) + (NP_DATA_CHUNK_SIZE - 1)) / NP_DATA_CHUNK_SIZE)
/*
 * Number of data chunks received
 */
static uint32_t data_chunk_count_length = 0;
/*
 * Callback which pretty much only resumes the SPI read task
 * Just like everything else, it's mostly stolen from the Espressif SPI examples
 */
static void IRAM_ATTR spi_event_callback(int event, void *arg);
/*
 * A debug function for printing the contents of a pattern data struct
 */
static void print_pattern_data(struct np_pattern_data *pattern_data);

static void IRAM_ATTR spi_event_callback(int event, void *arg) {
  uint32_t transfer_done;
  uint32_t status;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (event == SPI_TRANS_DONE_EVENT) {
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
    transfer_done = *(uint32_t*) arg; // I don't know what this means
    if (transfer_done & SPI_SLV_WR_STA_DONE) {
      spi_slave_get_status(HSPI_HOST, &status);
      if (status == 0x00) {
        // Resume the SPI read task only after all the data chunks have been sent
        vTaskNotifyGiveFromISR(spi_read_task_handle, &xHigherPriorityTaskWoken);
      } else {
        // This will be used later by the dynamic pattern section
        data_chunk_count_length = status / NP_DATA_CHUNK_SIZE;
      }
    }
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 0);
    if (xHigherPriorityTaskWoken == pdTRUE) {
      taskYIELD();
    }
  } // end if event
}
static void print_pattern_data(struct np_pattern_data *pattern_data) {
  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
}
void np_setup_spi(void) {
  gpio_config_t io_conf = NP_GPIO_CONFIG_DEFAULT(CONFIG_NP_SPI_HANDSHAKE);
  gpio_config(&io_conf);
  gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
  spi_config_t spi_config = NP_SPI_CONFIG_DEFAULT(spi_event_callback);
  spi_init(HSPI_HOST, &spi_config);
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    // Number of data chunks by size of a data chunk plus stream buffer size
    uint32_t rx_buffer_size = (NP_DATA_CHUNK_COUNT(struct np_message_metadata) * NP_DATA_CHUNK_SIZE) +
      CONFIG_NP_STREAM_BUFFER_SIZE;
  #else
    // Same idea but with only the metadata plus pattern data
    uint32_t rx_buffer_size = (NP_DATA_CHUNK_COUNT(struct np_message_metadata) * NP_DATA_CHUNK_SIZE) +
      (NP_DATA_CHUNK_COUNT(struct np_pattern_data) * NP_DATA_CHUNK_SIZE);
  #endif
  // tx_buffer_size is 64-bytes because it's unused at the moment and I don't know what else to do with it
  hspi_slave_logic_device_create(CONFIG_NP_SPI_HANDSHAKE, NP_DATA_CHUNK_SIZE, NP_DATA_CHUNK_SIZE, rx_buffer_size);
}
void IRAM_ATTR np_spi_slave_read_task(void *arg) {
  // refer to hw_spi.h for some more documentation
  static uint32_t data_chunk[NP_DATA_CHUNK_SIZE / 4];
  memset(data_chunk, 0x00, sizeof(data_chunk));
  static struct np_message_metadata metadata_buffer;
  for (;;) {
    memset(&metadata_buffer, 0x00, sizeof(struct np_message_metadata));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block until more SPI data chunks are received
    ESP_LOGI(__ESP_FILE__, "Task resume");
    // Read just the metadata first, then determine what to do with it afterwards based on the message type
    for (uint32_t data_index_metadata = 0; data_index_metadata < NP_DATA_CHUNK_COUNT(struct np_message_metadata);
      ++data_index_metadata) {
      hspi_slave_logic_read_data((uint8_t *) data_chunk, sizeof(data_chunk), portMAX_DELAY);
      memcpy((uint8_t *) &metadata_buffer + (data_index_metadata * sizeof(data_chunk)), data_chunk,
        (size_t) fminf(sizeof(data_chunk), sizeof(struct np_message_metadata)) -
        (data_index_metadata * sizeof(data_chunk)));
      memset(data_chunk, 0x00, sizeof(data_chunk));
    }
    if (metadata_buffer.type == BASIC_PATTERN_DATA) {
      static struct np_pattern_data pattern_data_buffer;
      memset(&pattern_data_buffer, 0x00, sizeof(struct np_pattern_data));
      // Read the pattern data into the message buffer
      for (uint32_t data_index_pattern = 0; data_index_pattern < NP_DATA_CHUNK_COUNT(struct np_pattern_data);
        ++data_index_pattern) {
        hspi_slave_logic_read_data((uint8_t *) data_chunk, sizeof(data_chunk), portMAX_DELAY);
        memcpy((uint8_t *) &pattern_data_buffer + (data_index_pattern * sizeof(data_chunk)), data_chunk,
          (size_t) fminf(sizeof(data_chunk), sizeof(struct np_pattern_data)) -
          (data_index_pattern * sizeof(data_chunk)));
        memset(data_chunk, 0x00, sizeof(data_chunk));
      }
      // Just debug the data for now, worry about changing the actual strip and stuff later
      print_pattern_data(&pattern_data_buffer);
      ESP_LOGI(__ESP_FILE__, "End transmission");
    } else if (metadata_buffer.type == DYNAMIC_PATTERN_DATA) {
      #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
        // TODO: implement this as well
      #endif
    } else {
      // Things have gone wrong again for some reason
    }
  }
}

#ifdef __cplusplus
}
#endif
