// ===================================================================================
// Project:   TinyBling - One-Button Hunting Game
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
// In this simple one-button game, a hunter (represented by a green LED) chases a deer
// (represented by a red LED). The player must press the button at the exact moment
// when the hunter catches up to the deer. If the button is pressed too early or too
// late, the game is lost. After each successful catch, the hunter’s speed increases,
// making the game progressively more challenging as the player tries to maintain
// perfect timing. The objective is to see how many times the player can successfully
// catch the deer before missing.
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
#include <config.h>                   // user configurations
#include <system.h>                   // system functions
#include <gpio.h>                     // GPIO functions

// ===================================================================================
// NeoPixel Functions
// ===================================================================================

// NeoPixel color defines (24-bit color: 0x00bbrrgg)
#define NEO_BLACK       0x00000000
#define NEO_WHITE       0x003f3f3f
#define NEO_RED         0x00003f00
#define NEO_GREEN       0x0000003f
#define NEO_BLUE        0x003f0000
#define NEO_YELLOW      0x00003f3f
#define NEO_CYAN        0x003f003f
#define NEO_MAGENTA     0x003f3f00

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

// Send color to one pixel
void NEO_sendColor(uint32_t color) {
  NEO_sendByte(color); color >>= 8;
  NEO_sendByte(color); color >>= 8;
  NEO_sendByte(color);
}

// Fill all pixel with the same color
void NEO_fillColor(uint32_t color) {
  for(uint8_t i=0; i<NEO_COUNT; i++) NEO_sendColor(color);
}

// Init NeoPixel pin
void NEO_init(void) {
  PIN_output(PIN_NEO);
}

// ===================================================================================
// Game Functions
// ===================================================================================

// Global variables
uint8_t  hunter = 0;                            // current LED position of hunter
uint8_t  deer;                                  // current LED position of deer
uint8_t  dir;                                   // current direction of hunter
uint32_t speed;                                 // current delay between hunter steps
uint32_t end;                                   // next SysTick counter for hunter step

// Update game on NeoPixel display
void GAME_update(void) {
  uint32_t color;
  DLY_us(300);                                  // make sure last colors latched
  for(uint8_t i=0; i<NEO_COUNT; i++) {          // all LEDs:
    if(i == hunter)    color = NEO_GREEN;       // set green LED for hunter
    else if(i == deer) color = NEO_RED;         // set red LED for deer
    else               color = NEO_BLACK;       // turn LED off otherwise
    NEO_sendColor(color);                       // send color to LED
  }
  end = STK->CNT + speed;                       // calculate time for next hunter step
}

// Reset game
void GAME_reset(void) {
  DLY_us(300);                                  // make sure last colors latched
  NEO_fillColor(NEO_BLUE);                      // set all LEDs to blue
  DLY_ms(100);                                  // wait a bit
  deer  = (hunter + 4 + (STK->CNT & 7)) & 15;   // next position of deer
  dir   = !dir;                                 // revert hunter direction
  speed = GAME_SPEED_START * DLY_MS_TIME;       // set hunter speed for game start
  GAME_update();                                // update game display
}

// Main function
int main(void) {
  // Setup
  PIN_input_PU(PIN_KEY);                        // set button pin to input pullup
  NEO_init();                                   // init Neopixels
  GAME_reset();                                 // reset game parameters

  // Loop
  while(1) {
    if(((int32_t)(STK->CNT - end)) >= 0) {      // next step?
      if(hunter == deer) GAME_reset();          // missed deer? -> reset game
      else {
        if(dir) hunter++;                       // move hunter
        else hunter--;
        hunter &= 15;
        GAME_update();                          // update game display
      }
    }

    if(!PIN_read(PIN_KEY)) {                          // key pressed?
      if(hunter == deer) {                            // catched the deer?
        deer   = (hunter + 4 + (STK->CNT & 7)) & 15;  // next position of deer
        dir    = !dir;                                // revert hunter direction
        speed -= GAME_SPEED_INC * DLY_MS_TIME;        // increase hunter speed
        GAME_update();                                // update game display
      }
      else GAME_reset();                              // missed deer? -> reset game
      while(!PIN_read(PIN_KEY));                      // wait for key released
      DLY_ms(10);                                     // debounce key
    }
  }
}
