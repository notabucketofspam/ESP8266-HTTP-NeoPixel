/*
 * HW = HTTP-WiFi
 * A lot of this is stolen from various examples
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "hw_def.h"
#include "hw_spi.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Below is a list of handles for the running various tasks / queues / etc
 */
TaskHandle_t spi_write_task_handle;
QueueHandle_t http_to_spi_queue_handle;
StreamBufferHandle_t http_to_spi_stream_buffer_handle;
/*
 * This is all the task creation, peripheral initialization, etc
 */
void app_main(void) {
  http_to_spi_queue_handle = xQueueCreate(CONFIG_HW_QUEUE_SIZE, sizeof(struct hw_message_t *));
  #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
    http_to_spi_stream_buffer_handle = xStreamBufferCreate(CONFIG_HW_STREAM_BUFFER_SIZE, 64);
  #endif
  hw_setup_spi();
  xTaskCreate(hw_spi_master_write_task, "spi_task", 2048, NULL, 4, &spi_write_task_handle);
}
