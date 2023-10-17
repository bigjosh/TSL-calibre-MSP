/*
 * lcd_display.cpp
 *
 *  Created on: Jan 20, 2023
 *      Author: josh
 */


// Needed for LCDMEM addresses
#include <msp430.h>


#include "lcd_display.h"
#include "lcd_display_exp.h"

/* ======================= CUT HERE START ====================================== */

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

constexpr glyph_segment_t glyph_0      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }; // "0"
constexpr glyph_segment_t glyph_1      = {                SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                                               0}; // "1" (no high pin segments lit in the number 1)
constexpr glyph_segment_t glyph_2      = {SEG_A_COM_BIT | SEG_B_COM_BIT |                 SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }; // "2"
constexpr glyph_segment_t glyph_3      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                                 SEG_G_COM_BIT  }; // "3"
constexpr glyph_segment_t glyph_4      = {                SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "4"
constexpr glyph_segment_t glyph_5      = {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "5"
constexpr glyph_segment_t glyph_6      = {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "6"
constexpr glyph_segment_t glyph_7      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                                               0}; // "7"(no high pin segments lit in the number 7)
constexpr glyph_segment_t glyph_8      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "8"
constexpr glyph_segment_t glyph_9      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "9"
constexpr glyph_segment_t glyph_A      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "A"
constexpr glyph_segment_t glyph_b      = {                                SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "b"
constexpr glyph_segment_t glyph_C      = {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }; // "C"
constexpr glyph_segment_t glyph_c      = {                                                SEG_D_COM_BIT , SEG_E_COM_BIT                 | SEG_G_COM_BIT  }; // "c"
constexpr glyph_segment_t glyph_d      = {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }; // "d"
constexpr glyph_segment_t glyph_E      = {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "E"
constexpr glyph_segment_t glyph_F      = {SEG_A_COM_BIT                                                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "F"
constexpr glyph_segment_t glyph_g      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  };  // g
constexpr glyph_segment_t glyph_H      = {                SEG_B_COM_BIT | SEG_C_COM_BIT                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "8"
constexpr glyph_segment_t glyph_I      = {                SEG_B_COM_BIT | SEG_C_COM_BIT                 ,                                               0};  // I
constexpr glyph_segment_t glyph_J      = {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT                                  }; // "J"
constexpr glyph_segment_t glyph_K      = {                SEG_B_COM_BIT | SEG_C_COM_BIT                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "K"
constexpr glyph_segment_t glyph_i      = {                                SEG_C_COM_BIT                 ,                                               0};  // i
constexpr glyph_segment_t glyph_L      = {                                                SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  };  // L
constexpr glyph_segment_t glyph_m1     = {                                SEG_C_COM_BIT                 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }; // left half of "m"
constexpr glyph_segment_t glyph_m2     = {                                SEG_C_COM_BIT                 ,                                 SEG_G_COM_BIT  }; // right half of "m"
constexpr glyph_segment_t glyph_n      = {                                SEG_C_COM_BIT                 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  };  // n
constexpr glyph_segment_t glyph_O      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  };  // O
constexpr glyph_segment_t glyph_o      = {                                SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }; // "o"
constexpr glyph_segment_t glyph_P      = {SEG_A_COM_BIT | SEG_B_COM_BIT                                 , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "P"
constexpr glyph_segment_t glyph_r      = {                                                            0 , SEG_E_COM_BIT |                 SEG_G_COM_BIT  }; // "r"
constexpr glyph_segment_t glyph_S      = {SEG_A_COM_BIT |                 SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  };  // S
constexpr glyph_segment_t glyph_t      = {                                                SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  };  // t
constexpr glyph_segment_t glyph_X      = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT | SEG_G_COM_BIT  }; // "X"
constexpr glyph_segment_t glyph_y      = {                SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                 SEG_F_COM_BIT | SEG_G_COM_BIT  };  // y

constexpr glyph_segment_t glyph_SPACE  = {                                                            0 ,                                               0}; // " "

constexpr glyph_segment_t glyph_dash   = { 0x00 , SEG_G_COM_BIT };                                                                                          // "-"
constexpr glyph_segment_t glyph_rbrac  = {SEG_A_COM_BIT | SEG_B_COM_BIT | SEG_C_COM_BIT | SEG_D_COM_BIT ,                                               0}; // "]"
constexpr glyph_segment_t glyph_lbrac  = {SEG_A_COM_BIT |                                 SEG_D_COM_BIT , SEG_E_COM_BIT | SEG_F_COM_BIT                  }; // "["


// All the single digit numbers (up to 0x0f hex) in an array for easy access

constexpr glyph_segment_t digit_segments[0x10] = {

    glyph_0, // "0"
    glyph_1, // "1" (no high pin segments lit in the number 1)
    glyph_2, // "2"
    glyph_3, // "3"
    glyph_4, // "4"
    glyph_5, // "5"
    glyph_6, // "6"
    glyph_7, // "7"(no high pin segments lit in the number 7)
    glyph_8, // "8"
    glyph_9, // "9"
    glyph_A, // "A"
    glyph_b, // "b"
    glyph_C, // "C"
    glyph_d, // "d"
    glyph_E, // "E"
    glyph_F, // "F"

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


// These are little jaggy lines going one way and the other way
constexpr glyph_segment_t left_tick_segments = {  SEG_C_COM_BIT , SEG_F_COM_BIT | SEG_G_COM_BIT };
constexpr glyph_segment_t right_tick_segments = { SEG_B_COM_BIT , SEG_E_COM_BIT | SEG_G_COM_BIT };


// This represents a logical digit that we will use to actually display numbers on the screen
// Digit 0 is the rightmost and digit 11 is the leftmost
// LPIN is the name of the pin on the MSP430 that the LCD pin for that digit is connected to.
// Because this is a 4-mux LCD, we need 2 LCD pins for each digit because 2 pins *  4 com lines = 8 segments (7 for the 7-segment digit and sometimes one for an indicator)
// These mappings come from our PCB trace layout

struct digit_lpin_record_t {
    const uint8_t lpin_e_thru_g;       // The LPIN for segments E-G
    const uint8_t lpin_a_thru_d;       // The LPIN for segments A-D

};

// Map logical digits on the LCD display to LPINS on the MCU. Each LPIN maps to a single nibble the LCD controller memory.
// A digitplace is one of the 12 digits on the LCD display

constexpr digit_lpin_record_t digitplace_lpins_table[DIGITPLACE_COUNT] {
    { 33 , 32 },        //  0 (LCD 12) - sec   1 -rightmost digit
    { 35 , 34 },        //  1 (LCD 11) - sec  10
    { 30 , 31 },        //  2 (LCD 10) - min   1
    { 28 , 29 },        //  3 {LCD 09) - min  10
    { 26 , 27 },        //  4 {LCD 08) - hour  1
    { 20 , 21 },        //  5 {LCD 07) - hour 10
    { 18 , 19 },        //  6 (LCD 06) - day 1
    { 16 , 17 },        //  7 (LCD 05)
    { 14 , 15 },        //  8 (LCD 04)
    {  1 , 13 },        //  9 (LCD 03)
    {  3 ,  2 },        // 10 (LCD 02)
    {  5 ,  4 },        // 11 (LCD 01) - day 6 - leftmost digit
};

#define SECS_ONES_DIGITPLACE_INDEX ( 0)
#define SECS_TENS_DIGITPLACE_INDEX ( 1)
#define MINS_ONES_DIGITPLACE_INDEX ( 2)
#define MINS_TENS_DIGITPLACE_INDEX ( 3)

// Note that hours digits did not end up on consecutive LPINs, so we update the bytes separately.
#define HOURS_ONES_DIGITPLACE_INDEX ( 4)
#define HOURS_TENS_DIGITPLACE_INDEX ( 5)

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


constexpr word * LCDMEMW = (word *) LCDMEM;

constexpr byte LCDMEM_WORD_COUNT=16;             // Total number of words in LCD mem. Note that we do not actually use them all, but our display is spread across it.

// these arrays hold the pre-computed words that we will write to word in LCD memory that
// controls the seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.

#pragma RETAIN
word secs_lcd_words[SECS_PER_MIN];
word secs_lcd_words_no_offset[SECS_PER_MIN];

// Make sure that the 4 nibbles that make up the 2 seconds digits are all in the same word in LCD memory
// Note that if you move pins around and are not able to satisfy this requirement, you can not use this optimization and will instead need to manually set the various nibbles in LCDMEM individually.

static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");

// Write a value from the array into this word to update the two digits on the LCD display
// It does not matter if we pick ones or tens digit or upper or lower nibble because if they are all in the
// same word. The `>>1` converts the byte pointer into a word pointer.

// This address is hardcoded into the ASM so we don't need the reference here.
word *secs_lcdmem_word = (word *) (&LCDMEMW[ lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() >> 1 ]);

#define MINS_PER_HOUR 60
word mins_lcd_words[MINS_PER_HOUR];
word mins_lcd_words_no_offset[MINS_PER_HOUR];


static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");




// Builds a RAM-based table of words where each word is the value you would assign to a single LCD word address to display a given 2-digit number
// Note that this only works for cases where all 4 of the LPINs for a pair of digits are on consecutive LPINs that use on 2 LCDM addresses.
// In our case, seconds and minutes meet this constraint (not by accident!) so we can do the *vast* majority of all updates efficiently with just a single instruction word assignment.
// Note that if the LPINs are not in the right places, then this will fail by having some unlit segments in some numbers.

// Note there is a tricky part - [59] = "00", [00]="01", [01] = "02"... [58] = "59". This is because the MSP430 only has POST INCREMENT instructions
// and no pre increment, so we are basically living one second into the future get to use the free post increment and to save some cycles.

// Returns the address in LCDMEM that you should assign a words[] to in order to display the indexed 2 digit number

void fill_lcd_words( word *words , word *words_no_offset ,  const byte tens_digit_index , const byte ones_digit_index , const byte max_tens_digit , const byte max_ones_digit ) {

    const digit_lpin_record_t tens_logical_digit = digitplace_lpins_table[tens_digit_index];

    const digit_lpin_record_t ones_logical_digit = digitplace_lpins_table[ones_digit_index];


    for( byte tens_digit = 0; tens_digit < max_tens_digit ; tens_digit ++ ) {

        // We do not need to initialize this since (if all the nibbles a really in the same word) then each of the nibbles will get assigned below.
        word_of_bytes_of_nibbles_t word_of_nibbles;

        // The ` & 0x01` here is normalizing the address in the LCDMEM to be just the offset into the word (hi or low byte)

        word_of_nibbles.set_nibble( lpin_lcdmem_offset( tens_logical_digit.lpin_a_thru_d ) & 0x01 , lpin_nibble( tens_logical_digit.lpin_a_thru_d ) , digit_segments[tens_digit].nibble_a_thru_d );
        word_of_nibbles.set_nibble( lpin_lcdmem_offset( tens_logical_digit.lpin_e_thru_g ) & 0x01 , lpin_nibble( tens_logical_digit.lpin_e_thru_g ) , digit_segments[tens_digit].nibble_e_thru_g );

        for( byte ones_digit = 0; ones_digit < max_ones_digit ; ones_digit ++ ) {

            word_of_nibbles.set_nibble( lpin_lcdmem_offset( ones_logical_digit.lpin_a_thru_d ) & 0x01 , lpin_nibble(ones_logical_digit.lpin_a_thru_d) , digit_segments[ones_digit].nibble_a_thru_d );
            word_of_nibbles.set_nibble( lpin_lcdmem_offset( ones_logical_digit.lpin_e_thru_g ) & 0x01 , lpin_nibble(ones_logical_digit.lpin_e_thru_g) , digit_segments[ones_digit].nibble_e_thru_g );

            // This next line is where we add the +1 offset to all our tables.
            words[ (((tens_digit * max_ones_digit) + ones_digit) + ( (max_tens_digit*max_ones_digit )-1 )  ) % (max_tens_digit*max_ones_digit )] = word_of_nibbles.as_word;
            words_no_offset[ (tens_digit * max_ones_digit) + ones_digit] = word_of_nibbles.as_word;

        }


    }

};


// Write a value from the array into this word to update the two digits on the LCD display
word *mins_lcdmem_word = &LCDMEMW[ lpin_t<digitplace_lpins_table[MINS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() >> 1 ];

byte hours_lcd_bytes[10];

// The two pins for each of the two hours digits must be in the same LCDMEM byte for this optimization to work
static_assert( lpin_t<digitplace_lpins_table[HOURS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() == lpin_t<digitplace_lpins_table[HOURS_ONES_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() , "hours 1's digit pins must be in the same LCDMEM byte " );
static_assert( lpin_t<digitplace_lpins_table[HOURS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d>::lcdmem_offset() == lpin_t<digitplace_lpins_table[HOURS_TENS_DIGITPLACE_INDEX].lpin_e_thru_g>::lcdmem_offset() , "hours 1's digit pins must be in the same LCDMEM byte " );

// T0 use the same table for the two hours digits, they both have have the nibbles in the same order
// (if this is not possible, then you can use two tables, one for each order)
static_assert( lpin_t<digitplace_lpins_table[SECS_ONES_DIGITPLACE_INDEX].lpin_a_thru_d >::nibble() ==lpin_t<digitplace_lpins_table[SECS_TENS_DIGITPLACE_INDEX].lpin_a_thru_d >::nibble() , "both hours digits must have thier nibbbles in the same order" );


// Builds a RAM-based table of bytes where each bytes is the value you would assign to a single LCD byte address to display a given 1-digit number
// Note that this only works for cases where both of the LPINs for a digit are on consecutive LPINs that use on 1 LCDM address.
// In our case, hours ones and tens meet this constraint (not by accident!)
// Note that if the LPINs are not in the right places, then this will fail by having some unlit segments in some numbers.

// Returns the address in LCDMEM that you should assign a words[] to in order to display the indexed 2 digit number

void fill_lcd_bytes( byte *bytes , const byte digit_index ) {

    const digit_lpin_record_t logical_digit = digitplace_lpins_table[ digit_index];


    for( byte digit = 0; digit < 10  ; digit ++ ) {

        // We do not need to initialize this since (if all the nibbles a really in the same word) then each of the nibbles will get assigned below.
        byte byte_of_nibbles;

        // The ` & 0x01` here is normalizing the address in the LCDMEM to be just the offset into the word (hi or low byte)

        if  ( lpin_nibble( logical_digit.lpin_a_thru_d ) == LOWER ) {
            // A-D is low
            byte_of_nibbles =  digit_segments[digit].nibble_a_thru_d  | ( digit_segments[digit].nibble_e_thru_g  ) << 4;

        } else {
            // A-D is high
            byte_of_nibbles =  (digit_segments[digit].nibble_a_thru_d << 4)  |  digit_segments[digit].nibble_e_thru_g  ;

        }

        bytes[ digit  ] = byte_of_nibbles;  // do not adjust +1 like we do for the words tables

    }

};




// Define a full LCD frame so we can put it into LCD memory in one shot.
// Note that a frame only includes words of LCD memory that have actual pins used. This is hard coded here and in the
// RTL_MODE_ISR. There are a total of only 8 words spread across 2 extents. This is driven by the PCB layout.
// It is faster to copy a sequence of words than try to only set the nibbles that have changed.

union lcd_frame_t {
    word as_words[LCDMEM_WORD_COUNT];
    byte as_bytes[LCDMEM_WORD_COUNT*2];
};

#define RTL_LCDMEM_WORD_COUNT 8

// The LCDMEM words actually used in our PCB layout. Also hardcoded in RTL_MODE_ISR
constexpr byte used_rtl_lcdmem_bytes[RTL_LCDMEM_WORD_COUNT] = {0,2,6,8,10,12,14,16};

// Here we hard code the size of each frame to 8 words because it is hardcoded in the asm code, so no point in making it dynamic.
// The fact that there are (8 frames * 8 words per frame * 2 bytes per word) in the animation is a happy conincendnce - it means we can use a single AND to reset the animation with no branch.
#pragma DATA_ALIGN ( 128 )
word ready_to_launch_lcd_frame_words[READY_TO_LAUNCH_LCD_FRAME_COUNT][RTL_LCDMEM_WORD_COUNT];     // Precomputed ready-to-launch animation frames ready to copy directly to LCD memory

static_assert( READY_TO_LAUNCH_LCD_FRAME_COUNT*RTL_LCDMEM_WORD_COUNT*2 == 128 , "RTL_MODE_ISR requires the frame table to be 128 bytes long");

void fill_ready_to_launch_lcd_frames() {

    // Generate each frame in the animation

    for( byte frame =0; frame < READY_TO_LAUNCH_LCD_FRAME_COUNT ; frame++ ) {

        lcd_frame_t lcd_frame;      // A full frame in working memory. We will later copy the words we need into our compact array.

        // Alternating digits on the display go in alternating directions
        const glyph_segment_t even_digit_segments = squiggle_segments[ frame ];
        const glyph_segment_t odd_digit_segments = squiggle_segments[ (SQUIGGLE_SEGMENTS_SIZE - frame) % (SQUIGGLE_SEGMENTS_SIZE) ];

        // Assign the lit up segments for each digit place in the current frame

        for( byte digit = 0; digit <  DIGITPLACE_COUNT ; digit++ ) {

            // Tells us the lpins for this digitplace
            const digit_lpin_record_t logical_digit = digitplace_lpins_table[digit];

            // Switch between clockwise and counter-clockwise rotation for alternating digits.
            const glyph_segment_t digit_segments = (digit & 0x01) ? odd_digit_segments : even_digit_segments;

            // Since we are filling the whole frame in batch, we don't care where the nibbles end up since we know we will eventually assign them all.

            // Set the a_thru_d nibble
            set_nibble( &(lcd_frame.as_bytes[ lpin_lcdmem_offset( logical_digit.lpin_a_thru_d) ]) , lpin_nibble( logical_digit.lpin_a_thru_d ) , digit_segments.nibble_a_thru_d  );

            // Set the e_thru_g nibble
            set_nibble( &(lcd_frame.as_bytes[ lpin_lcdmem_offset( logical_digit.lpin_e_thru_g) ]) , lpin_nibble( logical_digit.lpin_e_thru_g ) , digit_segments.nibble_e_thru_g  );

        }

        // Now we extract only the LCDMEM words that have pins actually connected into the array that the ISR will use.


        for( byte i=0 ; i< RTL_LCDMEM_WORD_COUNT ; i++ ) {
            ready_to_launch_lcd_frame_words[frame][i] = lcd_frame.as_words[used_rtl_lcdmem_bytes[i]/2];  // div by 2 to convert byte index into word index
        }



    }

}



/*
    // Highly optimized with pre-render buffer
    // 1.4uA
    // 138us

    void lcd_show_squiggle_frame( byte frame ) {

        word *w = ready_to_launch_lcd_frames[frame].as_words;
        word *lcdmemptr = LCDMEMW;

        // Fill the LCD mem with our pre-computed words.

        for(byte i=0;i<LCDMEM_WORD_COUNT;i++) {
            *(lcdmemptr++) = *(w++);
        }

    }
*/


// Fills the arrays

void initLCDPrecomputedWordArrays() {

    // Note that we need different arrays for the minutes and seconds because , while both have all four LCD pins in the same LCDMEM word,
    // they had to be connected in different orders just due to PCB routing constraints. Of course it would have been great to get them ordered the same
    // way and save some RAM (or even also get all 4 of the hours pin in the same LCDMEM word) but I think we are just lucky that we could get things router so that
    // these two updates are optimized since they account for the VAST majority of all time spent in the CPU active mode.

    // Fill the seconds array
    fill_lcd_words( secs_lcd_words , secs_lcd_words_no_offset , SECS_TENS_DIGITPLACE_INDEX , SECS_ONES_DIGITPLACE_INDEX , 6 , 10 );
    // Fill the minutes array
    fill_lcd_words( mins_lcd_words , mins_lcd_words_no_offset , MINS_TENS_DIGITPLACE_INDEX , MINS_ONES_DIGITPLACE_INDEX , 6 , 10 );
    // Fill the array of frames for ready-to-launch-mode animation
    fill_lcd_bytes( hours_lcd_bytes , HOURS_ONES_DIGITPLACE_INDEX );
    fill_ready_to_launch_lcd_frames();
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


// Show the glyph x at position p
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


// For now, show all 9's.
// TODO: Figure out something better here

void lcd_show_long_now() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_digit_f( i , 9 );

    }

}


/*

// Original non-optimized version.

// Show a frame n the ready-to-lanuch squiggle animation
// 0 <= step < SQUIGGLE_ANIMATION_FRAME_COUNT
// 839us, ~1.5uA

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

*/


// Fill the screen with horizontal dashes

void lcd_show_dashes() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_dash );

    }
}


// Fill the screen with 0's

void lcd_show_zeros() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_0 );

    }
}

// Fill the screen with X's

void lcd_show_XXX() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_X );

    }
}



