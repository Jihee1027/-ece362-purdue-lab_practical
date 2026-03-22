#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "support.h"
#include "hardware/xosc.h"

// Base library headers included for your convenience.
// ** You may have to add more depending on your practical. **
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"

void grader();

//////////////////////////////////////////////////////////////////////////////

// Make sure to set your pins if you are using this on your own breadboard.
// For the Platform Test Board, these are the correct pin numbers.
// const int SPI_7SEG_SCK = 14;
// const int SPI_7SEG_CSn = 13;
// const int SPI_7SEG_TX = 15;
const int SPI_7SEG_SCK = 18;
const int SPI_7SEG_CSn = 17;
const int SPI_7SEG_TX = 19;
// NOT NEEDED since we are not using LCD/OLED in this practical.
// But it needs to be defined to avoid compiler errors.
const int SPI_DISP_SCK = -1;
const int SPI_DISP_CSn = -1;
const int SPI_DISP_TX = -1;

// Remember this from ECE 270?
// Verilog borrowed this from C.
typedef enum {
    INIT = 0,
    READY = 1,
    REACT = 2,
    RESULT = 3
} reactor_state_t;
reactor_state_t reactor_state = INIT;

// Track which button was last pressed.
int last_pressed = -1;

//////////////////////////////////////////////
// Step 1
void reaction_isr() {
    // fill in
    uint32_t ev21 = gpio_get_irq_event_mask(21);
    uint32_t ev26 = gpio_get_irq_event_mask(26);

    if (ev21 & GPIO_IRQ_EDGE_RISE) {
       gpio_acknowledge_irq(21, GPIO_IRQ_EDGE_RISE);
       last_pressed = 21;
       reactor_state = RESULT;
    }

    if (ev26 & GPIO_IRQ_EDGE_RISE) {
       gpio_acknowledge_irq(26, GPIO_IRQ_EDGE_RISE);
       last_pressed = 26;
       reactor_state = RESULT;
    }
}

