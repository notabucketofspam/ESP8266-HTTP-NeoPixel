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
 * Determines whether or not vNpSetupSpi has finished
 */
static BaseType_t xNpSetupSpiComplete = pdFALSE;
/*
 * Stream Buffer for receiving transmissions during the ISR
 */
static StreamBufferHandle_t xSpiInternalStreamBufferHandle = NULL;
/*
 * Callback which pretty much only resumes the SPI read task
 * Just like everything else, it's mostly stolen from the Espressif SPI examples
 */
static void IRAM_ATTR spi_event_callback(int event, void *arg);
/*
 * A debug function for printing the contents of a pattern data struct
 */
//static void print_pattern_data(struct xNpStaticData *pattern_data);

static void IRAM_ATTR spi_event_callback(int event, void *arg) {
  if (xNpSetupSpiComplete != pdTRUE) {
    return;
  }
  uint32_t ulTransmissionDone;
  uint32_t ulStatus;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  static uint32_t pulDataChunk[NP_DATA_CHUNK_SIZE / 4];
  memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
  if (event == SPI_TRANS_DONE_EVENT) {
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
    ulTransmissionDone = *(uint32_t *) arg; // Convert arg to a uint32_t pointer and subsequently dereference it
    if (ulTransmissionDone & SPI_SLV_WR_STA_DONE) {
      spi_slave_get_status(HSPI_HOST, &ulStatus);
      if (ulStatus == 0x00) {
        // Resume the SPI read task only after all the data chunks have been sent
        vTaskNotifyGiveFromISR(xSpiReadTaskHandle, &xHigherPriorityTaskWoken);
      } else {
        // This will be used later by the dynamic pattern section
        ulInitialDataReceiveBytes = ulStatus;
      }
    }
    if (ulTransmissionDone & SPI_SLV_WR_BUF_DONE) {
      for (uint32_t x = 0; x < sizeof(pulDataChunk) / 4; ++x) {
        pulDataChunk[x] = SPI1.data_buf[x];
      }
//      ESP_EARLY_LOGI(__ESP_FILE__, "%s", (char *) pulDataChunk);
      xStreamBufferSendFromISR(xSpiInternalStreamBufferHandle, pulDataChunk, NP_DATA_CHUNK_SIZE,
        &xHigherPriorityTaskWoken);
    }
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 0);
    if (xHigherPriorityTaskWoken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  } // if (event...)
}
//static void print_pattern_data(struct xNpStaticData *pattern_data) {
//  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
//  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
//  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
//}
void vNpSetupSpi(void) {
  gpio_config_t io_conf = NP_GPIO_CONFIG_DEFAULT(CONFIG_NP_SPI_HANDSHAKE);
  gpio_config(&io_conf);
  gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
  spi_config_t spi_config = NP_SPI_CONFIG_DEFAULT(spi_event_callback);
  spi_init(HSPI_HOST, &spi_config);
  uint32_t ulRxBufferSizePattern = 0;
  #if CONFIG_NP_ENABLE_STATIC_PATTERN
    ulRxBufferSizePattern = NP_DATA_CHUNK_COUNT(sizeof(struct xNpMessageMetadata) +
      (CONFIG_NP_STATIC_PATTERN_ARRAY_SIZE * sizeof(struct xNpStaticData))) * NP_DATA_CHUNK_SIZE;
  #endif
  uint32_t ulRxBufferSizeDynamic = 0;
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    ulRxBufferSizeDynamic = (NP_DATA_CHUNK_COUNT(sizeof(struct xNpMessageMetadata)) * NP_DATA_CHUNK_SIZE) +
      CONFIG_NP_STREAM_BUFFER_SIZE;
  #endif
  xSpiInternalStreamBufferHandle = xStreamBufferCreate((uint32_t)
    fmaxf(ulRxBufferSizePattern, ulRxBufferSizeDynamic), NP_DATA_CHUNK_SIZE);
  xNpSetupSpiComplete = pdTRUE;
}
void IRAM_ATTR vNpSpiSlaveReadTask(void *arg) {
  // refer to hw_spi.h for some more documentation
  static uint32_t pulDataChunk[NP_DATA_CHUNK_SIZE / 4];
  memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
  // This should've been static / not a pointer, but that seemed to break things, so here we are
  struct xNpMessageMetadata *pxMetadataBuffer = malloc(sizeof(struct xNpMessageMetadata));
  // Minimum number of data chunks to contain xMetadataBuffer
  const uint32_t ulDataChunkCountMetadata = NP_DATA_CHUNK_COUNT(sizeof(struct xNpMessageMetadata));
  for (;;) {
    memset(pxMetadataBuffer, 0x00, sizeof(struct xNpMessageMetadata));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block until all of the SPI data chunks are received
    ESP_LOGI(__ESP_FILE__, "Task resume");
    uint32_t ulRemainingDataReceiveBytes = ulInitialDataReceiveBytes; // Keep track of how much data we have left
    // Read just the metadata first, then determine what to do with it afterwards based on the message xType
    for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountMetadata; ++ulDataChunkIndex) {
      // Copy the minimum amount of data necessary to fill pxMetadataBuffer. Important because we don't want to
      // over-read from the received data and potentially delete some of it
      uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(pulDataChunk),
        (sizeof(struct xNpMessageMetadata)) - (ulDataChunkIndex * sizeof(pulDataChunk)));
      xStreamBufferReceive(xSpiInternalStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes, portMAX_DELAY);
      memcpy((void *) ((pxMetadataBuffer) + (ulDataChunkIndex * sizeof(pulDataChunk))), pulDataChunk,
        ulDataReceiveBytes);
      ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
      if (ulDataReceiveBytes == sizeof(pulDataChunk)) {
        memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
      }
    }
    if (pxMetadataBuffer->xType == STATIC_PATTERN_DATA) {
      #if CONFIG_NP_ENABLE_STATIC_PATTERN & 0
        // Classic way of receiving messages
        struct xNpStaticData pattern_data_buffer;
        memset(&pattern_data_buffer, 0x00, sizeof(struct xNpStaticData));
        // Read the pattern data into the message buffer
        for (uint32_t data_index_pattern = 0; data_index_pattern < NP_DATA_CHUNK_COUNT(sizeof(struct xNpStaticData));
          ++data_index_pattern) {
          memcpy((uint8_t *) &pattern_data_buffer + (data_index_pattern * sizeof(pulDataChunk)), pulDataChunk,
            (size_t) fminf(sizeof(pulDataChunk), sizeof(struct xNpStaticData)) -
            (data_index_pattern * sizeof(pulDataChunk)));
          memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
        }
        // Just debug the data for now, worry about changing the actual strip and stuff later
        print_pattern_data(&pattern_data_buffer);
        ESP_LOGI(__ESP_FILE__, "End transmission");
      #endif
      #if CONFIG_NP_ENABLE_STATIC_PATTERN & 0
        // Revised way, with compressed messages and a pattern array
      #endif
    } else if (pxMetadataBuffer->xType == DYNAMIC_PATTERN_DATA) {
      #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
        const uint32_t ulRemainingDataChunkCount = NP_DATA_CHUNK_COUNT(ulRemainingDataReceiveBytes);
        for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulRemainingDataChunkCount; ++ulDataChunkIndex) {
          uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(pulDataChunk), ulRemainingDataReceiveBytes);
          // Snag data from the SPI callback stream buffer...
          xStreamBufferReceive(xSpiInternalStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes, portMAX_DELAY);
          // ... and shuttle it over to the ANP stream buffer for processing
//          xStreamBufferSend(xSpiToAnpStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes, portMAX_DELAY);
          ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
          // Clear pulDataChunk no matter what ulDataReceiveBytes was, since (unlike most other transmission functions)
          // there will never be a straggler data chunk after this point that would need to be dealt with
          memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
        }
        ESP_LOGI(__ESP_FILE__, "End transmission");
      #endif
    } else {
      // Things have gone wrong again for some reason
    }
  }
}

#ifdef __cplusplus
}
#endif
