#ifndef NP_SPI_H
#define NP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "driver/hspi_logic_layer.h"

#include <inttypes.h>
#include "esp_log.h"

#include "np_def.h"

/*
 * These pass data from the SPI task to the main NeoPixel task
 */
extern QueueHandle_t spi_to_anp_queue_handle;
extern StreamBufferHandle_t spi_to_anp_stream_buffer_handle;
/*
 * This is used to resume the SPI read task, which remains suspended until a data transfer is completed
 */
extern TaskHandle_t spi_read_task_handle;
/*
 *  Saves a lot of headache in app_main()
 */
void np_setup_spi();
/*
 * Main task for receiving data from the HTTP-WiFi device.
 */
void IRAM_ATTR np_spi_slave_read_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif
