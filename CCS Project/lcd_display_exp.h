/*
 * lcd_display_exp.h
 *
 * exports from lcd_display.cpp to be used by tsl_asm.asm
 */

#ifndef LCD_DISPLAY_EXP_H_
#define LCD_DISPLAY_EXP_H_

// Word in lcd memory to write the seconds digits words
// (2 digits worth of segments in one word because we put all of the pins for these digits next to each other for efficiency)
extern unsigned int * lcdmem_secs_word;

// The words to be written to LCD memory for each of the 60 seconds digits
#define SECS_PER_MIN 60
extern unsigned int secs_lcd_words[];

#endif /* LCD_DISPLAY_EXP_H_ */