void init_reaction_irq() {
    // fill in
    // input
    gpio_init(21);                 
    gpio_set_dir(21, GPIO_IN);     
    gpio_set_function(21, GPIO_FUNC_SIO);
    gpio_init(26);                 
    gpio_set_dir(26, GPIO_IN);     
    gpio_set_function(26, GPIO_FUNC_SIO);

    // output
    gpio_init(37);                 
    gpio_set_dir(37, GPIO_OUT);    
    gpio_set_function(37, GPIO_FUNC_SIO);
    gpio_init(38);                 
    gpio_set_dir(38, GPIO_OUT);    
    gpio_set_function(38, GPIO_FUNC_SIO);
    gpio_init(39);                 
    gpio_set_dir(39, GPIO_OUT);    
    gpio_set_function(39, GPIO_FUNC_SIO);

    gpio_put(37, 1);
    gpio_put(38, 1);
    gpio_put(39, 1);

    // To ISR
    uint32_t mask = (1u << 21) | (1u << 26);
    gpio_add_raw_irq_handler_masked(mask, reaction_isr);

    // Rising edge IRQ
    gpio_set_irq_enabled(21, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(26, GPIO_IRQ_EDGE_RISE, true);

    // Dormant Wake up IRQ
    gpio_set_dormant_irq_enabled(26, GPIO_IRQ_EDGE_RISE, true);

    // IRQ enable
    irq_set_enabled(IO_IRQ_BANK0, true);
}

//////////////////////////////////////////////
//// Step 2. 

uint16_t __attribute__((aligned(16))) message[8] = {
    (0 << 8) | 0x3F,    // seven-segment value of 0
    (1 << 8) | 0x06,    // seven-segment value of 1
    (2 << 8) | 0x5B,    // seven-segment value of 2
    (3 << 8) | 0x4F,    // seven-segment value of 3
    (4 << 8) | 0x66,    // seven-segment value of 4
    (5 << 8) | 0x6D,    // seven-segment value of 5
    (6 << 8) | 0x7D,    // seven-segment value of 6
    (7 << 8) | 0x07,    // seven-segment value of 7
};
// 7-segment display font mapping
extern char font[];

void sevenseg_display(const char* str) {
    // fill in
    for (int i = 0; i < 8; i++) {
        char c = str[i];
        if (c == '\0') c = ' ';
        message[i] = ((uint16_t)i << 8) | (uint8_t)font[(uint8_t)c];
    }
}

void init_sevenseg_spi() {
    // fill in
    gpio_set_function(SPI_7SEG_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_7SEG_TX,  GPIO_FUNC_SPI);
    gpio_set_function(SPI_7SEG_CSn, GPIO_FUNC_SPI);
    spi_init(spi0, 125000);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

//////////////////////////////////////////////
//// Step 3. 

uint32_t last_set_time = 0;

void timer_isr() {
    // fill in 
    // clear TIMER0 alarm 0 interrupt
    timer_hw->intr = 1u << 0;
    // set state to RESULT
    reactor_state = RESULT;  
}

void init_reaction_timer() {
    // fill in
    // enable TIMER0 alram 0 interrupt
    timer_hw->inte |= 1u << 0;
    // register hadler only once
    if (!irq_is_enabled(TIMER0_IRQ_0)) {
        irq_set_exclusive_handler(TIMER0_IRQ_0, timer_isr);
    }
    // enalbe TIMER0 IRQ in NVIC
    irq_set_enabled(TIMER0_IRQ_0, true);
    // store current time in last_set_time
    last_set_time = timer_hw->timerawl;
    // one-shot alarm 1 second later
    timer_hw->alarm[0] = last_set_time + 1000000;
}

//////////////////////////////////////////////
//// Step 4. 

void init_sevenseg_dma() {
    // fill in
    dma_channel_hw_t *ch = &dma_hw->ch[1];
    ch->read_addr = (uintptr_t)message;
    ch->write_addr = (uintptr_t)&spi0_hw->dr;
    // Set transfer count to 8 with TRIGGER_SELF mode (mode=1) for infinite repeat
    ch->transfer_count = (1u << DMA_CH0_TRANS_COUNT_MODE_LSB) | 8;
    
    uint32_t temp = 0;

    // 16 bit transfers
    temp |= DMA_SIZE_16 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;

    // increment read only
    temp |= DMA_CH0_CTRL_TRIG_INCR_READ_BITS;

    // ring on read addr, wrap every 16 bytes => 4 bits (2^4 = 16 bytes)
    temp |= DMA_CH0_CTRL_TRIG_RING_SEL_BITS;
    temp |= 4u << DMA_CH0_CTRL_TRIG_RING_SIZE_LSB;

    // SPI0 TX DREQ
    temp |= (DREQ_SPI0_TX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);

    // enable
    temp |= DMA_CH0_CTRL_TRIG_EN_BITS;
    ch->ctrl_trig = temp;
}

//////////////////////////////////////////////////////////////////////////////

// There is no need to modify main() unless you want to add debugging output.

// The only exception may be to disable the grader() call if you think it's 
// interfering with your functionality, somehow.

int main()
{
    stdio_init_all();
    // grader();
    init_reaction_irq();
    init_sevenseg_spi();
    init_sevenseg_dma();

    for(;;) {
        sevenseg_display("READY? ");
        sio_hw->gpio_hi_clr = (1 << (38 - 32)); // blue on
        int seed = get_rand_32() % 5;
        // between 2 and 7 seconds.
        sleep_ms(2000 + (seed * 1000));

        if (last_pressed != -1) {
            // they messed up and pressed button too early
            sevenseg_display("FAIL   !");
            sio_hw->gpio_hi_set = (1 << (38 - 32)); // blue off
            sio_hw->gpio_hi_clr = (1 << (37 - 32)); // red on
            sleep_ms(3000);
            sio_hw->gpio_hi_set = (1 << (37 - 32)); // red off
            last_pressed = -1;
            reactor_state = READY;
            continue;
        }

        // set the pushbutton wanted and then change state
        int want = (get_rand_32() % 2) ? 21 : 26;
        sio_hw->gpio_hi_set = (1 << (38 - 32)); // blue on
        reactor_state = REACT;
        if (want == 26) {
            sevenseg_display("PRESS  L");
        } else {
            sevenseg_display("PRESS  R");
        }
        init_reaction_timer();

        // wait for pushbutton or timer interrupt
        while (reactor_state != RESULT);

        // At this point, either pushbuttons or timer woke us up.
        // Find out what.
        if (reactor_state == RESULT) {
            // correct button
            if (last_pressed == want) {
                uint64_t now = timer_hw->timerawl;

                sevenseg_display("PASS");
                sio_hw->gpio_hi_clr = (1 << (39 - 32)); // green on
                sleep_ms(1000);
                
                // show diff in time
                float diff = (now - last_set_time) / 1000.00;
                char buf[9];
                snprintf(buf, sizeof(buf), "%3.3f", diff);
                sevenseg_display(buf);
                sleep_ms(2000);
            }
            else {
                sevenseg_display("FAIL");
                sio_hw->gpio_hi_clr = (1 << (37 - 32)); // red on 
            }
        }
        sleep_ms(3000);
        sio_hw->gpio_hi_set = (1 << (37 - 32)) | (1 << (39 - 32)); // red and green off
        last_pressed = -1;
        reactor_state = READY;
    }

    for(;;);
    return 0;
}