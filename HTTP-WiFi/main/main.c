/*
 * HW = HTTP-WiFi
 * A lot of this is stolen from various examples
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_log.h"

#include "hw_def.h"
#include "hw_spi.h"
#include "hw_wifi.h"

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
 * Test SPI task stuff
 */
//TaskHandle_t test_spi_task_handle = NULL;
void test_spi_task(void *arg) {
  for (;;) {
    // Set the strip to red
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct hw_message *test_message_red = malloc(sizeof(struct hw_message));
    struct hw_message_metadata *test_metadata_red = malloc(sizeof(struct hw_message_metadata));
    strcpy(test_metadata_red->name, "test_message_red");
    test_metadata_red->type = BASIC_PATTERN_DATA;
    test_message_red->metadata = test_metadata_red;
    struct hw_pattern_data *test_pattern_data_red = malloc(sizeof(struct hw_pattern_data));
    test_pattern_data_red->val = 0;
    test_pattern_data_red->cmd = 0;
    test_pattern_data_red->pattern = FILL_COLOR;
    test_pattern_data_red->pixel_index_start = 0;
    test_pattern_data_red->pixel_index_end = 29;
    test_pattern_data_red->delay = 0;
    test_pattern_data_red->color = 0x00FF0000;
    test_message_red->pattern_data = test_pattern_data_red;
    xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message_red, portMAX_DELAY);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
//    xTaskNotify(spi_write_task_handle, HW_BIT_SPI_TRANS_START, eSetBits);
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_red->metadata);
    free(test_message_red->pattern_data);
    free(test_message_red);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
//    xTaskNotify(spi_write_task_handle, HW_BIT_HTTP_MSG_MEM_FREE, eSetBits);
    // Set the strip to green
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct hw_message *test_message_green = malloc(sizeof(struct hw_message));
    struct hw_message_metadata *test_metadata_green = malloc(sizeof(struct hw_message_metadata));
    strcpy(test_metadata_green->name, "test_message_green");
    test_metadata_green->type = BASIC_PATTERN_DATA;
    test_message_green->metadata = test_metadata_green;
    struct hw_pattern_data *test_pattern_data_green = malloc(sizeof(struct hw_pattern_data));
    test_pattern_data_green->val = 0;
    test_pattern_data_green->cmd = 0;
    test_pattern_data_green->pattern = FILL_COLOR;
    test_pattern_data_green->pixel_index_start = 0;
    test_pattern_data_green->pixel_index_end = 29;
    test_pattern_data_green->delay = 0;
    test_pattern_data_green->color = 0x0000FF00;
    test_message_green->pattern_data = test_pattern_data_green;
    xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message_green, portMAX_DELAY);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
//    xTaskNotify(spi_write_task_handle, HW_BIT_SPI_TRANS_START, eSetBits);
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_green->metadata);
    free(test_message_green->pattern_data);
    free(test_message_green);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
//    xTaskNotify(spi_write_task_handle, HW_BIT_HTTP_MSG_MEM_FREE, eSetBits);
  }
}

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
  xTaskCreate(test_spi_task, "test_spi_task", 2048, NULL, 4, &http_server_task_handle);
}
