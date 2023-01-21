/*
 * lcd_display.cpp
 *
 *  Created on: Jan 20, 2023
 *      Author: josh
 */


// Needed for LCDMEM addresses
#include <msp430.h>


#include "lcd_display.h"


// *** LCD layout

// These values come straight from pin listing the LCD datasheet combined with the MSP430 LCD memory map
// The 1 bit means that the segment + common attached to that pin will lite.
// Remember that COM numbering on the LCD datasheet starts at COM1 while MSP430 numbering starts at COM0
// And note that the COM order of the E-F-G segments are scrambled!

#define COM0_BIT (0b00000001)
#define COM1_BIT (0b00000010)
#define COM2_BIT (0b00000100)
#define COM3_BIT (0b00001000)

// Low pins
#define SEG_A_COM_BIT (COM0_BIT)
#define SEG_B_COM_BIT (COM1_BIT)
#define SEG_C_COM_BIT (COM2_BIT)
#define SEG_D_COM_BIT (COM3_BIT)

// High pins
#define SEG_E_COM_BIT (COM2_BIT)
#define SEG_F_COM_BIT (COM0_BIT)
#define SEG_G_COM_BIT (COM1_BIT)
#define SEG_S_COM_BIT (COM3_BIT)        // S segments include the low batt, H, M, and S indicators


typedef byte nibble;        // Keep nibbles semantically different just for clarity
#define NIBBLE_MAX ((1<<4)-1)

// Now we draw the digits using the segments as mapped in the LCD datasheet

struct glyph_segment_t {
    const nibble nibble_a_thru_d;     // The COM bits for the digit's A-D segments
    const nibble nibble_e_thru_g;     // The COM bits for the digit's E-G segments
};


