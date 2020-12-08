#ifndef HW_DEF_H
#define HW_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

/*
 * Max size of a single SPI transfer on the ESP8266
 */
#define HW_DATA_CHUNK_SIZE (64)
/*
 * Useful for getting the minimum number of data chunks needed to transmit some stuff
 * Primarily used with sizeof() for structs and xStreamBufferBytesAvailable() for the dynamic data stream buffer
 */
#define HW_DATA_CHUNK_COUNT(x) (((x) + (HW_DATA_CHUNK_SIZE - 1)) / HW_DATA_CHUNK_SIZE)
/*
 * Bits used by the HTTP and SPI event group
 */
#define HW_BIT_SPI_TRANS_END BIT(0) // SPI transfer is complete; safe to free message data memory
#define HW_BIT_HTTP_MSG_MEM_FREE BIT(1) // Message data memory from the HTTP task has been freed
#define HW_BIT_SPI_TRANS_START BIT(2) // Ready to start SPI transfer
#define HW_BIT_SPI_TRANS_CONTINUE BIT(3) // Intermediate block in SPI transfer, after waiting for the GPIO interrupt
/*
 * Type of message in the queue
 */
enum hw_message_type {
  NO_TYPE = 0, // Unused at present
  STATIC_PATTERN_DATA, // Used in conjunction with hw_basic_pattern_data_t
  DYNAMIC_PATTERN_DATA // Reads commands from a stream buffer and sends them to the NeoPixel device
};
/*
 * Patterns understood by the NeoPixel device via SPI
 * These are selected on the HTML document
 * Most of the implementations on the NeoPixel device are stolen from the Adafruit_NeoPixel examples
 */
enum hw_pattern {
  NO_PATTERN = 0, // Used to turn off the strip
  FILL_COLOR,
  THEATRE_CHASE,
  RAINBOW_SOLID, // Strip changes color uniformly
  RAINBOW_WAVE // Strip changes color in a shifting gradient
};
/*
 * Contains information about the message, such as type and whatever
 * Useful for transmitting to the NeoPixel device more than anything
 */
struct hw_message_metadata {
  char name[16]; // Why this is important is beyond me, but I'm leaving it for now
  enum hw_message_type type;
};
/*
 * A struct for assigning a pattern to a segment of the NeoPixel strip
 * Note that it's possible to have different sections of the strip have different patterns,
 * so long as you send multiple queue messages in a row
 */
struct hw_pattern_data {
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
 * Packed structure for transferring dynamic pattern data
 */
struct hw_dynamic_data {
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
struct hw_message {
  struct hw_message_metadata *metadata;
  union {
    struct hw_pattern_data *pattern_data;
    struct {
      struct hw_pattern_data **pattern_data_array;
      uint32_t pattern_data_array_length;
    };
    StreamBufferHandle_t *xStreamBufferHandle;
  };
};

#ifdef __cplusplus
}
#endif

#endif // HW_DEF_H
