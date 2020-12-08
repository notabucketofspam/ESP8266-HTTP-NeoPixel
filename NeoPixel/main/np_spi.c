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
 * Number of data chunks received in the transmission
 */
static uint32_t ulInitialDataReceiveBytes = 0;
/*
 * Determines whether or not np_setup_spi has finished
 */
static BaseType_t xNpSetupSpiComplete = pdFALSE;
/*
 * Internal Stream Buffer for receiving transmissions during the ISR
 */
static StreamBufferHandle_t xSpiInternalStreamBufferHandle;
/*
 * Callback which pretty much only resumes the SPI read task
 * Just like everything else, it's mostly stolen from the Espressif SPI examples
 */
static void IRAM_ATTR spi_event_callback(int event, void *arg);
/*
 * A debug function for printing the contents of a pattern data struct
 */
//static void print_pattern_data(struct np_pattern_data *pattern_data);

static void IRAM_ATTR spi_event_callback(int event, void *arg) {
//    ESP_EARLY_LOGI(__ESP_FILE__, "Event: %d", event);
  if (xNpSetupSpiComplete != pdTRUE) {
    return;
  }
  uint32_t transmit_done;
  uint32_t status;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  static uint32_t data_chunk[NP_DATA_CHUNK_SIZE / 4];
  memset(data_chunk, 0x00, sizeof(data_chunk));
  if (event == SPI_TRANS_DONE_EVENT) {
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
    transmit_done = *(uint32_t *) arg; // Convert arg to a uint32_t pointer and subsequently dereference it
    if (transmit_done & SPI_SLV_WR_STA_DONE) {
      spi_slave_get_status(HSPI_HOST, &status);
//      ESP_EARLY_LOGI(__ESP_FILE__, "status: %"PRIu32, status);
      if (status == 0x00) {
        // Resume the SPI read task only after all the data chunks have been sent
        vTaskNotifyGiveFromISR(spi_read_task_handle, &xHigherPriorityTaskWoken);
      } else {
        // This will be used later by the dynamic pattern section
        ulInitialDataReceiveBytes = status;
      }
    }
    if (transmit_done & SPI_SLV_WR_BUF_DONE) {
      for (uint32_t x = 0; x < sizeof(data_chunk) / 4; ++x) {
        data_chunk[x] = SPI1.data_buf[x];
      }
//      ESP_EARLY_LOGI(__ESP_FILE__, "%s", (char *) data_chunk);
      xStreamBufferSendFromISR(xSpiInternalStreamBufferHandle, data_chunk, NP_DATA_CHUNK_SIZE,
        &xHigherPriorityTaskWoken);
    }
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 0);
    if (xHigherPriorityTaskWoken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  } // if (event...)
}
//static void print_pattern_data(struct np_pattern_data *pattern_data) {
//  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
//  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
//  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
//}
void np_setup_spi(void) {
  gpio_config_t io_conf = NP_GPIO_CONFIG_DEFAULT(CONFIG_NP_SPI_HANDSHAKE);
  gpio_config(&io_conf);
  gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
  spi_config_t spi_config = NP_SPI_CONFIG_DEFAULT(spi_event_callback);
  spi_init(HSPI_HOST, &spi_config);
  uint32_t ulRxBufferSizePattern = 0;
  #if CONFIG_NP_ENABLE_STATIC_PATTERN
    ulRxBufferSizePattern = NP_DATA_CHUNK_COUNT(sizeof(struct np_message_metadata) +
      (CONFIG_NP_STATIC_PATTERN_ARRAY_SIZE * sizeof(struct np_pattern_data))) * NP_DATA_CHUNK_SIZE;
  #endif
  uint32_t ulRxBufferSizeDynamic = 0;
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    ulRxBufferSizeDynamic = (NP_DATA_CHUNK_COUNT(sizeof(struct np_message_metadata)) * NP_DATA_CHUNK_SIZE) +
      CONFIG_NP_STREAM_BUFFER_SIZE;
  #endif
  xSpiInternalStreamBufferHandle = xStreamBufferCreate((uint32_t)
    fmaxf(ulRxBufferSizePattern, ulRxBufferSizeDynamic), NP_DATA_CHUNK_SIZE);
  xNpSetupSpiComplete = pdTRUE;
}
void IRAM_ATTR np_spi_slave_read_task(void *arg) {
  // refer to hw_spi.h for some more documentation
  static uint32_t data_chunk[NP_DATA_CHUNK_SIZE / 4];
  memset(data_chunk, 0x00, sizeof(data_chunk));
  struct np_message_metadata *pxMetadataBuffer = malloc(sizeof(struct np_message_metadata));
  // Minimum number of data chunks to contain xMetadataBuffer
  const uint32_t ulDataChunkCountMetadata = NP_DATA_CHUNK_COUNT(sizeof(struct np_message_metadata));
  for (;;) {
    memset(pxMetadataBuffer, 0x00, sizeof(struct np_message_metadata));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block until more SPI data chunks are received
    ESP_LOGI(__ESP_FILE__, "Task resume");
    uint32_t ulRemainingDataReceiveBytes = ulInitialDataReceiveBytes;
    // Read just the metadata first, then determine what to do with it afterwards based on the message type
    for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountMetadata; ++ulDataChunkIndex) {
      // Copy the minimum amount of data necessary to fill xMetadataBuffer. Important because we don't want to
      // over-read from the received data and potentially delete some of it
      uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(data_chunk),
        (sizeof(struct np_message_metadata)) - (ulDataChunkIndex * sizeof(data_chunk)));
//      ESP_LOGI(__ESP_FILE__, "ulDataReceiveBytes: %"PRIu32, ulDataReceiveBytes);
      xStreamBufferReceive(xSpiInternalStreamBufferHandle, (void *) data_chunk, ulDataReceiveBytes, portMAX_DELAY);
      memcpy((void *) ((pxMetadataBuffer) + (ulDataChunkIndex * sizeof(data_chunk))), data_chunk,
        ulDataReceiveBytes);
//      ESP_LOGI(__ESP_FILE__, "sizeof: %d", sizeof(struct np_message_metadata));
//      ESP_LOGI(__ESP_FILE__, "name: %s", (char *) *(data_chunk + 0));
//      ESP_LOGI(__ESP_FILE__, "type: %"PRIu32, ((struct np_message *) data_chunk)->metadata->type);
      ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
      if (ulDataReceiveBytes == sizeof(data_chunk)) {
        memset(data_chunk, 0x00, sizeof(data_chunk));
      }
    }
