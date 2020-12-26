#include "np_anp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The main object that will be referred to throughout the program
 */
static struct AnpStrip *pxAnpStrip;

void vNpSetupAnp(void) {
  // Currently only GRB at 800kHz is supported, as it would be way too tedious to add all the options to Kconfig
  pxAnpStrip = new_AnpStrip(CONFIG_NP_NEOPIXEL_COUNT, CONFIG_NP_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
  anp_begin(pxAnpStrip);
  // Dynamic data processing happens separately since it has its own dedicated stream buffer
}
//void IRAM_ATTR vNpAnpStripUpdateTask(void *arg) {
//  for (;;) {
//    // Do nothing for now, eventually this will run the majority of static pattern updates
//  }
//}
void IRAM_ATTR vDynamicDataProcessTask(void *arg) {
  static struct xNpDynamicData xDynamicDataBuffer;
//  memset(&xDynamicDataBuffer, 0x00, sizeof(struct xNpDynamicData));
  for (;;) {
    xEventGroupWaitBits(xSpiAndAnpEventGroupHandle, NP_BIT_ANP_DYNAMIC_START, pdTRUE, pdTRUE, portMAX_DELAY);
    anp_clear(pxAnpStrip);
//    ESP_LOGI(__ESP_FILE__, "Task resume");
    // Since the metadata has already been stripped from the message, we can find the
    // total number of pixels to update via simple arithmetic
    // This rounds up to the nearest data chunk, so we have to use an all-high
    // marker as a stop point
    uint16_t usTotalPixelCount = xStreamBufferBytesAvailable(xSpiStreamBufferHandle) /
      sizeof(struct xNpDynamicData);
//    uint16_t usTotalPixelCount = 300;
//    ESP_LOGI(__ESP_FILE__, "usTotalPixelCount %u", usTotalPixelCount);
    // Cycle through every pixel given to us and change its color on the strip
    for (uint16_t usPixelCounter = 0; usPixelCounter < usTotalPixelCount; ++usPixelCounter) {
//      ESP_LOGI(__ESP_FILE__, "usPixelCounter %u", usPixelCounter);
//      ESP_LOGI(__ESP_FILE__, "avail: %u", xStreamBufferBytesAvailable(xSpiStreamBufferHandle));
      xStreamBufferReceive(xSpiStreamBufferHandle, (void *) &xDynamicDataBuffer, sizeof(struct xNpDynamicData),
        portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "0x%X %X", ((uint32_t *) &xDynamicDataBuffer)[0], ((uint32_t *) &xDynamicDataBuffer)[1]);
      if (!((((uint32_t *) &xDynamicDataBuffer)[0] ^ UINT32_MAX) &
        (((uint32_t *) &xDynamicDataBuffer)[1] ^ UINT32_MAX))) {
        break;
      }
      anp_setPixelColor_C(pxAnpStrip, xDynamicDataBuffer.ulPixelIndex, xDynamicDataBuffer.ulColor);
//      ESP_LOGI(__ESP_FILE__, "pixel %u, red %u, green %u, blue%u", xDynamicDataBuffer.ulPixelIndex,
//        xDynamicDataBuffer.ulColor >> 16 & 0xFF, xDynamicDataBuffer.ulColor >>  8 & 0xFF,
//        xDynamicDataBuffer.ulColor & 0xFF);
    }
    // Clear the stream buffer of the remaining garbage bytes
    if (xStreamBufferBytesAvailable(xSpiStreamBufferHandle) > 0) {
      xStreamBufferReset(xSpiStreamBufferHandle);
    }
    vPortEnterCritical();
    anp_show(pxAnpStrip);
    vPortExitCritical();
//    ESP_LOGI(__ESP_FILE__, "heap free: %u", esp_get_free_heap_size());
    xEventGroupSetBits(xSpiAndAnpEventGroupHandle, NP_BIT_ANP_DYNAMIC_END);
//    ESP_LOGI(__ESP_FILE__, "Task complete");
  }
}

#ifdef __cplusplus
}
#endif
