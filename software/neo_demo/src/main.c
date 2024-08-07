// ===================================================================================
// Project:   TinyBling - Animation Demo
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
// Various decorative light animations using the TinyBling's NeoPixels.
// The device automatically switches between the various animations after a defined 
// time interval. However, if the button is held down during power-up, the switching 
// occurs with each button press.
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

// NeoPixel buffer
uint8_t NEO_hue[NEO_COUNT];
uint8_t NEO_bright[NEO_COUNT];

// Gamma correction table
const uint8_t NEO_gamma[] = {
    0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   3,   4,   5,
    6,   7,   8,  10,  11,  13,  14,  16,  18,  20,  22,  25,  27,  30,  33,  36,
   39,  43,  47,  50,  55,  59,  63,  68,  73,  78,  83,  89,  95, 101, 107, 114,
  120, 127, 135, 142, 150, 158, 167, 175, 184, 193, 203, 213, 223, 233, 244, 255
};

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

// Write buffer to pixels
void NEO_show(void) {
  for(uint8_t i=0; i<NEO_COUNT; i++) {
    uint8_t phase = NEO_hue[i] >> 6;
    uint8_t step  = NEO_hue[i] & 63;
    uint8_t col   = NEO_gamma[step >> (6 - NEO_bright[i])];
    uint8_t ncol  = NEO_gamma[(63 - step) >> (6 - NEO_bright[i])];
    switch(phase) {
      case 0:   NEO_writeColor(ncol,  col,     0); break;
      case 1:   NEO_writeColor(    0, ncol,  col); break;
      case 2:   NEO_writeColor( col,     0, ncol); break;
      default:  break;
    }
  }
}

// Init NeoPixel pin
#define NEO_init()  PIN_output(PIN_NEO)

// ===================================================================================
// NeoPixel Animation Functions
// ===================================================================================

// Set color to a single pixel
void NEO_set(uint8_t number, uint8_t hue) {
  NEO_bright[number] = 6;
  NEO_hue[number] = hue;
}

// Clear all pixels
void NEO_clear(void) {
  for(uint8_t i=0; i<NEO_COUNT; i++) NEO_bright[i] = 0;
}

// Fill all pixel with the same color
void NEO_fill(uint8_t hue) {
  for(uint8_t i=0; i<NEO_COUNT; i++) NEO_hue[i] = hue;
}

// Fade in all pixels one step
void NEO_fadeIn(void) {
  for(uint8_t i=0; i<NEO_COUNT; i++) {
    if(NEO_bright[i] < 6) NEO_bright[i]++;
  }
}

// Fade out all pixels one step
void NEO_fadeOut(void) {
  for(uint8_t i=0; i<NEO_COUNT; i++) {
    if(NEO_bright[i]) NEO_bright[i]--;
  }
}

// Circle all pixels clockwise
void NEO_cw(void) {
  uint8_t btemp = NEO_bright[NEO_COUNT-1];
  uint8_t htemp = NEO_hue[NEO_COUNT-1];
  for(uint8_t i=NEO_COUNT-1; i; i--) {
    NEO_bright[i] = NEO_bright[i-1];
    NEO_hue[i]    = NEO_hue[i-1];
  }
  NEO_bright[0] = btemp;
  NEO_hue[0] = htemp;
}

// Circle all pixels counter-clockwise
void NEO_ccw(void) {
  uint8_t btemp = NEO_bright[0];
  uint8_t htemp = NEO_hue[0];
  for(uint8_t i=0; i<NEO_COUNT-1; i++) {
    NEO_bright[i] = NEO_bright[i+1];
    NEO_hue[i] = NEO_hue[i+1];
  }
  NEO_bright[NEO_COUNT-1] = btemp;
  NEO_hue[NEO_COUNT-1] = htemp;
}

// ===================================================================================
// Pseudo Random Number Generator
// ===================================================================================
uint32_t prng(uint32_t max) {
  static uint32_t rnval = 0xDEADBEEF;
  rnval = rnval << 16 | (rnval << 1 ^ rnval << 2) >> 16;
  return(rnval % max);
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void) {
  // Local variables
  uint8_t state = 0;                    // animation state variable
  uint8_t counter = NEO_AUTO_COUNT;     // animation state duration
  uint8_t hue1, hue2, ptr1, ptr2;       // animation parameters
  uint8_t mode;                         // automatic/button mode

  // Setup
  PIN_input_PU(PIN_KEY);                // set button pin to input pullup
  NEO_init();                           // init NeoPixels
  AWU_start(NEO_REFRESH);               // start automatic wake-up timer
  mode = !PIN_read(PIN_KEY);            // read button on start-up and set mode

  // Loop
  while(1) {
    // Animate
    switch(state) {
      case 0:   hue1 = 0; hue2 = 96; ptr1 = 0; ptr2 = NEO_COUNT>>1; state++;
                break;
                
      case 1:   NEO_fadeOut(); hue1 += 4; hue2 += 4;
                if(hue1 > 191) hue1 -= 192;
                if(hue2 > 191) hue2 -= 192;
                if(++ptr1 >= NEO_COUNT) ptr1 = 0;
                if(++ptr2 >= NEO_COUNT) ptr2 = 0;
                NEO_set(ptr1, hue1); NEO_set(ptr2, hue2); NEO_show();
                break;
             
      case 2:   NEO_fadeOut();
                for(uint8_t i=prng(4); i; i--) NEO_set(prng(NEO_COUNT), prng(192));
                NEO_show();
                break;

      case 3:   for(uint8_t i=0; i<NEO_COUNT; i++) NEO_set(i, prng(192));
                state++; NEO_show();
                break;

      case 4:   for(uint8_t i=0; i<NEO_COUNT; i++) {
                  NEO_hue[i] += prng(8); 
                  if(NEO_hue[i] > 191) NEO_hue[i] -= 192;
                }
                NEO_show();
                break;
                
      case 5:   hue1 = 0;
                for(uint8_t i=0; i<NEO_COUNT; i++, hue1+=192/NEO_COUNT) NEO_set(i, hue1);
                state++; NEO_show();
                break;
                
      case 6:   NEO_cw(); NEO_show();
                break;

      case 7:   hue1 += 3; if(hue1 > 191) hue1 -= 192;
                NEO_fill(hue1); NEO_show();
                break;
                
      default:  break;
    }

    if(mode) {                      // animation switching on button press
      if(!PIN_read(PIN_KEY)) {
        state++;
        while(!PIN_read(PIN_KEY));
      }
    }
    else {                          // atomatic animation switching
      if(!(--counter)) {
        counter = NEO_AUTO_COUNT;
        state++;
      }
    }
    if(state > 7) state = 0;
    STDBY_WFE_now();                // go to standby, wake up by AWU after defined period
  }
}