//    ESP_LOGI(__ESP_FILE__, "type: %d", ((struct np_message *) data_chunk)->metadata->type);
//    ESP_LOGI(__ESP_FILE__, "name %s", pxMetadataBuffer->name);
//    ESP_LOGI(__ESP_FILE__, "type %"PRIu32, pxMetadataBuffer->type);
    if (pxMetadataBuffer->type == STATIC_PATTERN_DATA) {
      // Classic way of receiving messages
      #if CONFIG_NP_ENABLE_STATIC_PATTERN & 0
        struct np_pattern_data pattern_data_buffer;
        memset(&pattern_data_buffer, 0x00, sizeof(struct np_pattern_data));
        // Read the pattern data into the message buffer
        for (uint32_t data_index_pattern = 0; data_index_pattern < NP_DATA_CHUNK_COUNT(sizeof(struct np_pattern_data));
          ++data_index_pattern) {
//          hspi_slave_logic_read_data((uint8_t *) data_chunk, sizeof(data_chunk), portMAX_DELAY);
          memcpy((uint8_t *) &pattern_data_buffer + (data_index_pattern * sizeof(data_chunk)), data_chunk,
            (size_t) fminf(sizeof(data_chunk), sizeof(struct np_pattern_data)) -
            (data_index_pattern * sizeof(data_chunk)));
          memset(data_chunk, 0x00, sizeof(data_chunk));
        }
        // Just debug the data for now, worry about changing the actual strip and stuff later
        print_pattern_data(&pattern_data_buffer);
        ESP_LOGI(__ESP_FILE__, "End transmission");
      #endif
      // Revised way, with compressed messages and a pattern array
      #if CONFIG_NP_ENABLE_STATIC_PATTERN & 0

      #endif
    } else if (pxMetadataBuffer->type == DYNAMIC_PATTERN_DATA) {
      #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
        const uint32_t ulRemainingDataChunkCount = NP_DATA_CHUNK_COUNT(ulRemainingDataReceiveBytes);
        for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulRemainingDataChunkCount; ++ulDataChunkIndex) {
//          ESP_LOGI(__ESP_FILE__, "Dynamic %"PRIu32, ulDataChunkIndex);
          uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(data_chunk), ulRemainingDataReceiveBytes);
          xStreamBufferReceive(xSpiInternalStreamBufferHandle, (void *) data_chunk, ulDataReceiveBytes, portMAX_DELAY);
//          xStreamBufferSend(xSpiToAnpStreamBufferHandle, (void *) data_chunk, ulDataReceiveBytes, portMAX_DELAY);
          ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
          memset(data_chunk, 0x00, sizeof(data_chunk));
        }
        ESP_LOGI(__ESP_FILE__, "End transmission");
//        vTaskDelay(10000 / portTICK_PERIOD_MS);
      #endif
    } else {
      // Things have gone wrong again for some reason
    }
  }
}

#ifdef __cplusplus
}
#endif
