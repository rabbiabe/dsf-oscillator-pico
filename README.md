# dsf-oscillator-pico
Digital audio synthesis oscillator based on James A. Moorer's 1975 paper "The Synthesis of Complex Audio Spectra by Means of Discrete Summation Formulae;" coded for Raspberry Pi Pico.

## Folder Structure
* `/` 
  Library source
  CMakeLists.txt for example program
  * `/inc`
    Header file defining `fix15` type and necessary conversion and arithmetic macros
  * `/example`
    * /lib
      * /MCP4725_PICO
        DAC library (see "Dependencies" below)
      * /usb_midi_host
        USB-MIDI host library (see "Dependencies" below)
    * `/src`
      Example source code
  * `/resources`
    Hardware schematic for example program
    Documentation images

## Implementation
The `DsfOsc` object generates audio using Equation 4 in Moorer's paper:
![Equation 4](resources/moorer-eq4.png)
To improve speed, the math is implented using fixed-point arithmetic (see `fix15.h` and the detailed explanation in [Hunter Adams' video lecture](need link with timing)) and sine/cosine lookup tables.

### Definitions
A few constants can be found near the top of `dsf-oscillator-pico.h`:
- `two32`: the value of 2^32, used to calculate the increment values for sine and cosine
- `one15`: fixed-point representation of 1, used to simplify fixed-point calculations
- `two15`: fixed-point representation of 2, used to simplify fixed-point calculations

Although Equation 4 specifies `a < 1`, I found that values close to the boundaries produced harsh sounds and limited my example code to `0.1 < a < 0.9` – but you could try other values by adjusting the values below:
- `param_a_max15`: maximum value for the `a` term used in the synthesis equation 
- `param_a_min15`: minimum value for the `a` term used in the synthesis equation 
I also have not tried negative values but the `fix15` type is signed so you could see what happens.

### Methods
#### DsfOsc(uint16_t sample_rate, uint8_t dac_bit_depth)
The constructor takes care of a number of housekeeping/setup items:
1. Store the sample rate and DAC bit depth for use later on
2. Because the audio algorithm returns a value between `-1 < x < 1` and the DAC needs a value between `0 < x < ((2 ^ dac_bit_depth) -1)`, we calculate 1/2 of the maximum DAC value to use in scaling the output value properly.
3. The constuctor performs a one-time conversion of the sine and cosine lookup tables from `float` to `fix15`.

`sample_rate`: The audio sample rate in Hz
`dac_bit_depth`: The DAC bit depth

#### uint16_t getNextSample(fix15 param_a)
This method returns the next sample and should be called repeatedly from a timer whose period is `1,000,000  / sample_rate` (i.e., the sample rate in microseconds).

`param_a`: Fixed-point representation of the `a` term in the synthesis equation. This method checks the condition `param_a_min15 < param_a < param_a_max15` and limits out-of-bounds values to stay within the specifiewds range.

`getNextSample` returns an unsigned 16-bit value within the initialized DAC range, which can be passed directly to the DAC.

#### void freqs(fix15 freqNote, fix15 freqMod, bool reset = true)
This method sets the carrier and modulator frequencies. By default, it also resets the sine and cosine counters to zero; pass a `false` value for `reset` to override this behavior.

`freqNote`: fixed-point carrier frequency in Hz
`freqMod`: fixed-point modulator frequency in Hz
`reset`: if `true`, resets sine and cosine counters to zero

#### void freqs(fix15 freqMod, bool reset = false)
This overload allows you to alter the modulator frequency without changing the carrier frequency. By default, it **does not** reset the sine and cosine counters; pass a `true` value for `reset` to override this behavior.

`freqMod`: fixed-point modulator frequency in Hz
`reset`: if `true`, resets sine and cosine counters to zero. **N.B.: passing a `true` value for `reset` will reset BOTH counters.**

#### void resetCount()
This method resets both sine and cosine counters.

## Example Code
The example code implements monophonic oscillator with a built-in ADS envelope (I'm sure I could have worked out how to get R into that envelope but I didn't feel like working so hard for it) and support for USB-MIDI controllers.

### Hardware Setup
[Click here for PDF Schematic](resources/dsf-example-schematic-3.1.pdf)

![Schematic](resources/dsf-example-schematic-3.1.png)



### Data Structures and Definitions
`midi_note_t`: struct holding MIDI note data and a `bool` flag indicating whether the note is currently active
`envelope_mode_t`: enumeration of envelope modes, includes `release` in case anyone wants to implement that functionality :>
`midiFreq_Hz`: array of floating-point MIDI note frequencies in Hz

`VERBOSE`: if true, program will output note status and debugging messages via UART serial
`SAMPLE_RATE`: audio sample rate in Hz
`SAMPLE_INTERVAL`: timer callback interval in µs, calculated based on sample rate
`DAC_BIT_DEPTH`: DAC bit depth
`I2C_SPEED`: i2c bus speed in kHz, passed to MCP4725 constructor
`ENV_TIME_MIN`: minimum Attack/Decay time in milliseconds
`ENV_TIME_MAX`: maximum Attack/Decay time in milliseconds 

`root2`: fixed point representation of sqrt(2), used for inharmonic modulator frequencies

### Methods
void setup();
bool timerSample_cb(repeating_timer_t *rt);
void buttons_cb(uint gpio, uint32_t event_mask);
void blinkLED(uint8_t count);
uint32_t uscale(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
