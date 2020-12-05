/*
 * HW = HTTP-WiFi
 * A lot of this is stolen from various examples
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include <inttypes.h>
#include "esp_log.h"

#include "hw_def.h"
#include "hw_spi.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Below is a list of handles for the running various tasks / queues / etc
 */
TaskHandle_t spi_write_task_handle = NULL;
QueueHandle_t http_to_spi_queue_handle = NULL;
EventGroupHandle_t http_and_spi_event_group_handle = NULL;
StreamBufferHandle_t http_to_spi_stream_buffer_handle = NULL;
/*
 * This is all the task creation, peripheral initialization, etc
 */
void app_main(void) {
  http_and_spi_event_group_handle = xEventGroupCreate();
  http_to_spi_queue_handle = xQueueCreate(CONFIG_HW_QUEUE_SIZE, sizeof(struct hw_message *));
  #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
    http_to_spi_stream_buffer_handle = xStreamBufferCreate(CONFIG_HW_STREAM_BUFFER_SIZE, 64);
  #endif
  hw_setup_spi();
  xTaskCreate(hw_spi_master_write_task, "spi_task", 2048, NULL, 4, &spi_write_task_handle);
  // Test message stuff
  struct hw_message *test_message = malloc(sizeof(struct hw_message *));
  struct hw_message_metadata *test_metadata = malloc(sizeof(struct hw_message_metadata));
  strcpy(test_metadata->name, "test_message");
  test_metadata->type = BASIC_PATTERN_DATA;
  test_message->metadata = test_metadata;
  struct hw_pattern_data *test_pattern_data = malloc(sizeof(struct hw_pattern_data));
  test_pattern_data->val = 0;
  test_pattern_data->cmd = 0;
  test_pattern_data->pattern = FILL_COLOR;
  test_pattern_data->pixel_index_start = 0;
  test_pattern_data->pixel_index_end = 29;
  test_pattern_data->delay = 20;
  test_pattern_data->color = 0xFF;
  test_message->pattern_data = test_pattern_data;
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  // Actually send the message now
  xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message, portMAX_DELAY);
  vTaskResume(spi_write_task_handle);
  xEventGroupWaitBits(http_and_spi_event_group_handle, HW_SPI_TRANS_COMP, pdTRUE, pdTRUE, portMAX_DELAY);
  free(test_message->metadata);
  free(test_message->pattern_data);
  free(test_message);
  xEventGroupSetBits(http_and_spi_event_group_handle, HW_MSG_MEM_FREED);
}
