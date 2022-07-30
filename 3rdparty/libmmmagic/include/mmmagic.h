/*
IMPORTANT NOTE FOR *NIX USERS:
You must add permissions to access the minimaid as a user! Either do this through udev or manually or something.The below should work but fuck if it works for me :/


sudo -i
echo SUBSYSTEM==\"usb\", ATTR{idVendor}==\"beef\", ATTR{idProduct}==\"5730\", MODE=\"0666\" > /etc/udev/rules.d/50-minimaid



*/

#ifndef MMMAGIC_H
#define MMMAGIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int MINIMAID_STATUS;
enum {
    MINIMAID_NOT_CONNECTED = 0,
    MINIMAID_CONNECTED = 1,
};

/*Minimaid bit identifiers*/
typedef int MINIMAID_LIGHT;
enum
{
	//CABINET_LIGHTS
	DDR_DOUBLE_BASS_LIGHTS = 0, //unknown but guessed
	DDR_DOUBLE_PLAYER1_PANEL = 2,
	DDR_DOUBLE_PLAYER2_PANEL = 3,
	DDR_DOUBLE_MARQUEE_LOWER_RIGHT = 4,
	DDR_DOUBLE_MARQUEE_UPPER_RIGHT = 5,
	DDR_DOUBLE_MARQUEE_LOWER_LEFT = 6,
	DDR_DOUBLE_MARQUEE_UPPER_LEFT = 7,

	//PADX_LIGHTS
	DDR_DOUBLE_PAD_UP = 0,
	DDR_DOUBLE_PAD_DOWN = 1,
	DDR_DOUBLE_PAD_LEFT = 2,
	DDR_DOUBLE_PAD_RIGHT = 3,
	DDR_DOUBLE_PAD_RESET = 4,
};

struct output_report
{
	unsigned char unknown[2]; // ?
	unsigned char buffer[4];
	unsigned char blueLedValue; // Set by mm_setBlueLED
	unsigned char kbValue; // Set by mm_setKB
};

// These return the overall target light's status once the bit has been applied.
uint32_t mm_setDDRPad1Light(MINIMAID_LIGHT bit, int ison);
uint32_t mm_setDDRPad2Light(MINIMAID_LIGHT bit, int ison);
uint32_t mm_setDDRCabinetLight(MINIMAID_LIGHT bit, int ison);
uint32_t mm_setDDRBassLight(MINIMAID_LIGHT bit, int ison);

// Connect to Minimaid device.
// Must be called before anything else will work.
MINIMAID_STATUS mm_connect_minimaid();

// Returns same value as input parameter "val" when a minimaid device is not connected (!) or when the output buffers were successfully updated.
// Returns 0 when the output buffers failed to be updated, or if val is false/0 in any of the above listed scenarios.
// Device must be initialized.
bool mm_setKB(bool val);

// Returns 255 (= result of mm_setBlueLED(0xff))
uint32_t mm_setDDRAllOn();
// Returns 0 (= result of mm_setBlueLED(0))
uint32_t mm_setDDRAllOff();

// Returns same value as input value
uint32_t mm_setBlueLED(unsigned char value);

// Write values to reports buffer.
// If minimaid is not connected, returns NULL.
// If minimaid is connected, returns a pointer to the interal output reports buffer.
// NOTE: Calling mm_sendDDRMiniMaidUpdate() will overwrite any values written using mm_setMMOutputReports() so it's only useful for getting the raw buffer.
// Device must be initialized.
output_report *mm_setMMOutputReports(unsigned char a, unsigned char b, unsigned char c, unsigned char d);

// Sends output reports buffer data to minimaid device.
// Returns 1 on success, 0 on failure or when minimaid device not found.
// NOTE: Will call mm_connect_minimaid() if not previously called already.
bool mm_sendDDRMiniMaidUpdate();

// Initialize output report buffers.
// If a minimaid device is not connected, returns 0.
// If a minimaid device is connected, returns either -1 on failure, else returns result of mm_setKB() where mm_setKB's input parameter is the last set KB value.
// NOTE: Called by mm_connect_minimaid() so not necessary to explicitly call this unless you want to reinitialize the buffers at some point.
int mm_init();

#ifdef __cplusplus
}
#endif

#endif
