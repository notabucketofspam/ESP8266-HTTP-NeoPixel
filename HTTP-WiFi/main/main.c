/*
 * HW = HTTP-WiFi
 * A lot of this is stolen from various examples
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_log.h"

#include "hw_def.h"
#include "hw_spi.h"
#include "hw_wifi.h"
#include "hw_http.h"
#include "hw_test.h"

#include "anp_component.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Task handles
 */
TaskHandle_t spi_write_task_handle = NULL;
TaskHandle_t http_server_task_handle = NULL;
/*
 * Other handles
 */
QueueHandle_t http_to_spi_queue_handle = NULL;
EventGroupHandle_t http_and_spi_event_group_handle = NULL;
StreamBufferHandle_t http_to_spi_stream_buffer_handle = NULL;

/*
 * This is all the task creation, peripheral initialization, etc
 */
void app_main(void) {
  http_to_spi_queue_handle = xQueueCreate(CONFIG_HW_QUEUE_SIZE, sizeof(struct hw_message *));
  http_and_spi_event_group_handle = xEventGroupCreate();
  #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
    http_to_spi_stream_buffer_handle = xStreamBufferCreate(CONFIG_HW_STREAM_BUFFER_SIZE, HW_DATA_CHUNK_SIZE);
  #endif
  hw_setup_spi();
//  vTaskDelay(2000 / portTICK_PERIOD_MS);
  xTaskCreate(hw_spi_master_write_task, "hw_spi_master_write_task", 2048, NULL, 4, &spi_write_task_handle);
//  xTaskCreate(test_spi_pattern_task, "test_spi_pattern_task", 2048, NULL, 3, &http_server_task_handle);
//  xTaskCreate(test_spi_pattern_array_task, "test_spi_pattern_array_task", 2048, NULL, 3, &http_server_task_handle);
  xTaskCreate(test_spi_dynamic_task, "test_spi_dynamic_task", 2048, NULL, 3, &http_server_task_handle);
}
