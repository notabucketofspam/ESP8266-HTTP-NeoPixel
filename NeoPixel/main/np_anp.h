#ifndef NP_ANP_H
#define NP_ANP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_attr.h"

#include "np_def.h"

#include "anp_component.h"

/*
 * Not used currently
 */
extern TaskHandle_t xAnpStripTaskHandle;
/*
 * Handles for receiving different kinds data from the SPI task
 */
extern QueueHandle_t xSpiToAnpQueueHandle;
extern EventGroupHandle_t xSpiAndAnpEventGroupHandle;
extern StreamBufferHandle_t xSpiToAnpStreamBufferHandle;
/*
 * Uses the Kconfig values for the strip length
 */
void vNpSetupAnp(void);
/*
 * Schedule sub-processes to run dynamic / static patterns
 */
void IRAM_ATTR vNpAnpStripUpdateTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif // NP_ANP_H
