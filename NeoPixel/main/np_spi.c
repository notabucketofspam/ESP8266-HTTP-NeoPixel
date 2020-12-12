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
 * Whether or not the device is ready for more data
 */
static BaseType_t xSpiReadyFlag = pdTRUE;
/*
 * Number of data chunks received in the transmission
 */
static uint32_t ulInitialDataReceiveBytes = 0;
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
  uint32_t ulTransmissionDone;
  uint32_t ulStatus;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  static uint32_t pulDataChunk[NP_DATA_CHUNK_SIZE / 4];
//  memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
  if (event == SPI_TRANS_DONE_EVENT) {
    gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
    ulTransmissionDone = *(uint32_t *) arg; // Convert arg to a uint32_t pointer and subsequently dereference it
    if (ulTransmissionDone & SPI_SLV_WR_STA_DONE) {
      spi_slave_get_status(HSPI_HOST, &ulStatus);
//      ESP_EARLY_LOGI(__ESP_FILE__, "status: %u", ulStatus);
      if (ulStatus == 0x00) {
        // Resume the SPI read task only after all the data chunks have been sent
        xSpiReadyFlag = xStreamBufferBytesAvailable(xSpiStreamBufferHandle) <
          ulInitialDataReceiveBytes ? pdTRUE : pdFALSE;
//        ESP_EARLY_LOGI(__ESP_FILE__, "avail at status 0: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
        vTaskNotifyGiveFromISR(xSpiReadTaskHandle, &xHigherPriorityTaskWoken);
      } else {
        // This will be used later by the dynamic pattern section
        ulInitialDataReceiveBytes = ulStatus;
        xSpiReadyFlag = pdTRUE;
      }
    }
    if (ulTransmissionDone & SPI_SLV_WR_BUF_DONE) {
      for (uint32_t ulPosition = 0; ulPosition < sizeof(pulDataChunk) / 4; ++ulPosition) {
        pulDataChunk[ulPosition] = SPI1.data_buf[ulPosition];
      }
      xStreamBufferSendFromISR(xSpiStreamBufferHandle, (void *) pulDataChunk, sizeof(pulDataChunk),
        &xHigherPriorityTaskWoken);
//      ESP_EARLY_LOGI(__ESP_FILE__, "avail at write: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
    }
//    ESP_EARLY_LOGI(__ESP_FILE__, "xSpiReadyFlag: %d", xSpiReadyFlag);
    if (xSpiReadyFlag == pdTRUE) {
      gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 0);
    }
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
}
void IRAM_ATTR vNpSpiSlaveReadTask(void *arg) {
  // refer to hw_spi.h for some more documentation
//  static uint32_t pulDataChunk[NP_DATA_CHUNK_SIZE / 4];
//  memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
  static struct xNpMessageMetadata xMetadataBuffer;
  // Minimum number of data chunks to contain xMetadataBuffer
  const uint32_t ulDataChunkCountMetadata = NP_DATA_CHUNK_COUNT(sizeof(struct xNpMessageMetadata));
  for (;;) {
    memset(&xMetadataBuffer, 0x00, sizeof(struct xNpMessageMetadata));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block until all of the SPI data chunks are received
//    ESP_LOGI(__ESP_FILE__, "Task resume");
//    uint32_t ulRemainingDataReceiveBytes = ulInitialDataReceiveBytes; // Keep track of how much data we have left
//    ESP_LOGI(__ESP_FILE__, "initial byte count: %u", ulInitialDataReceiveBytes);
    // Read just the metadata first, then determine what to do with it afterwards based on the message xType
    for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountMetadata; ++ulDataChunkIndex) {
//      ESP_LOGI(__ESP_FILE__, "ulDataChunkIndex: %u", ulDataChunkIndex);
      // Copy the minimum amount of data necessary to fill pxMetadataBuffer.
      // Important because we don't want to over-read from the received data
      // and potentially delete some of it
//      uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(pulDataChunk),
//        (sizeof(struct xNpMessageMetadata)) - (ulDataChunkIndex * sizeof(pulDataChunk)));
      uint32_t ulDataReceiveBytes = sizeof(struct xNpMessageMetadata) - (ulDataChunkIndex * NP_DATA_CHUNK_SIZE);
//      ESP_LOGI(__ESP_FILE__, "ulDataReceiveBytes: %u", ulDataReceiveBytes);
//      xStreamBufferReceive(xSpiStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes, portMAX_DELAY);
//      memcpy((void *) (&xMetadataBuffer) + (ulDataChunkIndex * sizeof(pulDataChunk)), pulDataChunk,
//        ulDataReceiveBytes);
//      xStreamBufferReceive(xSpiStreamBufferHandle, (void *) (&xMetadataBuffer) + (ulDataChunkIndex *
//        sizeof(pulDataChunk)), ulDataReceiveBytes, portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "bytes before receive: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
      xStreamBufferReceive(xSpiStreamBufferHandle, (void *) (&xMetadataBuffer) + (ulDataChunkIndex *
        NP_DATA_CHUNK_SIZE), ulDataReceiveBytes, portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "bytes after receive: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
//      ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
//      if (ulDataReceiveBytes == sizeof(pulDataChunk)) {
//        memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
//      }
    }
//    ESP_LOGI(__ESP_FILE__, "%s", xMetadataBuffer.pcName);
    if (xMetadataBuffer.xType == STATIC_PATTERN_DATA) {
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
    } else if (xMetadataBuffer.xType == DYNAMIC_PATTERN_DATA) {
      #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
        // Just give the stream buffer to ANP, let things get figured out there
        /*
        uint32_t ulRemainingDataChunkCount = NP_DATA_CHUNK_COUNT(ulRemainingDataReceiveBytes);
        for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulRemainingDataChunkCount; ++ulDataChunkIndex) {
          uint32_t ulDataReceiveBytes = (uint32_t) fminf(sizeof(pulDataChunk), ulRemainingDataReceiveBytes);
          // Snag data from the SPI callback stream buffer...
//          xStreamBufferReceive(xSpiInternalStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes,
//            portMAX_DELAY);
          // ... and shuttle it over to the ANP stream buffer for processing
          xStreamBufferSend(xSpiStreamBufferHandle, (void *) pulDataChunk, ulDataReceiveBytes, portMAX_DELAY);
          ulRemainingDataReceiveBytes -= ulDataReceiveBytes;
          // Clear pulDataChunk no matter what ulDataReceiveBytes was, since (unlike most other transmission functions)
          // there will never be a straggler data chunk after this point that would need to be dealt with
          memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
//          ESP_LOGI(__ESP_FILE__, "bytes left: %u", xStreamBufferBytesAvailable(xSpiInternalStreamBufferHandle));
//          ESP_LOGI(__ESP_FILE__, "counter   : %u", ulRemainingDataReceiveBytes);
        }
        */
//        ESP_LOGI(__ESP_FILE__, "bytes left: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
        xEventGroupSetBits(xSpiAndAnpEventGroupHandle, NP_BIT_ANP_DYNAMIC_START);
        xEventGroupWaitBits(xSpiAndAnpEventGroupHandle, NP_BIT_ANP_DYNAMIC_END, pdTRUE, pdTRUE, portMAX_DELAY);
        xSpiReadyFlag = pdTRUE;
        gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 0);
//        vTaskDelay(1);
        gpio_set_level(CONFIG_NP_SPI_HANDSHAKE, 1);
//        ESP_LOGI(__ESP_FILE__, "End transmission");
      #endif
    } else {
      // Things have gone wrong again for some reason
      ESP_LOGI(__ESP_FILE__, "Bad message!");
    }
  }
}

#ifdef __cplusplus
}
#endif