constexpr glyph_segment_t digit_segments[NIBBLE_MAX+1] = {

    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }, // "0"
    {                SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                                               0}, // "1" (no high pin segments lit in the number 1)
    {SEG_A_COM_BIT | SEG_B_COM_BIT |                 SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "2"
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                                 SEG_G_COM_BIT  }, // "3"
    {                SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "4"
    {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "5"
    {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "6"
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                                               0}, // "7"(no high pin segments lit in the number 7)
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "8"
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "9"
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "A"
    {                                SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "b"
    {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }, // "C"
    {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "d"
    {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "E"
    {SEG_A_COM_BIT                                                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "F"

};


// Squiggle segments are used during the Ready To Launch mode animation before the pin is pulled

constexpr unsigned SQUIGGLE_SEGMENTS_SIZE = 8;          // There really should be a better way to do this.

constexpr glyph_segment_t squiggle_segments[SQUIGGLE_SEGMENTS_SIZE] = {
    { SEG_A_COM_BIT , 0x00  },
    { SEG_B_COM_BIT , 0x00 },
    { 0x00          , SEG_G_COM_BIT  },
    { 0x00          , SEG_E_COM_BIT  },
    { SEG_D_COM_BIT , 0x00  },
    { SEG_C_COM_BIT , 0x00  },
    { 0x00          , SEG_G_COM_BIT  },
    { 0x00          , SEG_F_COM_BIT  },
};


constexpr glyph_segment_t dash_segments = { 0x00 , SEG_G_COM_BIT };

constexpr glyph_segment_t blank_segments = {  0x00 , 0x00 };

constexpr glyph_segment_t x_segments = {  SEG_B_COM_BIT | SEG_C_COM_BIT   , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT };

constexpr glyph_segment_t all_segments = { SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT };      // Note this is a just an '8' but good starting point for copy/paste!


// These are little jaggy lines going one way and the other way
constexpr glyph_segment_t left_tick_segments = {  SEG_C_COM_BIT , SEG_F_COM_BIT | SEG_G_COM_BIT };
constexpr glyph_segment_t right_tick_segments = { SEG_B_COM_BIT , SEG_E_COM_BIT | SEG_G_COM_BIT };


constexpr glyph_segment_t testingonly_message[] = {

    {                                                SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  },  // t
    {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  },  // E
    {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  },  // S
    {                                                SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  },  // t
    {                                SEG_C_COM_BIT                 ,                                             0  },  // i
    {                                SEG_C_COM_BIT                 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  },  // n
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  },  // g
    {                                                            0 ,                                             0  },  // (space)
    {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  },  // O
    {                                SEG_C_COM_BIT                 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  },  // n
    {                                                SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  },  // L
    {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  },  // y

};


// This represents a logical digit that we will use to actually display numbers on the screen
// Digit 0 is the rightmost and digit 11 is the leftmost
// LPIN is the name of the pin on the MSP430 that the LCD pin for that digit is connected to.
// Because this is a 4-mux LCD, we need 2 LCD pins for each digit because 2 pins *  4 com lines = 8 segments (7 for the 7-segment digit and sometimes one for an indicator)
// These mappings come from our PCB trace layout

struct digit_lpin_recond_t {
    const uint8_t lpin_e_thru_g;       // The LPIN for segments E-G
    const uint8_t lpin_a_thru_d;       // The LPIN for segments A-D

};

// Map logical digits on the LCD display to LPINS on the MCU. Each LPIN maps to a single nibble the LCD controller memory.
// A digitplace is one of the 12 digits on the LCD display

constexpr digit_lpin_recond_t digitplace_lpins_table[DIGITPLACE_COUNT] {
    { 33 , 32 },        //  0 (LCD 12) - rightmost digit
    { 35 , 34 },        //  1 (LCD 11)
    { 30 , 31 },        //  2 (LCD 10)
    { 28 , 29 },        //  3 {LCD 09)
    { 26 , 27 },        //  4 {LCD 08)
    { 20 , 21 },        //  5 {LCD 07)
    { 18 , 19 },        //  6 (LCD 06)
    { 16 , 17 },        //  7 (LCD 05)
    { 14 , 15 },        //  8 (LCD 04)
    {  1 , 13 },        //  9 (LCD 03)
    {  3 ,  2 },        // 10 (LCD 02)
    {  5 ,  4 },        // 11 (LCD 01) - leftmost digit
};

#define SECS_ONES_DIGITPLACE_INDEX ( 0)
#define SECS_TENS_DIGITPLACE_INDEX ( 1)
#define MINS_ONES_DIGITPLACE_INDEX ( 2)
#define MINS_TENS_DIGITPLACE_INDEX ( 3)

// Returns the byte address for the specified L-pin
// Assumes MSP430 LCD is in 4-Mux mode
// This mapping comes from the MSP430FR2xx datasheet Fig 17-2

enum nibble_t {LOWER,UPPER};

// Each LCD lpin has one nibble, so there are two lpins for each LCD memory address.
// These two functions compute the memory address and the nibble inside that address for a given lpin.

#pragma FUNC_ALWAYS_INLINE
constexpr static uint8_t lpin_lcdmem_offset(const uint8_t lpin) {
    return (lpin>>1); // Intentionally looses bottom bit since consecutive L-pins share the same memory address (but different nibbles)
}

#pragma FUNC_ALWAYS_INLINE
constexpr static nibble_t lpin_nibble(const uint8_t lpin) {
    return lpin & 0x01 ? UPPER : LOWER;  // Extract which nibble
}

// Also in a template format to do computations at compile time for compile time known LPIN

template <uint8_t lpin>
struct lpin_t {
    constexpr static uint8_t lcdmem_offset() {
        return lpin_lcdmem_offset(lpin);
    };

    constexpr static nibble_t nibble() {
        return  lpin_nibble(lpin);
    }
};


// Write the specified nibble. The other nibble at address a is unchanged.

void set_nibble( uint8_t * a , const nibble_t nibble_index , const uint8_t x ) {

    if ( nibble_index == nibble_t::LOWER  ) {

        *a  = (*a & 0b11110000 ) | (x);     // Combine the nibbles and write back to the address

    } else {


        *a  = (*a & 0b00001111 ) | (x<<4);     // Combine the nibbles and write back to the address

    }

}


struct word_of_bytes_of_nibbles_t {

   word as_word;

   void set_nibble( const byte byte_index , nibble_t nibble_index , const byte x ) {

       // This ugliness is only because C++ will not let us make a packed array of nibbles. :/

       if ( byte_index == 0 && nibble_index == nibble_t::LOWER ) {

           as_word &= 0xfff0;
           as_word |= x;

       } else if ( byte_index == 0 && nibble_index == nibble_t::UPPER ) {

           as_word &= 0xff0f;
           as_word |= x << 4;

       } else if ( byte_index == 1 && nibble_index == nibble_t::LOWER ) {

           as_word &= 0xf0ff;
           as_word |= x << 8;

       } else if ( byte_index == 1 && nibble_index == nibble_t::UPPER ) {

           as_word &= 0x0fff;
           as_word |= x << 12;
       }

   }

};


// Make a view of the LCD mem as words
// Remember that that whatever index we go into this pointer will be implicitly *2 because it is words not bytes
constexpr word * LCDMEMW = (word *) (LCDMEM);
constexpr byte LCDMEM_WORD_COUNT=16;             // Total number of words in LCD mem. Note that we do not actually use them all, but our display is spread across it.

// these arrays hold the pre-computed words that we will write to word in LCD memory that
// controls the seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.

#define SECS_PER_MIN 60
word secs_lcd_words[SECS_PER_MIN];

// Make sure that the 4 nibbles that make up the 2 seconds digits are all in the same word in LCD memory
// Note that if you move pins around and are not able to satisfy this requirement, you can not use this optimization and will instead need to manually set the various nibbles in LCDMEM individually.

static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");

// Write a value from the array into this word to update the two digits on the LCD display
// It does not matter if we pick ones or tens digit or upper or lower nibble because if they are all in the
// same word. The `>>1` converts the byte pointer into a word pointer.

constexpr word *secs_lcdmem_word = &LCDMEMW[ lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() >> 1 ];

#define MINS_PER_HOUR 60
word mins_lcd_words[MINS_PER_HOUR];

static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");


// Write a value from the array into this word to update the two digits on the LCD display
constexpr word *mins_lcdmem_word = &LCDMEMW[ lpin_t<digitplace_lpins_table[MINS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() >> 1 ];

// Builds a RAM-based table of words where each word is the value you would assign to a single LCD word address to display a given 2-digit number
// Note that this only works for cases where all 4 of the LPINs for a pair of digits are on consecutive LPINs that use on 2 LCDM addresses.
// In our case, seconds and minutes meet this constraint (not by accident!) so we can do the *vast* majority of all updates efficiently with just a single instruction word assignment.
// Note that if the LPINs are not in the right places, then this will fail by having some unlit segments in some numbers.

// Returns the address in LCDMEM that you should assign a words[] to in order to display the indexed 2 digit number

void fill_lcd_words( word *words , const byte tens_digit_index , const byte ones_digit_index , const byte max_tens_digit , const byte max_ones_digit ) {

    const digit_lpin_recond_t tens_logical_digit = digitplace_lpins_table[tens_digit_index];

    const digit_lpin_recond_t ones_logical_digit = digitplace_lpins_table[ones_digit_index];


    for( byte tens_digit = 0; tens_digit < max_tens_digit ; tens_digit ++ ) {

        // We do not need to initialize this since (if all the nibbles a really in the same word) then each of the nibbles will get assigned below.
        word_of_bytes_of_nibbles_t word_of_nibbles;

        // The ` & 0x01` here is normalizing the address in the LCDMEM to be just the offset into the word (hi or low byte)

        word_of_nibbles.set_nibble( lpin_lcdmem_offset( tens_logical_digit.lpin_a_thru_d ) & 0x01 , lpin_nibble( tens_logical_digit.lpin_a_thru_d ) , digit_segments[tens_digit].nibble_a_thru_d );
        word_of_nibbles.set_nibble( lpin_lcdmem_offset( tens_logical_digit.lpin_e_thru_g ) & 0x01 , lpin_nibble( tens_logical_digit.lpin_e_thru_g ) , digit_segments[tens_digit].nibble_e_thru_g );

        for( byte ones_digit = 0; ones_digit < max_ones_digit ; ones_digit ++ ) {

            word_of_nibbles.set_nibble( lpin_lcdmem_offset( ones_logical_digit.lpin_a_thru_d ) & 0x01 , lpin_nibble(ones_logical_digit.lpin_a_thru_d) , digit_segments[ones_digit].nibble_a_thru_d );
            word_of_nibbles.set_nibble( lpin_lcdmem_offset( ones_logical_digit.lpin_e_thru_g ) & 0x01 , lpin_nibble(ones_logical_digit.lpin_e_thru_g) , digit_segments[ones_digit].nibble_e_thru_g );

            words[ (tens_digit * max_ones_digit) + ones_digit ] = word_of_nibbles.as_word;

        }


    }

};

// Define a full LCD frame so we can put it into LCD memory in one shot.
// Note that a frame includes all of LCD memory even though only some of the nibbles are actually displayed
// It is faster to copy a sequence of words than try to only set the nibbles that have changed.

union lcd_frame_t {
    word as_words[LCDMEM_WORD_COUNT];
    byte as_bytes[LCDMEM_WORD_COUNT*2];
};

lcd_frame_t ready_to_launch_lcd_frames[READY_TO_LAUNCH_LCD_FRAME_COUNT];     // Precomputed ready-to-launch animation frames ready to copy directly to LCD memory

void fill_ready_to_lanch_lcd_frames() {

    // Generate each frame in the animation

    for( byte frame =0; frame < READY_TO_LAUNCH_LCD_FRAME_COUNT ; frame++ ) {

        lcd_frame_t * const lcd_frame = &ready_to_launch_lcd_frames[frame];

        // Alternating digits on the display go in alternating directions
        const glyph_segment_t even_digit_segments = squiggle_segments[ frame ];
        const glyph_segment_t odd_digit_segments = squiggle_segments[ (SQUIGGLE_SEGMENTS_SIZE - frame) % (SQUIGGLE_SEGMENTS_SIZE-1) ];

        // Assign the lit up segments for each digit place in the current frame

        for( byte digit = 0; digit <  DIGITPLACE_COUNT ; digit++ ) {

            // Tells us the lpins for this digitplace
            const digit_lpin_recond_t logical_digit = digitplace_lpins_table[digit];

            const glyph_segment_t digit_segments = (digit & 0x01) ? odd_digit_segments : even_digit_segments;

            // Since we are filling the whole frame in batch, we don't care where the nibbles end up since we know we will eventually assign them all.

            // Set the a_thru_d nibble
            set_nibble( &(lcd_frame->as_bytes[ lpin_lcdmem_offset( logical_digit.lpin_a_thru_d) ]) , lpin_nibble( logical_digit.lpin_a_thru_d ) , digit_segments.nibble_a_thru_d  );

            // Set the e_thru_g nibble
            set_nibble( &(lcd_frame->as_bytes[ lpin_lcdmem_offset( logical_digit.lpin_e_thru_g) ]) , lpin_nibble( logical_digit.lpin_e_thru_g ) , digit_segments.nibble_e_thru_g  );


        }
/*
        ready_to_launch_lcd_frames[frame].as_words[frame]=0xffff;
        ready_to_launch_lcd_frames[frame].as_words[frame+8]=0xffff;
*/
    }



}

// Fills the arrays

void initLCDPrecomputedWordArrays() {

    // Note that we need different arrays for the minutes and seconds because , while both have all four LCD pins in the same LCDMEM word,
    // they had to be connected in different orders just due to PCB routing constraints. Of course it would have been great to get them ordered the same
    // way and save some RAM (or even also get all 4 of the hours pin in the same LCDMEM word) but I think we are just lucky that we could get things router so that
    // these two updates are optimized since they account for the VAST majority of all time spent in the CPU active mode.

    // Fill the seconds array
    fill_lcd_words( secs_lcd_words , SECS_TENS_DIGITPLACE_INDEX , SECS_ONES_DIGITPLACE_INDEX , 6 , 10 );
    // Fill the minutes array
    fill_lcd_words( mins_lcd_words , MINS_TENS_DIGITPLACE_INDEX , MINS_ONES_DIGITPLACE_INDEX , 6 , 10 );
    // Fill the array of frames for ready-to-launch-mode animation
    fill_ready_to_lanch_lcd_frames();
}


// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show() {

    constexpr uint8_t nibble_a_thru_d =  digit_segments[x].nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    constexpr uint8_t nibble_e_thru_g =  digit_segments[x].nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    constexpr uint8_t lpin_a_thru_d = digitplace_lpins_table[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    constexpr uint8_t lpin_e_thru_g = digitplace_lpins_table[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

    constexpr uint8_t lcdmem_offset_a_thru_d = (lpin_t< lpin_a_thru_d >::lcdmem_offset()); // Look up the memory address for the low segment bits
    constexpr uint8_t lcdmem_offset_e_thru_g = lpin_t< lpin_e_thru_g >::lcdmem_offset(); // Look up the memory address for the high segment bits

    uint8_t * const lcd_mem_reg = LCDMEM;

    /*
      I know that line above looks dumb, but if you just access LCDMEM directly the compiler does the wrong thing and loads the offset
      into a register rather than the base and this creates many wasted loads (LCDMEM=0x0620)...

            00c4e2:   403F 0010           MOV.W   #0x0010,R15
            00c4e6:   40FF 0060 0620      MOV.B   #0x0060,0x0620(R15)
            00c4ec:   403F 000E           MOV.W   #0x000e,R15
            00c4f0:   40FF 00F2 0620      MOV.B   #0x00f2,0x0620(R15)

      If we load it into a variable first, the compiler then gets wise and uses that as the base..

            00c4e2:   403F 0620           MOV.W   #0x0620,R15
            00c4e6:   40FF 0060 0010      MOV.B   #0x0060,0x0010(R15)
            00c4ec:   40FF 00F2 000E      MOV.B   #0x00f2,0x000e(R15)

     */

    if ( lcdmem_offset_a_thru_d == lcdmem_offset_e_thru_g  ) {

        // If the two L-pins for this digit are in the same memory location, we can update them both with a single byte write
        // Note that the whole process that gets us here is static at compile time, so the whole lcd_show() call will compile down
        // to just a single immediate byte write, which is very efficient.

        const uint8_t lpin_a_thru_d_nibble = lpin_t< lpin_a_thru_d >::nibble();

        if ( lpin_a_thru_d_nibble == nibble_t::LOWER  ) {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_e_thru_g << 4 ) | (nibble_a_thru_d);     // Combine the nibbles, write them to the mem address

        } else {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_a_thru_d << 4 ) | (nibble_e_thru_g);     // Combine the nibbles, write them to the mem address

        }

    } else {

        // The A-D segments are located on a pin that has a different address than the E-G segments, so we can to manually splice the nibbles into those two addresses

        // Write the a_thru_d nibble to the memory address is lives in

        const nibble_t lpin_a_thru_d_nibble_index = lpin_t< lpin_a_thru_d >::nibble();

        set_nibble( &lcd_mem_reg[lcdmem_offset_a_thru_d] , lpin_a_thru_d_nibble_index , nibble_a_thru_d );


        // Write the e_thru_f nibble to the memory address is lives in

        const nibble_t lpin_e_thru_g_nibble_index = lpin_t< lpin_e_thru_g >::nibble();

        set_nibble( &lcd_mem_reg[lcdmem_offset_e_thru_g] , lpin_e_thru_g_nibble_index , nibble_e_thru_g );

    }

}


// Show the digit x at position p
// where p=0 is the rightmost digit


void lcd_show_f( const uint8_t pos, const glyph_segment_t segs ) {

    const uint8_t nibble_a_thru_d =  segs.nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    const uint8_t nibble_e_thru_g =  segs.nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    const uint8_t lpin_a_thru_d = digitplace_lpins_table[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    const uint8_t lpin_e_thru_g = digitplace_lpins_table[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

    const uint8_t lcdmem_offset_a_thru_d = lpin_lcdmem_offset( lpin_a_thru_d ); // Look up the memory address for the low segment bits
    const uint8_t lcdmem_offset_e_thru_g = lpin_lcdmem_offset( lpin_e_thru_g ); // Look up the memory address for the high segment bits

    uint8_t * const lcd_mem_reg = (uint8_t *) LCDMEM;

    /*
      I know that line above looks dumb, but if you just access LCDMEM directly the compiler does the wrong thing and loads the offset
      into a register rather than the base and this creates many wasted loads (LCDMEM=0x0620)...

            00c4e2:   403F 0010           MOV.W   #0x0010,R15
            00c4e6:   40FF 0060 0620      MOV.B   #0x0060,0x0620(R15)
            00c4ec:   403F 000E           MOV.W   #0x000e,R15
            00c4f0:   40FF 00F2 0620      MOV.B   #0x00f2,0x0620(R15)

      If we load it into a variable first, the compiler then gets wise and uses that as the base..

            00c4e2:   403F 0620           MOV.W   #0x0620,R15
            00c4e6:   40FF 0060 0010      MOV.B   #0x0060,0x0010(R15)
            00c4ec:   40FF 00F2 000E      MOV.B   #0x00f2,0x000e(R15)

     */

    if ( lcdmem_offset_a_thru_d == lcdmem_offset_e_thru_g  ) {

        // If the two L-pins for this digit are in the same memory location, we can update them both with a single byte write
        // Note that the whole process that gets us here is static at compile time, so the whole lcd_show() call will compile down
        // to just a single immediate byte write, which is very efficient.

        const uint8_t lpin_a_thru_d_nibble = lpin_nibble( lpin_a_thru_d );

        if ( lpin_a_thru_d_nibble == nibble_t::LOWER  ) {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_e_thru_g << 4 ) | (nibble_a_thru_d);     // Combine the nibbles, write them to the mem address

        } else {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_a_thru_d << 4 ) | (nibble_e_thru_g);     // Combine the nibbles, write them to the mem address

        }

    } else {

        // The A-D segments are located on a pin that has a different address than the E-G segments, so we can to manually splice the nibbles into those two addresses

        // Write the a_thru_d nibble to the memory address is lives in

        const nibble_t lpin_a_thru_d_nibble_index = lpin_nibble(lpin_a_thru_d);

        if (lpin_a_thru_d_nibble_index == nibble_t::LOWER) {

            // a-d pin uses in lower nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (lcd_mem_reg[lcdmem_offset_a_thru_d] & 0xf0) | (( nibble_a_thru_d & 0x0f ) << 0 );    // Trailing & is unnecessary, there for clarity.

        } else {

            // a-d pin uses in upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (lcd_mem_reg[lcdmem_offset_a_thru_d] & 0x0f) | (( nibble_a_thru_d & 0x0f  ) << 4 ) ;    // Trailing & is unnecessary, there for clarity.

        }


        // Write the e_thru_f nibble to the memory address is lives in

        const nibble_t lpin_e_thru_g_nibble_index = lpin_nibble(lpin_e_thru_g);

        if (lpin_e_thru_g_nibble_index == nibble_t::LOWER) {

            // e-f pin uses in lower nibble

            lcd_mem_reg[lcdmem_offset_e_thru_g] = (lcd_mem_reg[lcdmem_offset_e_thru_g] & 0xf0) | (( nibble_e_thru_g & 0x0f ) << 0 );    // Trailing & is unnecessary, there for clarity.

        } else {

            // e-f pin uses in upper nibble

            lcd_mem_reg[lcdmem_offset_e_thru_g] = (lcd_mem_reg[lcdmem_offset_e_thru_g] & 0x0f) | (( nibble_e_thru_g & 0x0f  ) << 4 ) ;    // Trailing & is unnecessary, there for clarity.

        }


    }

}

inline void lcd_show_fast_secs( uint8_t secs ) {

    *secs_lcdmem_word = secs_lcd_words[  secs ];

}

void lcd_show_digit_f( const uint8_t pos, const byte d ) {

    lcd_show_f( pos , digit_segments[ d ] );
}



void lcd_show_testing_only_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , testingonly_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

}

// For now, show all 9's.
// TODO: Figure out something better here

void lcd_show_long_now() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_digit_f( i , 9 );

    }

}


// Show a frame n the ready-to-lanuch squiggle animation
// 0 <= step < SQUIGGLE_ANIMATION_FRAME_COUNT

void lcd_show_squiggle_frame( byte step ) {

    static_assert( SQUIGGLE_SEGMENTS_SIZE == 8 , "SQUIGGLE_SEGMENTS_SIZE should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07
    static_assert( SQUIGGLE_ANIMATION_FRAME_COUNT == 8 , "SQUIGGLE_ANIMATION_FRAME_COUNT should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        if ( i & 0x01 ) {
            lcd_show_f( i , squiggle_segments[ ((SQUIGGLE_SEGMENTS_SIZE + 4 )-  step ) & 0x07 ]);
        } else {
            lcd_show_f( i , squiggle_segments[ step ]);
        }

    }

}

// Fill the screen with horizontal dashes

void lcd_show_dashes() {
    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , dash_segments );

    }
}

