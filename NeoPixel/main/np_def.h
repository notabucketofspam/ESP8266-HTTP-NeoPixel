#ifndef NP_DEF_H
#define NP_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

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
  FILL_COLOR,
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
 * A struct for assigning a bPattern to a segment of the LED strip
 * Note that it's possible to have different sections of the strip have different patterns,
 * so long as you send multiple queue messages in a row
 */
struct xNpStaticData {
  union {
    struct {
      uint32_t bCmd: 3; // Unused at the moment
      uint32_t bPattern: 5;
      uint32_t bPixelIndexStart: 12; // Inclusive, since it's zero-indexed
      uint32_t bPixelIndexEnd: 12; // Also inclusive for the same reason
    };
    uint32_t bVal; // Fill for the union; usually just set this to zero to clear it
  };
  uint32_t ulDelay; // In milliseconds; used to control the speed of effects
  uint32_t ulColor; // Mostly used for fill ulColor and whatnot
};
/*
 * Packed structure for receiving dynamic bPattern data
 */
struct np_dynamic_data {
  union { // Done to at least partially match the layout of hw_pattern_data
    struct {
      uint16_t bCmd: 3;
      uint16_t bPattern: 1;
      uint16_t bPixelIndex: 12;
    };
    uint16_t bVal;
  };
  uint32_t ulColor; // This could've been 24-bit but then that would've negated RGBW support
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
