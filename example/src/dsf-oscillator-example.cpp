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

    printf("\n\n\n\n\nDiscrete Summation Formula Oscillator v2.5 (2024-08-08)\n=======================================================\n\n");
    if (VERBOSE) printf("Clock Speed %d MHz\nStarting timer at %dµs, %d Hz\n\n", clock_get_hz(clk_sys) / 1000000, SAMPLE_INTERVAL, SAMPLE_RATE);

    // negative SAMPLE_INTERVAL results in evenly-spaced timer calls
    add_repeating_timer_us((-1 * SAMPLE_INTERVAL), &timerSample_cb, NULL, &timerSample); 

    while (true) tight_loop_contents();

}

bool timerSample_cb(repeating_timer_t *rt)
{
    uint16_t dacValue;
    fix15 param_A; 
    if (thisNote.active) {

        adc_select_input(adc_in_EnvSustain);
        envSustain = (fix15)(uscale(adc_read(), 0, 4095, param_a_min15, param_a_max15));

        switch (envMode)
        {
        case attack:
            adc_select_input(adc_in_EnvAttack);
            envAttack = uscale(adc_read(), 0, 4095, envRangeMin, envRangeMax);
            if (envCounter % envAttack == 0) envelope += envStep;
            envCounter++;
            if (envelope >= param_a_max15) {
                envMode = decay;
                envCounter = 0;
                if (VERBOSE) printf("\nAttack -> Decay\n\n");
            } 
            param_A = envInvert ? envelope : one15 - envelope;
            break;
        
        case decay:
            adc_select_input(adc_in_EnvDecay);
            envDecay = uscale(adc_read(), 0, 4095, envRangeMin, envRangeMax);
            if (envCounter % envDecay == 0) envelope -= envStep;
            envCounter++;
            if (envelope <= envSustain) {
                envMode = sustain;
                envCounter = 0;
                if (VERBOSE) printf("\nDecay -> Sustain\n\n");
            } 
            param_A = envInvert ? envelope : one15 - envelope;
            break;
        
        case sustain:
            param_A = envInvert ? envSustain : one15 - envSustain;
            break;
        
        default:
            break;
        }

        dacValue = osc.getNextSample(param_A);
        //if (VERBOSE) printf("%d, %d\n", dacValue, param_A);
        if (dacValue > 4095) return true;
        dac.setInputCode(dacValue);
    }

    return true;
}

void buttons_cb(uint gpio, uint32_t event_mask)
{
    if (VERBOSE) printf("GPIO interrupt %d = %d\n", gpio, gpio_get(gpio));
    int8_t val;
    switch (gpio)
    {
    case pinHarmonic:
        isHarmonic = !isHarmonic;
        gpio_put(pinStatusHarmonic, isHarmonic);
        break;
        
    case pinEnvInvert:
        envInvert = !envInvert;
        gpio_put(pinStatusEnvInvert, envInvert);
        break;
        
    case pinMult:
        multState = !multState;
        gpio_put(pinStatusMult, multState);
        gpio_put(PICO_DEFAULT_LED_PIN, multState);
        break;
    
    default:
        break;
    }
}

void setup()
{
    uint32_t mask_input = (1 << pinHarmonic) | (1 << pinMult) | (1 << pinEnvInvert);
    uint32_t mask_output = (1 << pinStatusHarmonic) | (1 << pinStatusEnvInvert) | (1 << pinStatusMult);

    gpio_init_mask(mask_input | mask_output);

    gpio_set_dir_in_masked(mask_input);
    gpio_set_dir_out_masked(mask_output);

    gpio_set_pulls(pinHarmonic, true, false);
    gpio_set_pulls(pinEnvInvert, true, false);
    gpio_set_pulls(pinMult, true, false);

    gpio_set_irq_enabled_with_callback(pinHarmonic, GPIO_IRQ_EDGE_FALL, true, &buttons_cb);
    gpio_set_irq_enabled(pinEnvInvert, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(pinMult, GPIO_IRQ_EDGE_FALL, true);

    gpio_put(pinStatusHarmonic, isHarmonic);
    gpio_put(pinStatusEnvInvert, envInvert);
    gpio_put(pinStatusMult, multState);

    adc_init();
    adc_gpio_init(pinEnvAttack);
    adc_gpio_init(pinEnvDecay);
    adc_gpio_init(pinEnvSustain);

    bool dac_valid = dac.begin(MCP4725A0_Addr_A00, i2c0, I2C_SPEED, pinSDA, pinSCL);
    if (dac_valid) {
        blinkLED(3);
    } else {
        blinkLED(0);
    }

    for (uint m = 0; m < 128; m++) midiFreq15[m] = float2fix15(midiFreq_Hz[m]);
    
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

uint32_t uscale(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max)
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
                envMode = attack;
                envelope = 0;
                envCounter = 0;
                fMod = multfix15(midiFreq15[thisNote.note], multfix15(modFactor15[multState], (isHarmonic ? one15 : root2)));
                osc.freqs(midiFreq15[thisNote.note], fMod, false);
                gpio_put(PICO_DEFAULT_LED_PIN, true);
                if (VERBOSE) printf("Note On: %d (%f Hz)\n      >>> Carrier = %f, Modulator = %f\n", thisNote.note, midiFreq_Hz[thisNote.note], fix2float15(midiFreq15[thisNote.note]), fix2float15(fMod));
                break;
            
            case 0x80:
                if (thisNote.note == lastNote.note) {
                    thisNote.active = false;
                    gpio_put(PICO_DEFAULT_LED_PIN, false);
                    if (VERBOSE) printf(">>>>>Note Off: %d\n", thisNote.note);
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