// show a little dash sliding towards the trigger

// In position`step` we draw a trailing space that will erase the tail of the moving dash
// Remember that position 11 is the leftmost position and we want the dash to animate to the right to point to the physical trigger switch.

void lcd_show_lance( byte step ) {

    int blank_slot_pos       = step-1;      // Blank out the trail behind the lance as it moves
    int lance_left_slot_pos  = step;
    int lance_right_slot_pos = step+1;

    if ( blank_slot_pos >=0 && blank_slot_pos < DIGITPLACE_COUNT ) {
        lcd_show_f( blank_slot_pos , blank_segments );
    }

    if ( lance_left_slot_pos >=0 && lance_left_slot_pos< DIGITPLACE_COUNT ) {
        lcd_show_f( lance_left_slot_pos , dash_segments );
    }

    if ( lance_right_slot_pos >=0 && lance_right_slot_pos< DIGITPLACE_COUNT ) {
        lcd_show_f( lance_right_slot_pos , dash_segments );
    }

}


// "Error CodE X"

constexpr glyph_segment_t errorcode_message[] = {

     {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "E"
     {                                                            0 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "r"
     {                                                            0 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "r"
     {                                SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "o"
     {                                                            0 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "r"
     {                                                            0 ,                                             0  }, // " "
     {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }, // "C"
     {                                SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "o"
     {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }, // "d"
     {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "E"
     {                                                            0 ,                                             0  }, // " "
     {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }, // "X"

};

// Show the message "Error X" on the lcd.

void lcd_show_errorcode( byte code  ) {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , errorcode_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

    lcd_show_digit_f( 0 , code );

}


