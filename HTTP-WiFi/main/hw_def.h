#ifndef HW_DEF_H
#define HW_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Type of message in the queue
 */
enum hw_message_type {
  NO_TYPE = 0, // Unused at present
  BASIC_PATTERN_DATA, // Used in conjunction with hw_basic_pattern_data_t
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
  } spec; // Pattern specifications
  struct {
    uint32_t delay; // In milliseconds; used to control the speed of effects
    uint32_t color; // Mostly used for fill color and whatnot
  } mod; // Pattern modifications
};
/*
 * The message for communicating between the HTTP server and the SPI master task
 */
struct hw_message {
  struct hw_message_metadata *metadata;
  void *data;
};

#ifdef __cplusplus
}
#endif

#endif
