/*
 * NP = NeoPixel
 * Like the HTTP-WiFi counterpart, most of this is stolen from examples
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "np_def.h"
#include "np_spi.h"
#include "np_anp.h"

#ifdef __cplusplus
extern "C" { void app_main(void); }
#endif

/*
 * Task handles
 */
TaskHandle_t spi_read_task_handle = NULL;
TaskHandle_t anp_strip_task_handle = NULL;
/*
 * Other handles
 */
QueueHandle_t xSpiToAnpQueueHandle = NULL;
StreamBufferHandle_t xSpiToAnpStreamBufferHandle = NULL;

// TODO: make sure that anp_pinMode() doesn't interfere with the handshake pin

void app_main(void) {
  xSpiToAnpQueueHandle = xQueueCreate(CONFIG_NP_QUEUE_SIZE, sizeof(struct np_message_t *));
  #if CONFIG_NP_ENABLE_DYNAMIC_PATTERN
    xSpiToAnpStreamBufferHandle = xStreamBufferCreate(CONFIG_NP_STREAM_BUFFER_SIZE, NP_DATA_CHUNK_SIZE);
  #endif
  np_setup_spi();
//  ESP_LOGI(__ESP_FILE__, "POINT A");
  xTaskCreate(np_spi_slave_read_task, "spi_task", 2048, NULL, 4, &spi_read_task_handle);
//  ESP_LOGI(__ESP_FILE__, "POINT B");
}
