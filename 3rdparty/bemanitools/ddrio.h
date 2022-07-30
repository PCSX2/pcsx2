#ifndef BEMANITOOLS_DDRIO_H
#define BEMANITOOLS_DDRIO_H

#include <stdbool.h>
#include <stdint.h>

#include "bemanitools/glue.h"

enum ddr_pad_bit {
    DDR_TEST = 0x04,
    DDR_COIN = 0x05,
    DDR_SERVICE = 0x06,

    DDR_P2_START = 0x08,
    DDR_P2_UP = 0x09,
    DDR_P2_DOWN = 0x0A,
    DDR_P2_LEFT = 0x0B,
    DDR_P2_RIGHT = 0x0C,
    DDR_P2_MENU_LEFT = 0x0E,
    DDR_P2_MENU_RIGHT = 0x0F,
    DDR_P2_MENU_UP = 0x02,
    DDR_P2_MENU_DOWN = 0x03,

    DDR_P1_START = 0x10,
    DDR_P1_UP = 0x11,
    DDR_P1_DOWN = 0x12,
    DDR_P1_LEFT = 0x13,
    DDR_P1_RIGHT = 0x14,
    DDR_P1_MENU_LEFT = 0x16,
    DDR_P1_MENU_RIGHT = 0x17,
    DDR_P1_MENU_UP = 0x00,
    DDR_P1_MENU_DOWN = 0x01,
};

// see the functions below for more information

enum p3io_light_bit {
    LIGHT_P1_MENU = 0x00,
    LIGHT_P2_MENU = 0x01,
    LIGHT_P2_LOWER_LAMP = 0x04,
    LIGHT_P2_UPPER_LAMP = 0x05,
    LIGHT_P1_LOWER_LAMP = 0x06,
    LIGHT_P1_UPPER_LAMP = 0x07,
};

enum hdxs_light_bit {
    LIGHT_HD_P1_START = 0x08,
    LIGHT_HD_P1_UP_DOWN = 0x09,
    LIGHT_HD_P1_LEFT_RIGHT = 0x0A,
    LIGHT_HD_P2_START = 0x0B,
    LIGHT_HD_P2_UP_DOWN = 0x0C,
    LIGHT_HD_P2_LEFT_RIGHT = 0x0D,
};

// the indexing starts from 0x20 if you're looking in geninput
enum hdxs_rgb_light_idx {
    LIGHT_HD_P1_SPEAKER_F_R = 0x00,
    LIGHT_HD_P1_SPEAKER_F_G = 0x01,
    LIGHT_HD_P1_SPEAKER_F_B = 0x02,
    LIGHT_HD_P2_SPEAKER_F_R = 0x03,
    LIGHT_HD_P2_SPEAKER_F_G = 0x04,
    LIGHT_HD_P2_SPEAKER_F_B = 0x05,
    LIGHT_HD_P1_SPEAKER_W_R = 0x06,
    LIGHT_HD_P1_SPEAKER_W_G = 0x07,
    LIGHT_HD_P1_SPEAKER_W_B = 0x08,
    LIGHT_HD_P2_SPEAKER_W_R = 0x09,
    LIGHT_HD_P2_SPEAKER_W_G = 0x0A,
    LIGHT_HD_P2_SPEAKER_W_B = 0x0B,
};

enum extio_light_bit {
    LIGHT_NEONS = 0x0E,

    LIGHT_P2_RIGHT = 0x13,
    LIGHT_P2_LEFT = 0x14,
    LIGHT_P2_DOWN = 0x15,
    LIGHT_P2_UP = 0x16,

    LIGHT_P1_RIGHT = 0x1B,
    LIGHT_P1_LEFT = 0x1C,
    LIGHT_P1_DOWN = 0x1D,
    LIGHT_P1_UP = 0x1E
};

/* The first function that will be called on your DLL. You will be supplied
   with four function pointers that may be used to log messages to the game's
   log file. See comments in glue.h for further information. */

void ddr_io_set_loggers(
    log_formatter_t misc,
    log_formatter_t info,
    log_formatter_t warning,
    log_formatter_t fatal);

/* Initialize your DDR IO emulation DLL. Thread management functions are
   provided to you; you must use these functions to create your own threads if
   you want to make use of the logging functions that are provided to
   eam_io_set_loggers(). You will also need to pass these thread management
   functions on to geninput if you intend to make use of that library.

   See glue.h and geninput.h for further details. */

bool ddr_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy);


/* used to poll the IO for input, note that this is also where lights are
   flushed to the device for geninput */
uint32_t ddr_io_read_pad(void);

/* The following are optional to implement, and allow lights to be set and
   emulated as needed */

/* used to set pad lights, as well as the bass neon on SD cabs */
void ddr_io_set_lights_extio(uint32_t extio_lights);

/* used to set SD panel lights, and HD spot lights */
void ddr_io_set_lights_p3io(uint32_t p3io_lights);

/* used to set HD cab front panel lights */
void ddr_io_set_lights_hdxs_panel(uint32_t hdxs_lights);

/* used to set HD cab speaker RGB lights */
void ddr_io_set_lights_hdxs_rgb(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);

void ddr_io_fini(void);

#endif
