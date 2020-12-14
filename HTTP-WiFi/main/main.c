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

#include "esp_log.h"
#include "esp_system.h"

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
  ESP_LOGI(__ESP_FILE__, "Reset reason: %s", pcHwResetReason());
  xHttpToSpiQueueHandle = xQueueCreate(CONFIG_HW_QUEUE_SIZE, sizeof(struct xHwMessage *));
  xHttpAndSpiEventGroupHandle = xEventGroupCreate();
  #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
    xHttpToSpiStreamBufferHandle = xStreamBufferCreate(CONFIG_HW_NEOPIXEL_COUNT * sizeof(struct xHwDynamicData),
    HW_DATA_CHUNK_SIZE);
  #endif
  xTaskCreate(vHwSpiMasterWriteTask, "vHwSpiMasterWriteTask", 2048, NULL, 4, &xSpiWriteTaskHandle);
  vHwSetupSpi();
  vHwSetupWifi();
//  xTaskCreate(vHwPrintIpTask, "vHwPrintIpTask", 2048, NULL, 3, NULL);
  xTaskCreate(vHwPrintTicksTask, "vHwPrintTicksTask", 2048, NULL, 3, NULL);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
//  xTaskCreate(test_spi_pattern_task, "test_spi_pattern_task", 2048, NULL, 3, &xHttpServerTaskHandle);
//  xTaskCreate(test_spi_pattern_array_task, "test_spi_pattern_array_task", 2048, NULL, 3, &xHttpServerTaskHandle);
//  xTaskCreate(vHwTestSpiDynamicTask, "vHwTestSpiDynamicTask", 2048, NULL, 3, &xHttpServerTaskHandle);
//  xTaskCreate(vHwTestSpiDynamicTaskTwo, "vHwTestSpiDynamicTaskTwo", 2048, NULL, 3, &xHttpServerTaskHandle);
  xTaskCreate(vHwTestSpiDynamicTaskThree, "vHwTestSpiDynamicTaskThree", 2048, NULL, 3, &xHttpServerTaskHandle);
  ESP_LOGI(__ESP_FILE__, "app_main done");
}
