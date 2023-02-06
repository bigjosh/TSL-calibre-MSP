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

extern "C" {

    // Entry set vector to this to enter ready-to-launch mode on next interrupt
    // Assumes the symbol `ready_to_launch_lcd_frames` points to a table of LCD frames for the squiggle animation
    extern unsigned RTL_MODE_BEGIN; /* declare external asm function as an unsigned so we can easily stick it into the vector. */

    // Entry vector for time-since-launch mode
    // Assumes these symbols:
    // .ref    secs_lcd_words          ; - table of prerendered values to write to the seconds word in LCDMEM (one entry for each second 0-59)
    // .ref    secs                    ; - elapsed seconds
    extern unsigned TSL_MODE_BEGIN; /* declare external asm function as an unsigned so we can easily stick it into the vector. */

}

#endif /* TSL_ASM_H_ */
