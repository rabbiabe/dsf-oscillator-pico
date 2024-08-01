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

#include "dsf-oscillator-example.h"

static volatile bool core1_booting = true;
static volatile bool core0_booting = true;

static uint8_t midi_dev_addr = 0;

int main()
{
    stdio_init_all();
    busy_wait_ms(3000);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    bool overclock = set_sys_clock_khz(250000, false);

    setup();

    board_init();
    printf("launch core1 (USB Host)\n");
    multicore_reset_core1();
    // all USB task run in core1
    multicore_launch_core1(core1_main);
    // wait for core 1 to finish claiming PIO state machines and DMA
    while(core1_booting) tight_loop_contents();
    core0_booting = false;
    printf(">>> both cores running\n");

    printf("\n\n\n\n\nDiscrete Summation Formula Oscillator v2.0 (2024-07-14)\n=======================================================\n\n");
    if (overclock) {
        printf("Overclock %d MHz achieved!\n", clock_get_hz(clk_sys) / 1000000);
        //blinkLED(5);
    } else {
        printf("Overclock failed :( current clock %d MHz\n", clock_get_hz(clk_sys) / 1000000);
        //blinkLED(3);
    }


    add_repeating_timer_us(SAMPLE_INTERVAL, &timerSample_cb, NULL, &timerSample);
    //add_repeating_timer_us(LFO_INTERVAL, &timerLFO_cb, NULL, &timerLFO);
                      
    while (true) {
        static uint8_t last_rate = 0, last_center = 0;
        //static uint32_t loopCounter = 0;

        gpio_put(pinStatusLoop, !gpio_get(pinStatusLoop));

    /*
        adc_select_input(adc_in_LfoRate);
        uint16_t this_rate = adc_read();
        if (last_rate != (this_rate >> 3)) {
            last_rate = (this_rate >> 3);
            uint16_t lfo_rate_mHz = scale(this_rate, 0, 4095, 100, 10000);
            lfoStep = (lfo_rate_mHz * two32) / 100000;
        } 
    */

        adc_select_input(adc_in_Center);
        uint16_t this_center = adc_read();
        if (last_center != (this_center >> 3)) {
            last_center = (this_center >> 3);
            param_center = scale(this_center, 0, 4095, 100, 900);
            //printf("  > param_center %+d\n", param_center);
        }

        //if (loopCounter++ % 100 == 0) printf("    > LFO %d // Normalize %d // Harmonic %d\n", lfo_active, isNormalized, isHarmonic);

        busy_wait_ms(200);

    }

}

bool timerLFO_cb(repeating_timer_t *rt)
{
    lfoCounter += lfoStep;
    lfoValue = lfo_table_sine[lfoCounter >> 24];
    return true;
}

bool timerSample_cb(repeating_timer_t *rt)
{
    static uint32_t timerSampleCounter = 0;
    if (thisNote.active) {
        //uint16_t param = lfo_active ? param_center + lfoValue : param_center;
        uint16_t param =  param_center;
        uint16_t dacValue = osc.getNextSample(param);
        if (dacValue > 4095) return true;
        printf("%u,%u\n", timerSampleCounter++, dacValue);
        dac.setInputCode(dacValue);
    }

    return true;
}

void buttons_cb(uint gpio, uint32_t event_mask)
{
    printf("GPIO interrupt %d = %d\n", gpio, gpio_get(gpio));
    switch (gpio)
    {
    case pinHarmonic:
        isHarmonic = !isHarmonic;
        gpio_put(pinStatusHarmonic, isHarmonic);
        break;
    
    case pinNormalize:
        isNormalized = !isNormalized;
        osc.normalize(isNormalized);
        gpio_put(pinStatusNormalize, isNormalized);
        break;
    
    case pinLfoToggle:
        lfo_active = !lfo_active;
        gpio_put(pinStatusLFO, lfo_active);
        break;
    
    default:
        break;
    }
}

