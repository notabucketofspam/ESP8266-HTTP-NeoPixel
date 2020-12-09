/*
 * HW = HTTP-WiFi
 * A lot of this is stolen from various examples
 */
#include <stdio.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "hw_def.h"
#include "hw_spi.h"
#include "hw_wifi.h"
#include "hw_http.h"
#include "hw_test.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Task handles
 */
TaskHandle_t xSpiWriteTaskHandle = NULL;
TaskHandle_t xHttpServerTaskHandle = NULL;
TaskHandle_t xWifiTaskHandle = NULL;
/*
 * Other handles
 */
QueueHandle_t xHttpToSpiQueueHandle = NULL;
EventGroupHandle_t xHttpAndSpiEventGroupHandle = NULL;
StreamBufferHandle_t xHttpToSpiStreamBufferHandle = NULL;

/*
 * This is all the task creation, peripheral initialization, etc
 */
void app_main(void) {
  xHttpToSpiQueueHandle = xQueueCreate(CONFIG_HW_QUEUE_SIZE, sizeof(struct xHwMessage *));
  xHttpAndSpiEventGroupHandle = xEventGroupCreate();
  #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
    xHttpToSpiStreamBufferHandle = xStreamBufferCreate(CONFIG_HW_STREAM_BUFFER_SIZE, HW_DATA_CHUNK_SIZE);
  #endif
  vHwSetupSpi();
  xTaskCreate(vHwSpiMasterWriteTask, "vHwSpiMasterWriteTask", 2048, NULL, 4, &xSpiWriteTaskHandle);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
//  xTaskCreate(test_spi_pattern_task, "test_spi_pattern_task", 2048, NULL, 3, &xHttpServerTaskHandle);
//  xTaskCreate(test_spi_pattern_array_task, "test_spi_pattern_array_task", 2048, NULL, 3, &xHttpServerTaskHandle);
//  xTaskCreate(vHwTestSpiDynamicTask, "vHwTestSpiDynamicTask", 2048, NULL, 3, &xHttpServerTaskHandle);
  xTaskCreate(vHwTestSpiDynamicTaskTwo, "vHwTestSpiDynamicTaskTwo", 2048, NULL, 3, &xHttpServerTaskHandle);
}
