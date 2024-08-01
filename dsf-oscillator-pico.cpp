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

DsfOsc::DsfOsc (uint16_t sample_rate, uint8_t dac_bit_depth)
{
    fs = sample_rate;
    dacbits = dac_bit_depth;    
    halfDac = float2fix15(((float)((1 << dacbits) - 1) / 2.0));

    fixTables();
}

void DsfOsc::fixTables()
{
    for (uint t = 0; t < 256; t++) {
        table_sine[t] = float2fix15(table_sine_f[t]);
        table_cosine[t] = float2fix15(table_cosine_f[t]);
    }
    
    for (uint n = 0; n < 100; n++) {
        normValue[n] = float2fix15(normValue_f[n]);
    }
}

void DsfOsc::freqs(fix15 freqNote, fix15 freqMod, bool x10)
{
    uint32_t denominator = x10 ? (fs * 10) : fs;

    fn = freqNote;
    fm = freqMod;

    stepNote = (fix2int15(fn) * two32) / (denominator);
    stepMod = (fix2int15(fm) * two32) / (denominator);

}

void DsfOsc::freqs(fix15 freqMod, bool x10)
{
    uint32_t denominator = x10 ? (fs * 10) : fs;
    fm = freqMod;
    stepMod = (fix2int15(fm) * two32) / (denominator);
}

uint16_t DsfOsc::getNextSample(int16_t param_a_x1000)
{
    if (is_logging) gpio_put(log_pin, true);

    fix15 param_a_safe;

    if (param_a_x1000 >= 1000) {
        param_a_safe = param_a_max15;
    } else if (param_a_x1000 < 0) {
        param_a_safe = zero15;
    } else {
        param_a_safe = divfix15(int2fix15(param_a_x1000),thousand15);
    }
   
    fix15 a_squared = multfix15(param_a_safe,param_a_safe);
   

    fix15 sample = divfix15((multfix15((one15 - a_squared), table_sine[countNote >> 24])), 
                            ((one15 + a_squared) - (multfix15((multfix15(two15, param_a_safe)), table_cosine[countMod >> 24]))));
    
    
    
    
    fix15 dacValue = multfix15(sample, halfDac) + halfDac;

    countNote += stepNote;
    countMod += stepMod;
    
    if (is_logging) gpio_put(log_pin, false);

    return (uint16_t)fix2int15(dacValue);
}

void DsfOsc::logging(uint8_t pin)
{
    is_logging = true;
    log_pin = pin;
    gpio_put(log_pin, false);
}

void DsfOsc::logging()
{
    is_logging = false;
}


void DsfOsc::normalize(bool setting)
{
    normalized = setting;
}

void DsfOsc::normalize()
{
    normalized = !normalized;
}
