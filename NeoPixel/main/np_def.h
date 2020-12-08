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
enum np_message_type {
  NO_TYPE = 0, // Unused at present
  STATIC_PATTERN_DATA, // Used in conjunction with np_basic_pattern_data_t
  DYNAMIC_PATTERN_DATA // Reads commands from a stream buffer and sends them to the NeoPixel device
};
/*
 * Patterns understood by the LED strip device via SPI
 * These are selected on the HTML document
 * Most of the implementations on the NeoPixel device are stolen from the Adafruit_NeoPixel examples
 */
enum np_pattern {
  NO_PATTERN = 0, // Used to turn off the strip
  FILL_COLOR,
  THEATRE_CHASE,
  RAINBOW_SOLID, // Strip changes color uniformly
  RAINBOW_WAVE // Strip changes color in a shifting gradient
};
/*
 * Contains information about the message, such as type and whatever
 */
struct np_message_metadata {
  char name[16];
  enum np_message_type type;
};
/*
 * A struct for assigning a pattern to a segment of the LED strip
 * Note that it's possible to have different sections of the strip have different patterns,
 * so long as you send multiple queue messages in a row
 */
struct np_pattern_data {
  union {
    struct {
      uint32_t cmd: 3; // Unused at the moment
      uint32_t pattern: 5;
      uint32_t pixel_index_start: 12; // Inclusive, since it's zero-indexed
      uint32_t pixel_index_end: 12; // Also inclusive for the same reason
    };
    uint32_t val; // Fill for the union; usually just set this to zero to clear it
  };
  uint32_t delay; // In milliseconds; used to control the speed of effects
  uint32_t color; // Mostly used for fill color and whatnot
};
/*
 * Packed structure for receiving dynamic pattern data
 */
struct np_dynamic_data {
  union { // Done to at least partially match the layout of hw_pattern_data
    struct {
      uint16_t cmd: 3;
      uint16_t pattern: 1;
      uint16_t pixel_index: 12;
    };
    uint16_t val;
  };
  uint32_t color; // This could've been 24-bit but then that would've negated RGBW support
};
/*
 * The message for communicating between the HTTP server and the SPI master task
 */
struct np_message {
  struct np_message_metadata *metadata;
  union {
    struct np_pattern_data *pattern_data;
    StreamBufferHandle_t *stream_buffer_handle;
  };
};

#ifdef __cplusplus
}
#endif

#endif // NP_DEF_H