constexpr glyph_segment_t testingonly_message[] = {
                                                   glyph_t,
                                                   glyph_E,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_I,
                                                   glyph_n,
                                                   glyph_g,
                                                   glyph_SPACE,
                                                   glyph_O,
                                                   glyph_n,
                                                   glyph_L,
                                                   glyph_y,
};


void lcd_show_testing_only_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , testingonly_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

}


// "bAtt Error X"

constexpr glyph_segment_t batt_errorcode_message[] = {
     glyph_b, // "b"
     glyph_A, // "A"
     glyph_t,  // t
     glyph_t,  // t
     glyph_SPACE, // " "
     glyph_E, // "E"
     glyph_r, // "r"
     glyph_r, // "r"
     glyph_o, // "o"
     glyph_r, // "r"
     glyph_SPACE, // " "
     glyph_X, // "X"

};

// Show the message "bAtt Error X" on the lcd.

void lcd_show_batt_errorcode( byte code  ) {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , batt_errorcode_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

    lcd_show_digit_f( 0 , code );

}


// "Error CodE X"

constexpr glyph_segment_t errorcode_message[] = {
    glyph_E, // "E"
    glyph_r, // "r"
    glyph_r, // "r"
    glyph_o, // "o"
    glyph_r, // "r"
    glyph_SPACE, // " "
    glyph_C, // "C"
    glyph_o, // "o"
    glyph_d, // "d"
    glyph_E, // "E"
    glyph_SPACE, // " "
    glyph_X, // "X"

};

