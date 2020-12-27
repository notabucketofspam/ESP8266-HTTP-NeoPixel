#ifndef HW_SPI_H
#define HW_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "esp_log.h"

#include "hw_def.h"

/*
 * Passes data between the HTTP server and the main SPI task
 */
extern QueueHandle_t xHttpToSpiQueueHandle;
/*
 * For setting statuses between the HTTP task and the SPI task
 * Used for things like letting the HTTP task know when it's safe to free message data memory
 */
extern EventGroupHandle_t xHttpAndSpiEventGroupHandle;
/*
 * Used for unblocking the SPI write task after waiting for the GPIO interrupt
 */
extern TaskHandle_t xSpiWriteTaskHandle;
/*
 * An easier way to set up SPI-specific things, so that app_main() doesn't get bloated and unreadable.
 * This function assumes a bunch of default values, but allows for Kconfig things to be set
 */
void vHwSpiSetup(void);
/*
 * Main task for sending data to the NeoPixel device.
 * Automatically suspends itself upon writing, and can only be resumed by the HTTP task.
 * Due to this suspension, if dynamic pattern data is provided, it must be fed into the stream buffer in a large chunk,
 * so that said buffer doesn't run out and cause incomplete communication.
 */
void IRAM_ATTR vHwSpiPrimaryWriteTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif // HW_SPI_H
