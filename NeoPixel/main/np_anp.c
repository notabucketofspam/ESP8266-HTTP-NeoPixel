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
}
void IRAM_ATTR vNpAnpStripUpdateTask(void *arg) {

}

#ifdef __cplusplus
}
#endif
