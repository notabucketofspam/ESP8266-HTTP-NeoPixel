#ifndef NP_DEF_H
#define NP_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#include "esp8266/eagle_soc.h"

/*
 * Max size of a single SPI transfer on the ESP8266
 */
#define NP_DATA_CHUNK_SIZE (64)
/*
 * Useful for getting the minimum number of data chunks needed to receive some stuff
 * Data chunks are always sixteen uint32_t's, thus 64-bytes long
 */
#define NP_DATA_CHUNK_COUNT(x) (((x) + (NP_DATA_CHUNK_SIZE - 1)) / NP_DATA_CHUNK_SIZE)
/*
 * Bits used by the SPI and ANP event group
 */
#define NP_BIT_SPI_TRANS_END BIT(0)
#define NP_BIT_ANP_DYNAMIC_END BIT(1)
#define NP_BIT_ANP_DYNAMIC_START BIT(2)
/*
 * Type of message in the queue
 */
enum xNpMessageType {
  NO_TYPE = 0, // Unused at present
  STATIC_PATTERN_DATA, // Used in conjunction with np_basic_pattern_data_t
  DYNAMIC_PATTERN_DATA // Reads commands from a stream buffer and sends them to the NeoPixel device
};
/*
 * Patterns understood by the LED strip device via SPI
 * These are selected on the HTML document
 * Most of the implementations on the NeoPixel device are stolen from the Adafruit_NeoPixel examples
 */
enum xNpPattern {
  NO_PATTERN = 0, // Used to turn off the strip
  FILL_COLOUR,
  THEATRE_CHASE,
  RAINBOW_SOLID, // Strip changes color uniformly
  RAINBOW_WAVE // Strip changes color in a shifting gradient
};
/*
 * Contains information about the message, such as type and whatever
 */
struct xNpMessageMetadata {
  char pcName[16];
  enum xNpMessageType xType;
};
/*
 * A struct for assigning a predetermined pattern to a segment of the LED strip
 * Note that it's possible to have different sections of the strip have different patterns,
 * so long as you send multiple queue messages in a row
 */
struct xNpStaticData {
  enum xNpPattern xPattern;
  uint16_t usPixelIndexStart; // Inclusive, since it's zero-indexed
  uint16_t usPatternLength; // Also inclusive for the same reason
  uint32_t ulDelay; // In milliseconds; used to control the speed of effects
  uint32_t ulColour; // Mostly used for fill ulColour and whatnot
};
/*
 * Packed structure for receiving dynamic pattern data
 * Like xNpStaticData, it's crucial for the layout of this struct to match that found in HTTP-WiFi
 */
struct xNpDynamicData {
  uint32_t ulPixelIndex;
  uint32_t ulColour;
};
/*
 * The message for communicating between the HTTP server and the SPI master task
 */
struct xNpMessage {
  struct xNpMessageMetadata *pxMetadata;
  union {
//    struct xNpStaticData *pattern_data;
    StreamBufferHandle_t *pxStreamBufferHandle;
  };
};

#ifdef __cplusplus
}
#endif

#endif // NP_DEF_H