void setup()
{
    gpio_init(pinNormalize);
    gpio_init(pinHarmonic);
    gpio_init(pinLfoToggle);

    gpio_set_dir(pinNormalize, GPIO_IN);
    gpio_set_dir(pinHarmonic, GPIO_IN);
    gpio_set_dir(pinLfoToggle, GPIO_IN);

    gpio_set_pulls(pinNormalize, true, false);
    gpio_set_pulls(pinHarmonic, true, false);
    gpio_set_pulls(pinLfoToggle, true, false);

    gpio_set_irq_enabled_with_callback(pinNormalize, GPIO_IRQ_EDGE_FALL, true, &buttons_cb);
    gpio_set_irq_enabled(pinHarmonic, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(pinLfoToggle, GPIO_IRQ_EDGE_FALL, true);

    gpio_init(pinStatusHarmonic);
    gpio_init(pinStatusLFO);
    gpio_init(pinStatusNormalize);
    gpio_init(pinStatusLoop);
    gpio_set_dir(pinStatusHarmonic, GPIO_OUT);
    gpio_set_dir(pinStatusLFO, GPIO_OUT);
    gpio_set_dir(pinStatusNormalize, GPIO_OUT);
    gpio_set_dir(pinStatusLoop, GPIO_OUT);
    gpio_put(pinStatusHarmonic, isHarmonic);
    gpio_put(pinStatusLFO, lfo_active);
    gpio_put(pinStatusNormalize, isNormalized);
    gpio_put(pinStatusLoop, false);

    adc_init();
    adc_gpio_init(pinCenter);
    adc_gpio_init(pinLfoRate);
    adc_gpio_init(pinFmod);

    bool dac_valid = dac.begin(MCP4725A0_Addr_A00, i2c0, I2C_SPEED, pinSDA, pinSCL);

    if (dac_valid) {
        blinkLED(3);
    } else {
        blinkLED(0);
    }

    osc.normalize(isNormalized);

    for (uint m = 0; m < 128; m++) {
        midiFreq15[m] = float2fix15(midiFreq_Hz[m]);
    }

}

void blinkLED(uint8_t count)
{
    if (count == 0) {
        while (true) {
            gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
            busy_wait_ms(150);
        }
    }
    for (uint b = 0; b < count; b++) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        busy_wait_ms(333);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        busy_wait_ms(333);
    }    
}

int32_t scale(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



/**************************************************
 * Code below adapted from usb_midi_host_pio_example.c 
 * included with the usb_midi_host library: 
 * https://github.com/rppicomidi/usb_midi_host
 *************************************************/

void core1_main() 
{
    busy_wait_ms(10);
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    // Use GP16 for USB D+ and GP17 for USB D-
    pio_cfg.pin_dp = 16;
    // Swap PIOs from default. The RX state machine takes up the
    // whole PIO program memory. Without these two lines, if you
    // try to use this code on a Pico W board, the CYW43 SPI PIO
    // code, which runs on PIO 1, won't fit.
    // Other potential conflict is the DMA channel tx_ch. However,
    // the CYW43 SPI driver code is not hard-wired to any particular
    // DMA channel, so as long as tuh_configure() and tuh_ini()run
    // after board_init(), which also calls tuh_configure(), and before
    // cyw43_arch_init(), there should be no conflict.
    pio_cfg.pio_rx_num = 0;
    pio_cfg.pio_tx_num = 1;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(BOARD_TUH_RHPORT);
    core1_booting = false;
    while(core0_booting) {
    }
    while (true) {
        tuh_task(); // tinyusb host task
    }
}

void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
  printf("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
      dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);

  if (midi_dev_addr == 0) {
    // then no MIDI device is currently connected
    midi_dev_addr = dev_addr;
  } else {
    printf("A different USB MIDI Device is already connected.\r\nOnly one device at a time is supported in this program\r\nDevice is disabled\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr == midi_dev_addr) {
    midi_dev_addr = 0;
    printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  } else {
    printf("Unused MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  }
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
    if (midi_dev_addr == dev_addr) {
        
        if (num_packets != 0) {
            fix15 fNote, fMod, modFreq, modFactor, modBase, modHarm;
            uint8_t cable_num;
            uint8_t buffer[48];
            while (true) {
                uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
                if (bytes_read == 0) break;
                thisNote.command = buffer[0] & 0xF0; // ignore MIDI channel
                thisNote.note = buffer[1];
                thisNote.velocity = buffer[2];
            }

            switch (thisNote.command)
            {
            case 0x90:
                lastNote = thisNote;
                thisNote.active = true;
                lfoCounter = 0;
                adc_select_input(adc_in_Fmod);
                fMod = multfix15(midiFreq15[thisNote.note], multfix15(((adc_read() < 2048) ? half15 : two15), (isHarmonic ? one15 : root2)));
                if (thisNote.note < 116) {
                    osc.freqs(multfix15(midiFreq15[thisNote.note], ten15), multfix15(fMod, ten15), true);
                } else {
                    osc.freqs(midiFreq15[thisNote.note], fMod, false);
                }             
                gpio_put(PICO_DEFAULT_LED_PIN, true);
                printf("Note On: %d (%f Hz)\n      >>> Carrier = %f, Modulator = %f\n", thisNote.note, midiFreq_Hz[thisNote.note], fix2float15(midiFreq15[thisNote.note]), fix2float15(fMod));
                break;
            
            case 0x80:
                if (thisNote.note == lastNote.note) {
                    thisNote.active = false;
                    gpio_put(PICO_DEFAULT_LED_PIN, false);
                    printf(">>>>>Note Off: %d\n", thisNote.note);
                }
                break;
            
            default:
                break;
            }
        }
        
    }
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}
