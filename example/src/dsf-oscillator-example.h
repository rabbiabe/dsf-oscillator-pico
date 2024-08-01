/************************************************************
 * Discrete Summation Formula Oscillator
 * Example Implementation v2.3 (2024-07-19)
 * 
 * https://github.com/rabbiabe/dsf-oscillator-pico
 * 
 * Code incorporates portions of usb_midi_host_pio_example.c 
 * included with the usb_midi_host library: 
 * https://github.com/rppicomidi/usb_midi_host
 ************************************************************/

#pragma once

/*
 * C++ HEADERS
 */
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstring>

/*
 * PICO HEADERS
 */
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

/*
 * LIBRARIES
 */
#include "../../dsf-oscillator-pico.h"
#include "../lib/MCP4725_PICO/include/mcp4725/mcp4725.hpp"
#include "../lib/usb_midi_host/usb_midi_host.h"

/*
 * PROJECT VARIABLES
 */
#define SAMPLE_RATE 40000 // audio sample rate in Hz
#define DAC_BIT_DEPTH 12
#define SAMPLE_INTERVAL 1000000 / SAMPLE_RATE // timer callback interval in µs based on sample rate, negative for evenly-spaced calls
#define LFO_INTERVAL -1000000 / 100 // LFO has 100 Hz sample rate, should be fine for range of 0.1-10 HZ
#define I2C_SPEED 400 // i2c bus speed in kHz

constexpr uint8_t   pinCenter = 26,
                    pinLfoRate = 27,
                    pinFmod = 28,
                    adc_in_Center = 0,
                    adc_in_LfoRate = 1,
                    adc_in_Fmod = 2,
                    pinSDA = 4,
                    pinSCL = 5,
                    pinNormalize = 20,
                    pinHarmonic = 21,
                    pinLfoToggle = 22,
                    pinStatusLoop = 11,
                    pinStatusNormalize = 12,
                    pinStatusHarmonic = 13,
                    pinStatusLFO = 14;

constexpr float midiFreq_Hz[128] = { 8.18,8.66,9.18,9.72,10.3,10.91,11.56,12.25,12.98,13.75,14.57,15.43,16.35,17.32,18.35,19.45,20.6,21.83,23.12,24.5,25.96,27.5,29.14,30.87,32.7,34.65,36.71,38.89,41.2,43.65,46.25,49,51.91,55,58.27,61.74,65.41,69.3,73.42,77.78,82.41,87.31,92.5,98,103.83,110,116.54,123.47,130.81,138.59,146.83,155.56,164.81,174.61,185,196,207.65,220,233.08,246.94,261.63,277.18,293.66,311.13,329.63,349.23,369.99,392,415.3,440,466.16,493.88,523.25,554.37,587.33,622.25,659.26,698.46,739.99,783.99,830.61,880,932.33,987.77,1046.5,1108.73,1174.66,1244.51,1318.51,1396.91,1479.98,1567.98,1661.22,1760,1864.66,1975.53,2093,2217.46,2349.32,2489.02,2637.02,2793.83,2959.96,3135.96,3322.44,3520,3729.31,3951.07,4186.01,4434.92,4698.64,4978.03,5274.04,5587.65,5919.91,6271.93,6644.88,7040,7458.62,7902.13,8372.02,8869.84,9397.27,9956.06,10548.08,11175.3,11839.82,12543.85 };
constexpr fix15 root2 = float2fix15(1.4142135624);
//constexpr fix15 semitone = float2fix15(0.943865);
//constexpr fix15 mod_LfoRate_one_semitone = float2fix15(0.99999275);

fix15 midiFreq15[128];

volatile bool isNormalized = false, isHarmonic = true, lfo_active = false;
volatile uint16_t param_center; 
volatile int16_t lfoValue;
volatile uint32_t lfoCounter = 0, lfoStep = 0;

typedef struct {
    bool active;
    uint8_t command, note, velocity;
} MidiNote;

MidiNote thisNote;
MidiNote lastNote;

repeating_timer_t timerSample, timerLFO;

constexpr int16_t lfo_table_sine[256] = { 0,9,19,29,39,49,58,68,78,87,97,106,116,125,134,144,153,162,171,180,188,197,205,214,222,230,238,246,253,261,268,276,283,289,296,303,309,315,321,327,332,338,343,348,352,357,361,365,369,373,376,379,382,385,388,390,392,394,395,397,398,398,399,399,399,399,399,398,397,396,395,393,392,390,387,385,382,379,376,372,369,365,361,356,352,347,342,337,332,326,320,314,308,302,295,288,282,275,267,260,252,245,237,229,221,213,204,196,187,178,169,160,151,142,133,124,114,105,95,86,76,67,57,47,37,28,18,8,-1,-11,-21,-30,-40,-50,-60,-69,-79,-89,-98,-108,-117,-126,-136,-145,-154,-163,-172,-181,-190,-198,-207,-215,-223,-231,-239,-247,-255,-262,-269,-277,-284,-290,-297,-304,-310,-316,-322,-328,-333,-338,-344,-348,-353,-358,-362,-366,-370,-373,-377,-380,-383,-386,-388,-390,-392,-394,-395,-397,-398,-399,-399,-399,-399,-399,-399,-398,-397,-396,-395,-393,-391,-389,-387,-384,-382,-379,-375,-372,-368,-364,-360,-356,-351,-346,-341,-336,-331,-325,-319,-313,-307,-301,-294,-287,-281,-273,-266,-259,-251,-244,-236,-228,-220,-211,-203,-194,-186,-177,-168,-159,-150,-141,-132,-122,-113,-104,-94,-84,-75,-65,-55,-46,-36,-26,-16,-6 };

DsfOsc osc(SAMPLE_RATE, DAC_BIT_DEPTH);

MCP4725_PICO dac;



/*
 * PROJECT FUNCTIONS
 */
void setup();
bool timerSample_cb(repeating_timer_t *rt);
bool timerLFO_cb(repeating_timer_t *rt);
void buttons_cb(uint gpio, uint32_t event_mask);
void blinkLED(uint8_t count);
int32_t scale(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

/*
 * USB MIDI HOST FUNCTIONS
 */
void core1_main();
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx);
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
void tuh_midi_tx_cb(uint8_t dev_addr);