/************************************************************
 * Discrete Summation Formula Oscillator
 * Example Implementation v3.1 (2024-08-30)
 * 
 * https://github.com/rabbiabe/dsf-oscillator-pico
 * 
 * Code incorporates portions of usb_midi_host_pio_example.c 
 * included with the usb_midi_host library: 
 * https://github.com/rppicomidi/usb_midi_host
 ************************************************************/

#pragma once

/********************
 * C++ HEADERS
 ********************/
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstring>

/********************
 * PICO HEADERS
 ********************/
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pio_usb.h"
#include "tusb.h"

/********************
 * LIBRARIES
 ********************/
#include "../../dsf-oscillator-pico.h"
#include "../lib/MCP4725_PICO/include/mcp4725/mcp4725.hpp"
#include "../lib/usb_midi_host/usb_midi_host.h"

/********************
 * PROJECT DEFINES
 ********************/
#define VERBOSE false // print note status and debugging messages

#define SAMPLE_RATE 40000 // audio sample rate in Hz
#define SAMPLE_INTERVAL 1000000 / SAMPLE_RATE // timer callback interval in µs based on sample rate
#define DAC_BIT_DEPTH 12
#define I2C_SPEED 400 // i2c bus speed in kHz
#define ENV_TIME_MIN 100 //ms
#define ENV_TIME_MAX 1000 //ms

/********************
 * GPIO PINS
 ********************/
constexpr uint8_t   pinEnvAttack = 26,
                    pinEnvDecay = 27,
                    pinEnvSustain = 28,
                    pinEnvInvert = 21,

                    adc_in_EnvAttack = 0,
                    adc_in_EnvDecay = 1,
                    adc_in_EnvSustain = 2,

                    pinSDA = 4,
                    pinSCL = 5,


                    pinMult = 20,
                    pinHarmonic = 22,

                    pinStatusMult = 11,
                    pinStatusEnvInvert = 13,
                    pinStatusHarmonic = 14;

/********************
 * ENVELOPE
 ********************/

/*!
    @brief enumeration of envelope modes. `release` not implemented in v3.0 but maybe one day...
*/
enum envelope_mode_t : uint8_t
{
    attack,
    decay,
    sustain,
    release
};

fix15 envelope, envSustain;
envelope_mode_t envMode;
uint32_t envCounter;
uint8_t envAttack, envDecay;
constexpr fix15 envStep = float2fix15(0.001);
volatile bool envInvert = true, multState = true;

/*!
    @brief these values are used to scale the `attack` and `decay` ADC readings
*/
constexpr uint8_t envRangeMin = (ENV_TIME_MIN / (SAMPLE_INTERVAL)), 
                    envRangeMax = (ENV_TIME_MAX / (SAMPLE_INTERVAL));


/********************
 * MIDI & OSCILLATOR
 ********************/
constexpr float midiFreq_Hz[128] = { 8.18,8.66,9.18,9.72,10.3,10.91,11.56,12.25,12.98,13.75,14.57,15.43,16.35,17.32,18.35,19.45,20.6,21.83,23.12,24.5,25.96,27.5,29.14,30.87,32.7,34.65,36.71,38.89,41.2,43.65,46.25,49,51.91,55,58.27,61.74,65.41,69.3,73.42,77.78,82.41,87.31,92.5,98,103.83,110,116.54,123.47,130.81,138.59,146.83,155.56,164.81,174.61,185,196,207.65,220,233.08,246.94,261.63,277.18,293.66,311.13,329.63,349.23,369.99,392,415.3,440,466.16,493.88,523.25,554.37,587.33,622.25,659.26,698.46,739.99,783.99,830.61,880,932.33,987.77,1046.5,1108.73,1174.66,1244.51,1318.51,1396.91,1479.98,1567.98,1661.22,1760,1864.66,1975.53,2093,2217.46,2349.32,2489.02,2637.02,2793.83,2959.96,3135.96,3322.44,3520,3729.31,3951.07,4186.01,4434.92,4698.64,4978.03,5274.04,5587.65,5919.91,6271.93,6644.88,7040,7458.62,7902.13,8372.02,8869.84,9397.27,9956.06,10548.08,11175.3,11839.82,12543.85 };
constexpr fix15 root2 = float2fix15(1.4142135624);

volatile bool isHarmonic = true;

/*!
    @brief container to hold MIDI note values

    @param active true/false whether this note is "on" or "off"
    @param command stores the MIDI command byte (0x8x, 0x9x, etc.)
    @param note stores the MIDI note / controller number
    @param velocity stores the note velocity / controller value
*/
typedef struct {
    bool active;
    uint8_t command, note, velocity;
} midi_note_t;

midi_note_t thisNote;
midi_note_t lastNote;

fix15 midiFreq15[128], modFactor15[2] = { divfix15(int2fix15(1), int2fix15(2)), int2fix15(2) };

DsfOsc osc(SAMPLE_RATE, DAC_BIT_DEPTH);
MCP4725_PICO dac;
repeating_timer_t timerSample;

/********************
 * PROJECT FUNCTIONS
 ********************/
void setup();
bool timerSample_cb(repeating_timer_t *rt);
void buttons_cb(uint gpio, uint32_t event_mask);
void blinkLED(uint8_t count);
uint32_t uscale(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);

/******************************
 * USB MIDI HOST FUNCTIONS
 ******************************/
void core1_main();
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx);
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
void tuh_midi_tx_cb(uint8_t dev_addr);