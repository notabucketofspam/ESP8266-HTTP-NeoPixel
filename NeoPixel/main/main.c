#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "np_def.h"
#include "np_spi.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Below is a list of handles for the running various tasks / queues / etc
 */
TaskHandle_t spi_read_task_handle = NULL;
QueueHandle_t spi_to_anp_queue_handle = NULL;
StreamBufferHandle_t spi_to_anp_stream_buffer_handle = NULL;

// TODO: make sure that anp_pinMode() doesn't interfere with the handshake pin

void app_main(void) {
  spi_to_anp_queue_handle = xQueueCreate(CONFIG_NP_QUEUE_SIZE, sizeof(struct np_message_t *));
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    spi_to_anp_stream_buffer_handle = xStreamBufferCreate(CONFIG_NP_STREAM_BUFFER_SIZE, 64);
  #endif
  np_setup_spi();
  xTaskCreate(np_spi_slave_read_task, "spi_task", 2048, NULL, 4, &spi_read_task_handle);
}
