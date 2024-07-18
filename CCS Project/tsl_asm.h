/*
 * tsl-asm.h
 *
 *  The TSL spends nearly all of its life in these functions, so we optimize them in assembly.
 *  This header holds the entry points for the ISRs for ready-to-launch and time-since-lanuch
 *  modes. To enter either mode, set the ISR vector to point to one of these functions and then the
 *  next time the interrupt fires, that function will set everything up for that mode and then reassign
 *  the vector to point to the long-term ISR.
 *
 */

#ifndef TSL_ASM_H_
#define TSL_ASM_H_


// Entry set vector to this to enter ready-to-launch mode on next interrupt
// Assumes the symbol `ready_to_launch_lcd_frames` points to a table of LCD frames for the squiggle animation
extern unsigned RTL_MODE_BEGIN; /* declare external asm function as an unsigned so we can easily stick it into the vector. */



// Entry vector for time-since-launch mode
// Assumes these symbols:
// .ref    secs_lcd_words          ; - table of prerendered values to write to the seconds word in LCDMEM (one entry for each second 0-59)
// .ref    secs                    ; - elapsed seconds
extern unsigned TSL_MODE_BEGIN; /* declare external asm function as an unsigned so we can easily stick it into the vector. */

// These variables are passed into TSL_MODE_BEGIN
// Note that these variables are not updated, they are only used for initialization
extern unsigned tsl_hours;
extern unsigned tsl_mins;
extern unsigned tsl_secs;

extern unsigned *persistant_mins;   // Pointer to the word in FRAM that holds the current number of elapsed minutes.
                                    // The asm code does not need the days variable because there are only 1440 mins in a day and it calls back
                                    // to C on each day rollover.


// Note that I could not get this function prototype to work with the assembler. It chokes on the etern "C" which unmangles the function name. :/
// Instead we must add a ref in the ASM file. :(

//extern "C" void tsl_new_day();      // ASM calls this each time the day rolls over. It is expected to update the day on the LCD and also
                                    // atomically update the persistent mins and days counter.


// Here are the tables for values to write the the LCD control to display digits

extern unsigned secs_lcd_words[];          // table of prerendered values to write to the seconds word in LCDMEM (one entry for each second 0-59)
extern unsigned mins_lcd_words[];          // table of prerendered values to write to the minutes word in LCDMEM (one entry for each min 0-59)
extern unsigned char hours_lcd_bytes[];         // Unfortunately it was not possible to layout the PCB get both hours digits in the same MEMWORD, so we have to do each digit byte separately.


extern unsigned *ready_to_launch_lcd_frame_words;        // A complicated 2D table of words that we write to LCDMEM for the frames of the ready-to-launch animation

#endif /* TSL_ASM_H_ */
