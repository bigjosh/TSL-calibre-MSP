/*
 * lcd_display.h
 *
 *  Created on: Jan 20, 2023
 *      Author: passp
 */

#ifndef LCD_DISPLAY_H_
#define LCD_DISPLAY_H_

#include "util.h"

// *** LCD layout

constexpr uint8_t DIGITPLACE_COUNT=12;

// these arrays hold the pre-computed words that we will write to word in LCD memory that
// controls the seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.

#define SECS_PER_MIN 60

#define MINS_PER_HOUR 60


const byte READY_TO_LAUNCH_LCD_FRAME_COUNT=8;                             // How many frames in the ready-to-launch mode animation

// Fills the arrays

void initLCDPrecomputedWordArrays();

// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show();
// Show the digit x at position p
// where p=0 is the rightmost digit


inline void lcd_show_fast_secs( uint8_t secs );

void lcd_show_digit_f( const uint8_t pos, const byte d );

void lcd_show_testing_only_message();

// For now, show all 9's.
// TODO: Figure out something better here

void lcd_show_long_now();


// Squigle segments are used during the Ready To Launch mode animation before the pin is pulled
constexpr unsigned SQUIGGLE_ANIMATION_FRAME_COUNT = 8;          // There really should be a better way to do this.

// Show a frame n the ready-to-lanuch squiggle animation
// 0 <= step < SQUIGGLE_ANIMATION_FRAME_COUNT

void lcd_show_squiggle_frame( byte step );


// Fill the screen with horizontal dashes
void lcd_show_dashes();



// The lance is 2 digitplaces wide so we need extra frames to show it coming onto and off of the screen
constexpr unsigned LANCE_ANIMATION_FRAME_COUNT = DIGITPLACE_COUNT+3;          // There really should be a better way to do this.

// show a little dash sliding towards the trigger
void lcd_show_lance( byte step );


// Show the message "Error X" on the lcd.

void lcd_show_errorcode( byte code  );


#endif /* LCD_DISPLAY_H_ */