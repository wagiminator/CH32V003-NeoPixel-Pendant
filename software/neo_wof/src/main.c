// ===================================================================================
// Project:   TinyBling - Whhel of Fortune
// Version:   v1.0
// Year:      2024
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// A simple wheel of fortune on the TinyBling. Press the button and look forward to
// the result!
//
// References:
// -----------
// - ATtiny13A TinyBling: https://github.com/wagiminator/ATtiny13-TinyBling
// - Adafruit LED Tricks: https://learn.adafruit.com/led-tricks-gamma-correction
// - CNLohr ch32v003fun:  https://github.com/cnlohr/ch32v003fun
// - WCH Nanjing Qinheng Microelectronics: http://wch.cn
//
// Compilation Instructions:
// -------------------------
// - Make sure GCC toolchain (gcc-riscv64-unknown-elf, newlib) and Python3 with rvprog
//   are installed. In addition, Linux requires access rights to WCH-LinkE programmer.
// - Connect the WCH-LinkE programmer to the MCU board.
// - Run 'make flash'.


// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================
#include <config.h>             // user configurations
#include <system.h>             // system functions
#include <gpio.h>               // GPIO functions

// ===================================================================================
// NeoPixel Functions
// ===================================================================================

// Define some constants depending on the NeoPixel pin
#define NEO_GPIO_BASE \
  ((PIN_NEO>=PA0)&&(PIN_NEO<=PA7) ? ( GPIOA_BASE ) : \
  ((PIN_NEO>=PC0)&&(PIN_NEO<=PC7) ? ( GPIOC_BASE ) : \
  ((PIN_NEO>=PD0)&&(PIN_NEO<=PD7) ? ( GPIOD_BASE ) : \
(0))))
#define NEO_GPIO_BSHR   0x10
#define NEO_GPIO_BCR    0x14
#define NEO_PIN_BM      (1<<((PIN_NEO)&7))

// This is the most time sensitive part. Outside of the function, it must be 
// ensured that interrupts are disabled and that the time between the 
// transmission of the individual bytes is less than the pixel's latch time.

// Send one data byte to the pixels string (works at 8MHz CPU frequency)
void NEO_sendByte(uint8_t data) {
  asm volatile(
    " c.li a5, 8                \n"   // 8 bits to shift out (bit counter)
    " li a4, %[pin]             \n"   // neopixel pin bitmap (compressed for pins 0-4)
    " li a3, %[base]            \n"   // GPIO base address   (single instr for port C)
    "1:                         \n"
    " andi a2, %[byte], 0x80    \n"   // mask bit to shift (MSB first)
    " c.sw a4, %[bshr](a3)      \n"   // set neopixel pin HIGH
    " c.bnez a2, 2f             \n"   // skip next instruction if bit = "1"
    " c.sw a4, %[bcr](a3)       \n"   // bit = "0" : set pin LOW after <= 500ns
    "2:                         \n"
    " c.nop                     \n"   // delay
    " c.sw a4, %[bcr](a3)       \n"   // bit = "1" : set pin LOW after >= 625ns
    " c.slli %[byte], 1         \n"   // shift left for next bit
    " c.addi a5, -1             \n"   // decrease bit counter
    " c.bnez a5, 1b             \n"   // repeat for 8 bits
    :
    [byte] "+r" (data)
    :
    [pin]  "i"  (NEO_PIN_BM),
    [base] "i"  (NEO_GPIO_BASE),
    [bshr] "i"  (NEO_GPIO_BSHR),
    [bcr]  "i"  (NEO_GPIO_BCR)
    :
    "a2", "a3", "a4", "a5", "memory"
  );
}

// Write color to a single pixel
void NEO_writeColor(uint8_t r, uint8_t g, uint8_t b) {
  NEO_sendByte(g); NEO_sendByte(r); NEO_sendByte(b);
}

// Write hue value (0..191) to a single pixel
void NEO_writeHue(uint8_t hue) {
  uint8_t phase = hue >> 6;
  uint8_t step  = (hue & 63) << 2;
  uint8_t nstep = (63 << 2) - step;
  switch(phase) {
    case 0:   NEO_writeColor(nstep,  step,     0); break;
    case 1:   NEO_writeColor(    0, nstep,  step); break;
    case 2:   NEO_writeColor( step,     0, nstep); break;
    default:  break;
  }
}

// Set a single pixel and clear the others
void NEO_setPixel(uint8_t nr, uint8_t hue) {
  for(uint8_t i=0; i<NEO_COUNT; i++) {
    (i==nr) ? (NEO_writeHue(hue)) : (NEO_writeColor(0,0,0));
  }
}

// Init NeoPixel pin
void NEO_init(void) {
  PIN_output(PIN_NEO);
  DLY_us(300);
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void) {
  // Local variables
  uint8_t number = 0;                         // current LED number
  uint8_t speed;                              // current speed of wheel
  uint8_t hue = 0;                            // current hue

  // Setup
  PIN_input_PU(PIN_KEY);                      // set button pin to input pullup
  PIN_EVT_set(PIN_KEY, PIN_EVT_FALLING);      // set pin change event for button pin
  NEO_init();                                 // init Neopixels
  NEO_setPixel(number, hue);                  // set start pixel

  // Loop
  while(1) {    
    SLEEP_WFE_now();                          // put MCU to sleep, wake up by event
    if(!PIN_read(PIN_KEY)) {                  // if button pressed:
      speed = 16 + (STK->CNT & 15);           // set start speed randomly
      while(--speed) {                        // increase speed
        if(++hue > 191) hue = 0;              // next hue value
        if(++number >= NEO_COUNT) number = 0; // next pixel number
        NEO_setPixel(number, hue);            // set next pixel
        DLY_ms(speed);                        // delay
      }
      while(++speed < 96) {
        if(++hue > 191) hue = 0;              // next hue value
        if(++number >= NEO_COUNT) number = 0; // next pixel number
        NEO_setPixel(number, hue);            // set next pixel
        DLY_ms(speed);                        // delay
      }
      while(!PIN_read(PIN_KEY));              // wait for button released
      DLY_ms(10);                             // debounce
    }
  }
}
