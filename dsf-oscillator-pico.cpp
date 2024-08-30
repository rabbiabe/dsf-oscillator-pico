/************************************************************
 * Discrete Summation Formula Oscillator v2.1 (2024-07-15)
 * 
 * https://github.com/rabbiabe/dsf-oscillator-pico
 * 
 * Digital audio synthesis oscillator based on James A. 
 * Moorer's 1975 paper "The Synthesis of Complex Audio Spectra 
 * by Means of Discrete Summation Formulae;" coded for Raspberry 
 * Pi Pico. 
 * 
 * Article: https://ccrma.stanford.edu/files/papers/stanm5.pdf
 * 
 * For analysis of this synthesis method see Prof. Aaron 
 * Lanterman's video, which first brought this concept to my 
 * attention: https://www.youtube.com/watch?v=IoAc2241gx8
 ************************************************************/

#include "dsf-oscillator-pico.h"

/*!
    @brief Constructor.

    Sets up the fixed-point sine tables and basic state variables

    @param sample_rate the sample rate of the calling timer, in Hz.
    @param dac_bit_depth the number of bits (e.g., 12) in the DAC used by the calling program.
*/
DsfOsc::DsfOsc (uint16_t sample_rate, uint8_t dac_bit_depth)
{
    fs = sample_rate;
    dacbits = dac_bit_depth;    
    halfDac = float2fix15(((float)((1 << dacbits) - 1) / 2.0));

    for (uint t = 0; t < 256; t++) {
        table_sine[t] = float2fix15(table_sine_f[t]);
        table_cosine[t] = float2fix15(table_cosine_f[t]);
    }
    
}

/*!
    @brief resets the oscillator count for carrier and modulator to zero.
*/
void DsfOsc::resetCount()
{
    countNote = 0;
    countMod = 0;
}

/*!
    @brief sets the carrier and modulation frequencies for the oscillator

    @param freqNote the fixed-point frequency for the carrier
    @param freqMod the fixed-point frequency for the modulator
    @param reset resets the frequency counters; defaults true
*/
void DsfOsc::freqs(fix15 freqNote, fix15 freqMod, bool reset) 
{
    fn = freqNote;
    fm = freqMod;

    stepNote = (fix2float15(fn) * two32) / (float)fs;
    stepMod = (fix2float15(fm) * two32) / (float)fs;

    if (reset) resetCount();
}


/*!
    @brief sets the modulation frequency for the oscillator

    @param freqMod the fixed-point frequency for the modulator
    @param reset resets the frequency counters; defaults false
*/
void DsfOsc::freqs(fix15 freqMod, bool reset)
{
    fm = freqMod;

    stepMod = (fix2float15(fm) * two32) / fs;

    if (reset) resetCount();
}

/*!
    @brief generates the next sample of the synthesized wave

    This method first generates a "safe" value for `a` (`0 <= a < 1`) and calculates `a^2`. It then calculates the next sample value 
    and scales that value between 0 and dac_max. Finally, it increments the sine and cosine counters.

    @param param_a the `a` term from Moorer's equation. Limited to `0 <= a < 1`, but the function checks boundaries so you don't need to be as careful.
    @return a 16-bit integer value that can be passed directly to the DAC (assuming dac_bits is set correctly)

*/
uint16_t DsfOsc::getNextSample(fix15 param_a)
{

    fix15 param_a_safe;

    if (param_a > param_a_max15) {
        param_a_safe = param_a_max15;
    } else if (param_a < 0) {
        param_a_safe = 0;
    } else {
        param_a_safe = param_a;
    }

    fix15 a_squared = multfix15(param_a_safe,param_a_safe);

    fix15 sample = divfix15((multfix15((one15 - a_squared), table_sine[countNote >> 24])), 
                            ((one15 + a_squared) - (multfix15((multfix15(two15, param_a_safe)), table_cosine[countMod >> 24]))));

    fix15 dacValue = multfix15(sample, halfDac) + halfDac;

    countNote += stepNote;
    countMod += stepMod;

    return (uint16_t)fix2int15(dacValue);
}