// Show the message "Error X" on the lcd.

void lcd_show_errorcode( byte code  ) {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , errorcode_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

    lcd_show_digit_f( 0 , code );

}

// LoAd PIn --=

constexpr glyph_segment_t load_pin_message[] = {
                                                 glyph_L,
                                                 glyph_O,
                                                 glyph_A,
                                                 glyph_d,
                                                 glyph_SPACE,
                                                 glyph_P,
                                                 glyph_i,
                                                 glyph_n,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
};


void lcd_show_load_pin_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , load_pin_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 0 is rightmost, so reverse order for text
    }

}

void lcd_show_load_pin_animation(unsigned int step) {

    switch( step ) {

        case 0:
            lcd_show_f(  3 , glyph_dash );
            break;

        case 1:
            lcd_show_f(  3 , glyph_SPACE );
            lcd_show_f(  2 , glyph_dash );
            break;

        case 2:
            lcd_show_f(  2 , glyph_SPACE );
            lcd_show_f(  1 , glyph_dash );
            break;

        case 3:
            lcd_show_f(  1 , glyph_SPACE );
            lcd_show_f(  0 , glyph_dash );
            break;

        case 4:
            lcd_show_f(  0 , glyph_SPACE );
            break;

        default:
            __never_executed();

    }

}


