/*
 * NP = NeoPixel
 * Like the HTTP-WiFi counterpart, most of this is stolen from examples
 */
#include <stdio.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_log.h"

#include "np_def.h"
#include "np_spi.h"
#include "np_anp.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Task handles
 */
TaskHandle_t xSpiReadTaskHandle = NULL;
//TaskHandle_t xAnpStripTaskHandle = NULL;
TaskHandle_t xDynamicDataTaskHandle = NULL;
/*
 * Other handles
 */
QueueHandle_t xSpiToAnpQueueHandle = NULL;
EventGroupHandle_t xSpiAndAnpEventGroupHandle = NULL;
StreamBufferHandle_t xSpiStreamBufferHandle = NULL;

// TODO: make sure that anp_pinMode() doesn't interfere with the handshake pin

void app_main(void) {
  xSpiToAnpQueueHandle = xQueueCreate(CONFIG_NP_QUEUE_SIZE, sizeof(struct xNpMessage *));
  xSpiAndAnpEventGroupHandle = xEventGroupCreate();
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    uint32_t ulStreamBufferSize = NP_DATA_CHUNK_COUNT(sizeof(struct xNpMessageMetadata) +
      (CONFIG_NP_NEOPIXEL_COUNT * sizeof(struct xNpDynamicData))) * NP_DATA_CHUNK_SIZE;
//    ESP_LOGI(__ESP_FILE__, "ulStreamBufferSize %u", ulStreamBufferSize);
    xSpiStreamBufferHandle = xStreamBufferCreate(ulStreamBufferSize, NP_DATA_CHUNK_SIZE);
  #endif
  xTaskCreate(vDynamicDataProcessTask, "vDynamicDataProcessTask", 2048, NULL, 4, &xDynamicDataTaskHandle);
  vNpSetupAnp();
  xTaskCreate(vNpSpiSlaveReadTask, "xNpSpiSlaveReadTask", 2048, NULL, 4, &xSpiReadTaskHandle);
  // SPI setup has to happen after the task has started because the event
  // callback makes use of task notifications
  vNpSetupSpi();
//  xTaskCreate(vNpAnpStripUpdateTask, "vNpAnpStripUpdateTask", 2048, NULL, 5, &xAnpStripTaskHandle);
}
