// ===================================================================================
// User Configurations for TinyBling One-Button Hunting Game
// ===================================================================================
//
// In this simple one-button game, a hunter (represented by a green LED) chases a deer
// (represented by a red LED). The player must press the button at the exact moment
// when the hunter catches up to the deer. If the button is pressed too early or too
// late, the game is lost. After each successful catch, the hunter’s speed increases,
// making the game progressively more challenging as the player tries to maintain
// perfect timing. The objective is to see how many times the player can successfully
// catch the deer before missing.

#pragma once

// Pin definitions
#define PIN_NEO           PC1         // pin connected to NeoPixels
#define PIN_KEY           PC4         // pin connected to push button

// NeoPixel definitions
#define NEO_COUNT         16          // number of NeoPixels

// Game settings
#define GAME_SPEED_START  256         // Hunter step delay in ms at game start
#define GAME_SPEED_INC    8           // Hunter step delay increase in ms