constexpr glyph_segment_t first_start_message[] = {
                                                   glyph_F,
                                                   glyph_i,
                                                   glyph_r,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_SPACE,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_A,
                                                   glyph_r,
                                                   glyph_t,
                                                   glyph_SPACE,
};

// Show "First Start"
void lcd_show_first_start_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , first_start_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}

// " -Armning-  "

constexpr glyph_segment_t arming_message[] = {
                                                   glyph_SPACE,
                                                   glyph_SPACE,
                                                   glyph_A,
                                                   glyph_r,
                                                   glyph_m1,
                                                   glyph_m2,
                                                   glyph_i,
                                                   glyph_n,
                                                   glyph_g,
                                                   glyph_SPACE,
                                                   glyph_SPACE,
                                                   glyph_SPACE,
};

// Show "Arming"
void lcd_show_arming_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , arming_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}

// CLOCK GOOd

constexpr glyph_segment_t clock_good_message[] = {
                                                   glyph_SPACE,
                                                   glyph_C,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_C,
                                                   glyph_K,
                                                   glyph_SPACE,
                                                   glyph_g,
                                                   glyph_O,
                                                   glyph_O,
                                                   glyph_d,
                                                   glyph_SPACE,
};

// Show "CLOCK GOOd"
void lcd_show_clock_good_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , clock_good_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}



// CLOCK LOSt

constexpr glyph_segment_t clock_lost_message[] = {
                                                   glyph_SPACE,
                                                   glyph_C,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_C,
                                                   glyph_K,
                                                   glyph_SPACE,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_SPACE,
};

// Show "CLOCK LOSt"
void lcd_show_clock_lost_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , clock_lost_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}


// Every 100 days

constexpr glyph_segment_t centesimus_dies_message[] = {
    {0x09,0x05},
    {0x08,0x06},
    {0x0f,0x00},
    {0x0b,0x06},
    {0x0f,0x05},
    {0x0b,0x06},
    {0x0f,0x02},
    {0x00,0x00},
    {0x0e,0x04},
    {0x0f,0x05},
    {0x0d,0x03},
    {0x06,0x07},
};

// Refresh day 100's places digits
void lcd_show_centesimus_dies_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , centesimus_dies_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}
