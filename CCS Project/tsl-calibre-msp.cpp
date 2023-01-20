#include <msp430.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#define RV_3032_I2C_ADDR (0b01010001)           // Datasheet 6.6

// RV3032 Registers

#define RV3032_HUNDS_REG 0x00    // Hundredths of seconds
#define RV3032_SECS_REG  0x01
#define RV3032_MINS_REG  0x02
#define RV3032_HOURS_REG 0x03
#define RV3032_DAYS_REG  0x05
#define RV3032_MONS_REG  0x06
#define RV3032_YEARS_REG 0x07


// *** LCD layout

// These values come straight from pin listing the LCD datasheet combined with the MSP430 LCD memory map
// The 1 bit means that the segment + common attached to that pin will lite.
// The higher L pin is always attached to the A-D segments, the lower L pin is attached to E-G
// Remember that COM numbering for LCD starts at 1 while MSP starts at zero
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


// Squiggle segments are used during the Ready To Launch mode before the pin is pulled

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


// This represents a logical digit that we will use to actually display numbers on the screen
// Digit 0 is the rightmost and digit 11 is the leftmost
// LPIN is the name of the pin on the MSP430 that the LCD pin for that digit is connected to.
// Because this is a 4-mux LCD, we need 2 LCD pins for each digit because 2 pins *  4 com lines = 8 segments (7 ofr the 7-segment digit and sometimes one for an indicator)
// These mappings come from our PCB trace layout

struct logical_digit_t {
    const uint8_t lpin_e_thru_g;       // The LPIN for segments E-G
    const uint8_t lpin_a_thru_d;       // The LPIN for segments A-D

};

// Map logical digits on the LCD display to LPINS on the MCU. Each LPIN maps to a single nibble the LCD controller memory.

constexpr uint8_t LOGICAL_DIGITS_SIZE = 12;

constexpr logical_digit_t logical_digits[LOGICAL_DIGITS_SIZE] {
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

#define SECS_ONES_LOGICAL_DIGIT ( 0)
#define SECS_TENS_LOGICAL_DIGIT ( 1)
#define MINS_ONES_LOGICAL_DIGIT ( 2)
#define MINS_TENS_LOGICAL_DIGIT ( 3)

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
constexpr word * LCDMEMW = (word * ) LCDMEM;

// these arrays hold the pre-computed words that we will write to word in LCD memory that
// controls the seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.

#define SECS_PER_MIN 60
word secs_lcd_words[SECS_PER_MIN];

// Make sure that the 4 nibbles that make up the 2 seconds digits are all in the same word in LCD memory
// Note that if you move pins around and are not able to satisfy this requirement, you can not use this optimization and will instead need to manually set the various nibbles in LCDMEM individually.

static_assert( lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");

// Write a value from the array into this word to update the two digits on the LCD display
// It does not matter if we pick ones or tens digit or upper or lower nibble because if they are all in the
// same word. The `>>1` converts the byte pointer into a word pointer.

constexpr word *secs_lcdmem_word = &LCDMEMW[ lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_a_thru_d>::lcdmem_offset() >> 1 ];

#define MINS_PER_HOUR 60
word mins_lcd_words[MINS_PER_HOUR];

static_assert( lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds tens digit LPINs must be in the same LCDMEM word");
static_assert( lpin_t<logical_digits[SECS_ONES_LOGICAL_DIGIT].lpin_a_thru_d >::lcdmem_offset() >> 1 ==  lpin_t<logical_digits[SECS_TENS_LOGICAL_DIGIT].lpin_e_thru_g>::lcdmem_offset() >> 1  , "The seconds ones and tens digits LPINs must be in the same LCDMEM word");


// Write a value from the array into this word to update the two digits on the LCD display
constexpr word *mins_lcdmem_word = &LCDMEMW[ lpin_t<logical_digits[MINS_ONES_LOGICAL_DIGIT].lpin_a_thru_d>::lcdmem_offset() >> 1 ];


// Builds a RAM-based table of words where each word is the value you would assign to a single LCD word address to display a given 2-digit number
// Note that this only works for cases where all 4 of the LPINs for a pair of digits are on consecutive LPINs that use on 2 LCDM addresses.
// In our case, seconds and minutes meet this constraint (not by accident!) so we can do the *vast* majority of all updates efficiently with just a single instruction word assignment.
// Note that if the LPINs are not in the right places, then this will fail by having some unlit segments in some numbers.

// Returns the address in LCDMEM that you should assign a words[] to in order to display the indexed 2 digit number


void fill_lcd_words( word *words , const byte tens_digit_index , const byte ones_digit_index , const byte max_tens_digit , const byte max_ones_digit ) {

    const logical_digit_t tens_logical_digit = logical_digits[tens_digit_index];

    const logical_digit_t ones_logical_digit = logical_digits[ones_digit_index];


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

// Fills the arrays

void initLCDPrecomputedWordArrays() {

    // Note that we need different arrays for the minutes and seconds because , while both have all four LCD pins in the same LCDMEM word,
    // they had to be connected in different orders just due to PCB routing constraints. Of course it would have been great to get them ordered the same
    // way and save some RAM (or even also get all 4 of the hours pin in the same LCDMEM word) but I think we are just lucky that we could get things router so that
    // these two updates are optimized since they account for the VAST majority of all time spent in the CPU active mode.

    // Fill the seconds array
    fill_lcd_words( secs_lcd_words , SECS_TENS_LOGICAL_DIGIT , SECS_ONES_LOGICAL_DIGIT , 6 , 10 );
    // Fill the minutes array
    fill_lcd_words( mins_lcd_words , MINS_TENS_LOGICAL_DIGIT , MINS_ONES_LOGICAL_DIGIT , 6 , 10 );

}


// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show() {

    constexpr uint8_t nibble_a_thru_d =  digit_segments[x].nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    constexpr uint8_t nibble_e_thru_g =  digit_segments[x].nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    constexpr uint8_t lpin_a_thru_d = logical_digits[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    constexpr uint8_t lpin_e_thru_g = logical_digits[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

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


#pragma FUNC_ALWAYS_INLINE
static inline void lcd_show_f( const uint8_t pos, const glyph_segment_t segs ) {

    const uint8_t nibble_a_thru_d =  segs.nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    const uint8_t nibble_e_thru_g =  segs.nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    const uint8_t lpin_a_thru_d = logical_digits[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    const uint8_t lpin_e_thru_g = logical_digits[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

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


void wiggleFlashQ1() {
    SBI( Q1_TOP_LED_POUT , Q1_TOP_LED_B );
    __delay_cycles(10000);
    CBI( Q1_TOP_LED_POUT , Q1_TOP_LED_B );
}

void wiggleFlashQ2() {
    SBI( Q2_BOT_LED_POUT , Q2_BOT_LED_B );
    __delay_cycles(10000);
    CBI( Q2_BOT_LED_POUT , Q2_BOT_LED_B );
}


// Flash the bulbs in succession
// Leaves transistors driven off and flash bulbs off

void flash() {
    wiggleFlashQ1();
    __delay_cycles(30000);
    wiggleFlashQ2();
}

inline void initGPIO() {

    // TODO: use the full word-wide registers
    // Initialize all GPIO pins for low power OUTPUT + LOW
    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    // --- Flash bulbs off

    // Set Q1 & Q2 transistor pins to output (will be driven low by default so LEDs off)
    SBI( Q1_TOP_LED_PDIR , Q1_TOP_LED_B );
    SBI( Q2_BOT_LED_PDIR , Q2_BOT_LED_B );


    // --- TSP LCD voltage regulator

    SBI( TSP_IN_POUT , TSP_IN_B );          // Power up
    SBI( TSP_ENABLE_POUT , TSP_ENABLE_B );  // Enable

    // --- Debug pins

    // Debug pins as output
    SBI( DEBUGA_PDIR , DEBUGA_B );
    SBI( DEBUGB_PDIR , DEBUGB_B );

    // --- RV3032

    CBI( RV3032_CLKOUT_PDIR , RV3032_CLKOUT_B);      // Make the pin connected to RV3032 CLKOUT be input since the RV3032 will power up with CLKOUT running. We will pull it low after RV3032 initialized to disable CLKOUT.

    // ~INT in from RV3032 as INPUT with PULLUP

    CBI( RV3032_INT_PDIR , RV3032_INT_B ); // INPUT
    SBI( RV3032_INT_POUT , RV3032_INT_B ); // Pull-up
    SBI( RV3032_INT_PREN , RV3032_INT_B ); // Pull resistor enable

    // Note that we can leave the RV3032 EVI pin as is - tied LOW on the PCB so it does not float (we do not use it)
    // It is hard tied low so that it does not float during battery changes when the MCU will not be running

    // Power to RV3032

    CBI( RV3032_GND_POUT , RV3032_GND_B);
    SBI( RV3032_GND_PDIR , RV3032_GND_B);

    SBI( RV3032_VCC_PDIR , RV3032_VCC_B);
    SBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now we need to setup interrupt on RTC CLKOUT pin to wake us on each rising edge (1Hz)

    CBI( RV3032_CLKOUT_PDIR , RV3032_CLKOUT_B );      // Set input
    SBI( RV3032_CLKOUT_PIE  , RV3032_CLKOUT_B );      // Interrupt on high-to-low edge (Driven by RV3032 CLKOUT pin which we program to run at 1Hz)
    CBI( RV3032_CLKOUT_PIES , RV3032_CLKOUT_B );      // Enable interrupt on the CLKOUT pin. Calls rtc_isr()

    // --- Trigger

    // Trigger pin as in input with pull-up. This will keep it from floating and will let us detect when the trigger is inserted.

    CBI( TRIGGER_PDIR , TRIGGER_B );      // Set input
    SBI( TRIGGER_PREN , TRIGGER_B );      // Enable pull resistor
    SBI( TRIGGER_POUT , TRIGGER_B );      // Pull up

    // Note that we do not enable the trigger pin interrupt here. It will get enabled when we
    // switch to ready-to-lanch mode when the trigger is inserted at the factory. The interrupt will then get
    // disabled again and the pull up will get disabled after the trigger is pulled since once that happens we
    // dont care about the trigger state anymore and we don't want to waste power with either the pull-up getting shorted to ground
    // if they leave the trigger out, or from unnecessary ISR calls if they put the trigger in and pull it out again (or if it breaks and bounces).


    // Disable IO on the LCD power pins
    SYSCFG2 |= LCDPCTL;

    // Disable the GPIO power-on default high-impedance mode
    // to activate the above configured port settings
    PM5CTL0 &= ~LOCKLPM5;

}

void initLCD() {

    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;                                 // LCD R13/R23/R33/LCDCAP0/LCDCAP1 pins enabled

    // TODO: We can make a template to compute these from logical_digits
    LCDPCTL0 = 0b1110111100111110;  // LCD pins L15-L01, 1=enabled
    LCDPCTL1 = 0b1111110000111111;  // LCD pins L31-L16, 1=enabled
    LCDPCTL2 = 0b0000000000001111;  // LCD pins L35-L32, 1=enabled

    // LCDCTL0 = LCDSSEL_0 | LCDDIV_7;                     // flcd ref freq is xtclk

    // TODO: Try different clocks and dividers

    // Divide by 2 (so CLK will be 10KHz/2= 5KHz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    // Note this has a bit of a flicker
    //LCDCTL0 = LCDDIV_2 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON  ;

    // TODO: Try different clocks and dividers
    // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    //LCDCTL0 = LCDDIV_1 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON  ;

    // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON), Low power saveform
    //LCDCTL0 = LCDDIV_1 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON | LCDLP ;

    // LCD using VLO clock, divide by 4 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform
    LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__4 | LCD4MUX | LCDLP ;


/*
    // Divide by 32 (so CLK will be 32768/32 = ~1KHz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    LCDCTL0 = LCDDIV_7 | LCDSSEL__XTCLK | LCD4MUX | LCDSON | LCDON  ;
*/


    // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz, ~5uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);



    // LCD Operation - internal V1 regulator=3.32v , charge pump 256Hz
    // LCDVCTL = LCDCPEN | LCDREFEN | VLCD_12 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Pin R33 is connected to external Vcc, charge pump 256Hz, 1.7uA
    //LCDVCTL = LCDCPEN | LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Pin R33 is connected to internal Vcc, no charge pump
    //LCDVCTL = LCDSELVDD;


    // LCD Operation - Pin R33 is connected to external V1, charge pump 256Hz, 1.7uA
    //LCDVCTL = LCDCPEN | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Mode 3, internal 2.96v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
    // LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;

    // LCD Operation - Mode 3, internal 3.02v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_7 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - Mode 3, internal 2.78v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_3 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - Mode 3, internal 2.78v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.0uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_3 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - All 3 LCD voltages external. When generating all 3 with regulators, we get 2.48uA @ Vcc=3.5V so not worth it.
    //LCDVCTL = 0;


    // LCD Operation - All 3 LCD voltages external. When generating 1 regulators + 3 1M Ohm resistors, we get 2.9uA @ Vcc=3.5V so not worth it.
    //LCDVCTL = 0;


    // LCD Operation - Charge pump enable, Vlcd=Vcc , charge pump FREQ=256Hz (lowest)  2.5uA - Good for testing without a regulator
    //LCDVCTL = LCDCPEN |  LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    /* WINNER for controlled Vlcd - Uses external TSP7A0228 regulator for Vlcd on R33 */
    // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=256Hz (lowest). 2.1uA/180uA  @ Vcc=3.5V . Vlcd=2.8V  from TPS7A0228 no blinking.
    LCDVCTL = LCDCPEN |  (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    //LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory

    // For TSL
    // Configure COMs and SEGs
    LCDCSSEL0 = LCDCSS8 | LCDCSS9 | LCDCSS10 | LCDCSS11 ;     // L8-L11 are the 4 COM pins
    LCDCSSEL1 = 0x0000;
    LCDCSSEL2 = 0x0000;

    // L08 = MSP_COM3 = LCD_COM3
    // L09 = MSP_COM2 = LCD_COM2
    // L10 = MSP_COM1 = LCD_COM1
    // L11 = MSP_COM0 = LCD_COM0

    // Once we have selected the COM lines above, we have to connect them in the LCD memory. See Figure 17-2 in MSP430FR4x family guide.
    // Each nibble in the LCDMx regs holds 4 bits connecting the L pin to one of the 4 COM lines (two L pins per reg)

    LCDM4 =  0b01001000;  // L09=MSP_COM2  L08=MSP_COM3
    LCDM5 =  0b00010010;  // L10=MSP_COM0  L11=MSP_COM1

    LCDCTL0 |= LCDON;                                           // Turn on LCD

}



// This table of days per month came from the RX8900 datasheet page 9

static uint8_t daysInMonth( uint8_t m , uint8_t y) {

    switch ( m ) {

        case  4:
        case  6:
        case  9:
        case 11:
                    return 30 ;

        case  2:

                    if ( y % 4 == 0 ) {         // "Leap years are correctly handled from 2000 to 2099."
                                                // Empirical testing also shows that 00 is a leap year to the RV3032.
                                                // https://electronics.stackexchange.com/questions/385952/does-the-epson-rx8900-real-time-clock-count-the-year-00-as-a-leap-year

                        return 29 ;             // "February in leap year 01, 02, 03 ... 28, 29, 01

                    } else {

                        return 28;              // February in normal year 01, 02, 03 ... 28, 01, 02
                    }
/*
        case  1:
        case  3:
        case  5:
        case  7:
        case  8:
        case 10:
        case 12:    // Interestingly, we will never hit 12. See why?

*/
        default:
                    return 31 ;

    }

    // unreachable

}

static const unsigned long days_per_century = ( 100UL * 365 ) + 25;       // 25 leap years in every century (RV3032 never counts a leap year on century boundaries)

// Convert the y/m/d values to a count of the number of days since 00/1/1

static unsigned long date_to_days( uint8_t c , uint8_t y , uint8_t m, uint8_t d ) {

    uint32_t dayCount=0;

    // Count days in centuries past

    dayCount += days_per_century * c;

    // Count days in years past this century

    for( uint8_t y_scan = 0; y_scan < y ; y_scan++ ) {

        if ( y_scan % 4 == 0 ) {
            // leap year every 4 years on RX8900
            dayCount += 366UL;      // 366 days per year past in leap years

        } else {

            dayCount += 365UL;      // 365 days per year past in normal years
        }

    }


    // Now accumulate the days in months past so far this year

    for( uint8_t m_scan = 1; m_scan < m ; m_scan++ ) {      // Don't count current month

        dayCount += daysInMonth( m_scan , y );       // Year is needed to count leap day in feb in leap years.


    }

    // Now include the passed days so far this month

    dayCount += (uint32_t) d-1;     // 1st day of a month is 1, so if it is the 1st of the month then no days has elapsed this month yet.

    return dayCount;

}

// RV3032 uses BCD numbers :/

uint8_t bcd2c( uint8_t bcd ) {

    uint8_t tens = bcd/16;
    uint8_t ones = bcd - (tens*16);

    return (tens*10) + ones;
}

uint8_t c2bcd( uint8_t c ) {

    uint8_t tens = c/10;
    uint8_t ones = c - (tens*10);

    return (tens*16) + ones;

}

// Returns with the RV3032...
// 1. Generating a short low pulse on ~INT at 2Hz
// 2. CLKOUT output disabled on RV3032.
// 3. CLKOUT input on RV3032 pulled low.
// 2. i2c pins idle (driven low)
//
// Returns 0=RV3032 has been running no problems, 1=Low power condition detected since last call.

// TODO: Make this set our time variables to clock time
// TODO: Make a function to set the RTC and check if they have been set.
// TODO: Break out a function to close the RTC when we are done interacting with it.

// Time Since Launch time

uint8_t secs=0;
uint8_t mins=0;
uint8_t hours=0;
uint32_t days=0;       // needs to be able to hold up to 1,000,000. I wish we had a 24 bit type here. MSPX has 20 bit addresses, but not available on our chip.

// Returns  0=above time variables have been set to those held in the RTC
//         !0=either RTC has been reset or trigger never pulled.

uint8_t initRV3032() {
    // We will only be using the ~INT signal form the RV3032 to tick our ticker. We will use the periodic timer running at 500ms
    // to generate that tick. It would be better to use the 1s tick (fewer ISR wake-ups) but the width of the pulse is 7.8ms (much wider) for the seconds
    // timebase and that would waste power fighting against the pull-up, so we must use the shorter one which is 122us.
    // Note that the first period can be off by as much as 244.14 us. We will have to live with this - hopefully users will not notice.
    // TODO: We could also use the shorter timebase with a 0.9998s period and then keep track of the error and add a periodic offset. But this
    //       would be ugly when the extra tick happened. :/

    // We will be using the internal diode to charge the backup capacitors connected to Vbackup with these settings...
    // TCM=01 - Connects Vdd to the charging circuit
    // DSM=01 - Direct switchover mode, switches whenever Vbackup>Vcc
    // TCR=00 - Selects a 1K ohm trickle charge resistor

    uint8_t retValue=0;

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Give the RV3230 a chance to wake up before we start pounding it.
    // POR refresh time(1) At power up ~66ms
    __delay_cycles(100000);

    // Set all the registers

    // First control reg. Note that turning off backup switch-over seems to save ~0.1uA
    //uint8_t pmu_reg = 0b01000001;         // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd.

#warning
//    uint8_t pmu_reg = 0b01010000;          // CLKOUT off, Direct backup switching mode, no charge pump, 0.6K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.
    uint8_t pmu_reg = 0b00010000;          // CLKOUT ON, Direct backup switching mode, no charge pump, 0.6K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.

    //uint8_t pmu_reg = 0b01100001;         // CLKOUT off, Level backup switching mode (2v) , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd. Predicted to use ~200nA more than disabled because of voltage monitor.

    //uint8_t pmu_reg = 0b01000000;         // CLKOUT off, Other disabled backup switching mode, no charge pump, trickle resistor off, trickle charge Vbackup to Vdd

#warning
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

#warning
    uint8_t clkout2_reg = 0b01100000;        // XTAL low freq mode, freq=1Hz
    i2c_write( RV_3032_I2C_ADDR , 0xc3 , &clkout2_reg , 1 );


    // Periodic Timer value = 2048 for 500ms
    // It would be nice if we could set it to 1s, but then the pulse is like 7ms wide and that would waste too much
    // power against the pull-up that whole time. We could disable the pull-up on each INT, but then we'd need to wake again
    // to reset it back so mind as well just wake on the 500ms timer.
    // We could also pick a period of 4095/4096ths of a second, but then we'd need to sneak in the extra second every 4095 seconds
    // and that would just look ugly having it jump.
    // We could also maybe use the TI counter and only wake every 2 INTs but that means keeping the MSP430 timer hardware powered up which
    // probably uses more power than just servicing the extra INT every second.

    const unsigned timerValue = 2048;

    uint8_t timerValueH = timerValue / 0x100;
    uint8_t timerValueL = timerValue % 0x100;
    i2c_write( RV_3032_I2C_ADDR , 0x0b , &timerValueL  , 1 );
    i2c_write( RV_3032_I2C_ADDR , 0x0c , &timerValueH  , 1 );

    // Set TIE in Control 2 to 1 t enable interrupt pin for periodic countdown
    uint8_t control2_reg = 0b00010000;
    i2c_write( RV_3032_I2C_ADDR , 0x11 , &control2_reg , 1 );


#warning
    //uint8_t control1_reg = 0b00011100;    // set TD to 00 for 4068Hz timer, TE=1 to enable periodic count-down timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?).
    //uint8_t control1_reg = 0b00011101;    // set TD to 01 for 64Hz timer, TE=1 to enable periodic count-down timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?).
    //uint8_t control1_reg = 0b00011110;    // set TD to 01 for 1Hz timer, TE=1 to enable periodic count-down timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?).

    uint8_t control1_reg = 0b00000100;      // TE=0 so no periodic timer interrupt, EERD=1 to disable automatic EEPROM refresh (why would you want that?).

    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    // OK, we will now get a 122uS low pulse on INT every 500ms.


    // Next check if the RV3032 just powered up, or if it was already running and show a 1 in digit 10 if it was running (0 if not)
    uint8_t status_reg=0xff;
    i2c_read( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    if ( (status_reg & 0x03) != 0x00 ) {               //If power on reset or low voltage drop detected then we have not been running continuously

        // Signal to caller that we have had a low power event.
        retValue = 1;

    } else {

        // Read time

        uint8_t t;

        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &t , 1 );
        secs = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &t , 1 );
        mins = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &t , 1 );
        hours = bcd2c(t);

        uint8_t c;
        uint8_t y;
        uint8_t m;
        uint8_t d;

        i2c_read( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &t , 1 );
        d = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &t , 1 );
        m = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &t , 1 );
        y = bcd2c(t);
        c=0; // TODO: How to get this from RV_3032?

        days = date_to_days(c, y, m, d);

    }


    // We are done setting stuff up with the i2c connection, so drive the pins low
    // This will hopefully reduce leakage and also keep them from floating into the
    // RV3032 when the MSP430 looses power during a battery change.

    i2c_shutdown();

    return retValue;

}

// Test to see if the RV3032 considers (21)00 a leap year
// Note: Confirms that year "00" in a leap year with 29 days.

void testLeapYear() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // Feb 28, 2100 (or 2000)
    // Numbers in BCD
    uint8_t b23 = 0x23;
    uint8_t b59 = 0x59;

    uint8_t b02 = 0x02;
    uint8_t b28 = 0x28;
    uint8_t b00 = 0x00;


    i2c_write( RV_3032_I2C_ADDR , RV3032_HUNDS_REG , &b00 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &b00 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &b59 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &b23 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &b28 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &b02 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &b00 , 1 );

    i2c_shutdown();


    while (1) {


        __delay_cycles(1000000);

        i2c_init();

        uint8_t t;

        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &t , 1 );
        secs = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &t , 1 );
        mins = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &t , 1 );
        hours = bcd2c(t);

        uint8_t y;
        uint8_t m;
        uint8_t d;

        i2c_read( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &t , 1 );
        d = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &t , 1 );
        m = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &t , 1 );
        y = bcd2c(t);

        i2c_shutdown();

        lcd_show_f(  6 , digit_segments[ ( d / 1      ) % 10 ] );
        lcd_show_f(  7 , digit_segments[ ( d / 10     ) % 10 ] );
        lcd_show_f(  8 , digit_segments[ ( m / 1      ) % 10 ] );
        lcd_show_f(  9 , digit_segments[ ( m / 10     ) % 10 ] );
        lcd_show_f( 10 , digit_segments[ ( y / 1      ) % 10 ] );
        lcd_show_f( 11 , digit_segments[ ( y / 10     ) % 10 ] );

        lcd_show_f( 4 , digit_segments[ hours % 10 ] );
        lcd_show_f( 5 , digit_segments[ hours / 10 ] );

        lcd_show_f( 2 , digit_segments[ mins % 10 ] );
        lcd_show_f( 3 , digit_segments[ mins / 10 ] );

        lcd_show_f( 0 , digit_segments[ secs % 10 ] );
        lcd_show_f( 1 , digit_segments[ secs / 10 ] );


    }



}

// Write zeros to all time regs in RV3032

void zeroRV3032() {


    // Initialize our i2c pins as pull-up
    i2c_init();

    // Zero out all of the timekeep regs
    uint8_t zero_byte =0x00;
    uint8_t one_byte =0x01;      // We need this because month and day start at 0

    i2c_write( RV_3032_I2C_ADDR , RV3032_HUNDS_REG , &zero_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &zero_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &zero_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &zero_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &one_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &one_byte , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &zero_byte , 1 );


    // Clear the status regs so we know trigger was pulled.

    uint8_t status_reg =0x00;   // Clear all flags for next time we check. This will clear the low voltage and power up flags that we tested above.
    i2c_write( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    i2c_shutdown();

}


// Sets the TE bit in the control1 register which...
// "The Periodic Countdown Timer starts from the preset Timer Value in the registers 0Bh and 0Ch when
//  writing 1 to the TE bit. The countdown is based on the Timer Clock Frequency selected in the TD field."

#warning switch this to restart clkout
void restart_rv3032_periodic_timer() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // set TD to 00 for 4068Hz timer, TE=1 to enable periodic countdown timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?). Bit 5 not used but must be set to 1.
    uint8_t control1_reg = 0b00011100;
    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    // OK, we will now get a 122uS low pulse on INT after 500m, and every 500ms thereafter

    i2c_shutdown();

}

enum mode_t {
    START,                  // Waiting for pin to be inserted (shows dashes)
    ARMING,                 // Hold-off after pin is inserted to make sure we don't inadvertently trigger on a switch bounce (lasts 0.5s)
    READY_TO_LAUNCH,        // Pin inserted, waiting for it to be removed (shows squiggles)
    TIME_SINCE_LAUNCH       // Pin pulled, currently counting (shows count)
};

mode_t mode;

uint8_t step;          // START             - Used to keep the position of the dash moving across the display (0=leftmost position, only one dash showing)
                    // READY_TO_LAUNCH   - Which step of the squigle animation
                    // TIME_SINCE_LAUNCH - Count the half-seconds (0=1st 500ms, 1=2nd 500ms)


                            // TODO: Make a 24 bit type.


// This ISR is only called when we are in Time Since Launch count up mode
// It just keeps counting up on the screen as efficiently as possible
// until it hits 1,000,000 days, at which time it switches to Long Now mode
// which is permanent.

// TODO: Move to RAM, Move vector table to RAM, write in ASM with autoincrtement register for lcdmem_word pointer, maybe only clean stack once every 60 cycles
// TODO: increment hour + date probably stays in C called from ASM. Maybe pass the hours and days in the call registers so they naturally are maintained across calls?

__attribute__((ramfunc))  // This does not seem to do anything?
__interrupt void rtc_isr_time_since_launch_mode() {

    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    secs++;

     if (secs == 60) {

         secs=0;

     }

    *secs_lcdmem_word = secs_lcd_words[  secs ];

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    CBI( DEBUGA_POUT , DEBUGA_B );
}

// Copies interrupt vector table to RAM and then sets the RTC vector to point to the Time Since Launch mode ISR

void beginTimeSinceLaunchMode() {

}


// Called on RV3032 CLKOUT pin rising edge (1Hz)

#pragma vector = RV3032_CLKOUT_VECTOR
__interrupt void rtc_isr(void) {

    // Wake time measured at 48us
    // TODO: Make sure there are no avoidable push/pops happening at ISR entry (seems too long)

    // TODO: Disable pull-up while in ISR?

    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    if (0) {

        if (secs==0) {
            secs=1;
        } else {
            secs=0;
        }

        *secs_lcdmem_word = secs_lcd_words[  secs ];

        CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.
        CBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs
        return;
    }


    if (  __builtin_expect( mode == TIME_SINCE_LAUNCH , 1 ) ) {            // This is where we will spend most of out life, so optimize for this case

        if (!step) {


            // 31us
            step=1;

            //#warning this is just to visualize the half ticks
            //lcd_show_f( 11 , left_tick_segments );

        } else {

            // 502us
            // 170us

            //#warning this is just to visualize the half ticks
            //lcd_show_f( 11 , right_tick_segments );

            // Show current time

            step = 0;

            secs++;

            if (secs == 60) {

                secs=0;

                mins++;

                if (mins==60) {

                    mins = 0;

                    hours++;

                    if (hours==24) {

                        hours = 0;

                        if (days==999999) {

                            for( uint8_t i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                                // The long now

                                lcd_show_f( i , x_segments );           // TODO: Think of something better to show here.
                                                                        // TODO: Add a "long now" indicator on the LCD

                            }



                        } else {

                            days++;

                            lcd_show_f(  6 , digit_segments[ (days / 1      ) % 10 ] );
                            lcd_show_f(  7 , digit_segments[ (days / 10     ) % 10 ] );
                            lcd_show_f(  8 , digit_segments[ (days / 100    ) % 10 ] );
                            lcd_show_f(  9 , digit_segments[ (days / 1000   ) % 10 ] );
                            lcd_show_f( 10 , digit_segments[ (days / 10000  ) % 10 ] );
                            lcd_show_f( 11 , digit_segments[ (days / 100000 ) % 10 ] );

                        }


                    }

                    lcd_show_f( 4 , digit_segments[ hours % 10 ] );
                    lcd_show_f( 5 , digit_segments[ hours / 10 ] );

                }

                lcd_show_f( 2 , digit_segments[ mins % 10 ] );
                lcd_show_f( 3 , digit_segments[ mins / 10 ] );

            }

            *secs_lcdmem_word = secs_lcd_words[  secs ];

        }


    } else if ( mode==READY_TO_LAUNCH  ) {       // We will be in this mode potentially for a very long time, so also be power efficient here.

        // We depend on the trigger ISR to move use from ready-to-launch to time-since-launch

        uint8_t current_squiggle_step = step;

        static_assert( SQUIGGLE_SEGMENTS_SIZE == 8 , "SQUIGGLE_SEGMENTS_SIZE should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07

        for( uint8_t i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

            if ( i & 0x01 ) {
                lcd_show_f( i , squiggle_segments[ (SQUIGGLE_SEGMENTS_SIZE -  current_squiggle_step) & 0x07 ]);
            } else {
                lcd_show_f( i , squiggle_segments[ current_squiggle_step ]);
            }

        }

        step++;
        step &=0x07;


    } else if ( mode==ARMING ) {

        // This phase is just to add a 500ms debounce to when the trigger is initially inserted at the factory to make sure we do
        // not accidentally fire then.

        // We are currently showing dashes, and we will switch to squiggles on next tick if the trigger is still in

        // If trigger is still inserted (switch open, pin high), then go into READY_TO_LAUNCH were we wait for it to be pulled

        if ( TBI( TRIGGER_PIN , TRIGGER_B )  ) {

            // Now we need to setup interrupt on trigger pull to wake us when pin  goes low
            // This way we can react instantly when they pull the pin.

            SBI( TRIGGER_PIE  , TRIGGER_B );          // Enable interrupt on the INT pin high to low edge. Normally pulled-up when pin is inserted (switch lever is depressed)
            SBI( TRIGGER_PIES , TRIGGER_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)

            // Clear any pending interrupt so we will need a new transition to trigger
            CBI( TRIGGER_PIFG , TRIGGER_B);

            mode = READY_TO_LAUNCH;
            step = SQUIGGLE_SEGMENTS_SIZE-1;        // The ready_to_launch animation starts at the last frame and counts backwards for efficiency

        } else {

            // In the unlikely case where the pin was inserted for less than 500ms, then go back to the beginning and show dash scroll and wait for it to be inserted again.
            // This has the net effect of making an safety interlock that the pin has to be inserted for a full 1 second before we will arm.

            mode = START;
            step = LOGICAL_DIGITS_SIZE+3;       // start at leftmost position

        }


    } else { // if mode==START

        // Check if pin was inserted (pin high)

        if ( TBI( TRIGGER_PIN , TRIGGER_B ) ) {       // Check if trigger has been inserted yet

            // Trigger is in, we can arm now.

            // Show a little full dash screen to indicate that we know you put the pin in
            // TODO: make this constexpr

              for( uint8_t i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                  lcd_show_f( i , dash_segments );

              }


              //Start showing the ready to launch squiggles on next tick

              mode = ARMING;

              // note that here we wait a full tick before checking that the pin is out again. This will debounce the switch for 0.5s when the pin is inserted.

        } else {


            // trigger still out
            // show a little dash sliding towards the trigger

            // In position`step` we draw a trailing space that will erase the tail of the moving dash
            // Remember that positon 11 is the leftmost position and we want the dash to animate to the right to point to the physical trigger switch.

            if (step < LOGICAL_DIGITS_SIZE  ) {

                lcd_show_f( step , blank_segments );

            }


            if (step==0) {

                // The blank made it to the right side, so start over again with the dash past the left side of the display

                step=LOGICAL_DIGITS_SIZE+3;

            } else {


                // Animate one step to the right
                step--;

                // We are no where the left side of the dash will go if it is on the screen
                if (step < LOGICAL_DIGITS_SIZE  ) {

                    lcd_show_f( step , dash_segments );

                }

                // Is there room on the right side of the screen for the right dash?

                if (step > 0 )  {

                    if (step < LOGICAL_DIGITS_SIZE+1 ) {

                        lcd_show_f( step-1 , dash_segments );

                    }
                }

            }

        }

    }


    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    CBI( DEBUGA_POUT , DEBUGA_B );

}

// TODO: Try moving ISR and vector table into RAM to save 10us on wake and some amount of power for FRAM controller
//       FRAM controller automatically turned off when entering LMP3 and automatically turned back on at first FRAM access.

// Called when trigger pin changes high to low, indicating the trigger has been pulled and we should start ticking.
// Note that this interrupt is only enabled when we enter ready-to-launch mode, and then it is disabled and also the pin in driven low
// when we then switch to time-since-lanuch mode, so this ISR can only get called in ready-to-lanuch mode.

#pragma vector = TRIGGER_VECTOR

__interrupt void trigger_isr(void) {

    #warning
    SBI( DEBUGA_POUT , DEBUGA_B );

    // First we delay for about 1000 / 1.1Mhz  = ~1ms
    // This will filter glitches since the pin will not still be low when we sample it after this delay
    __delay_cycles( 1000 );

    // Grab the current trigger pin level

    unsigned triggerpin_level = TBI( TRIGGER_PIN , TRIGGER_B );

    // Then clear any interrupt flag that got us here.
    // Timing here is critical, we must clear the flag *after* we capture the pin state or we might mess a change

    CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending trigger pin interrupt flag

    // Now we can check to see if the trigger pin was set
    // Note there is a bit of a glitch filter here since the pin must stay low long enough after it sets the interrupt flag for
    // us to also see it low above after the ISR latency - otherwise it will be ignored. In practice I have seen glitches on this pin that are this short.

    if ( !triggerpin_level ) {      // was pin low when we sampled it?

        // WE HAVE LIFTOFF!

        // We need to get everything below done within 0.5 seconds so we do not miss the first tick.

        // Disable the trigger pin and drive low to save power and prevent any more interrupts from it
        // (if it stayed pulled up then it would draw power though the pull-up resistor whenever the pin was inserted)

        CBI( TRIGGER_POUT , TRIGGER_B );      // low
        SBI( TRIGGER_PDIR , TRIGGER_B );      // drive

        CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending interrupt from us driving it low (it could have gone high again since we sampled it)


        // Show all 0's on the LCD to instantly let the user know that we saw the pull

        #pragma UNROLL( LOGICAL_DIGITS_SIZE )

        for( uint8_t i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

            lcd_show_f( i , digit_segments[0] );

        }

        // Reset the RV3032's internal timer
        // Start counting the current second over again from right..... now

        restart_rv3032_periodic_timer();

        // We will get the next tick in 500ms. Make sure we return from this ISR within 500ms from now. (we really could wait up to 999ms since that would just delay the half tick and not miss the following real tick).

        // Clear any pending RTC interrupt so we will not tick until the next pulse comes from RTC in 500ms
        // There could be a race where an RTC interrupt comes in just after the trigger was pulled, in which case we would
        // have a pending interrupt that would get serviced immediately after we return from here, which would look ugly.

        CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending interrupt from us driving it low (it could have gone high again since we sampled it)

        // Flash lights

        flash();

        // Record timestamp

        zeroRV3032();

        // Start ticking!

        mode = TIME_SINCE_LAUNCH;
        step = 0;           // Start at the top of the first second.

    }


    CBI( DEBUGA_POUT , DEBUGA_B );

}

// The ISR we will put into the RAM-based vector table once we switch to TSL mode.
// Once we enter TSL mode, we never leave and we never see any other interrupt besides RTC
// so this is a no brainer. Running from RAM should be slightly lower power because (1) RAM
// access is slightly lower power than FRAM and, (2) if we never touch FRAM after waking from LMP3
// then the FRAM controller will stay disabled.

/*
    Save-on-entry registers. Registers R4-R10. It is the called function's responsibility to preserve the values
    in these registers. If the called function modifies these registers, it saves them when it gains control and
    preserves them when it returns control to the calling function.

    We will use these regs to efficiently hold the timer values, knowing that we can call into a C function if we
    need to (like to update hours and days) and it will not trash them.

    The caller places the first arguments in registers R12-R15, in that order. The caller moves the remaining
    arguments to the argument block in reverse order, placing the leftmost remaining argument at the lowest
    address. Thus, the leftmost remaining argument is placed at the top of the stack. An argument with a type
    larger than 16 bits that would start in a save-on-call register may be split between R15 and the stack.

    Functions defined in C++ that must be called in asm must be defined extern "C", and functions defined in
    asm that must be called in C++ must be prototyped extern "C" in the C++ file.

 */

__attribute__((ramfunc))
__interrupt void tsl_isr(void) {

}

int main( void )
{
    WDTCTL = WDTPW | WDTHOLD | WDTSSEL__VLO;   // Give WD password, Stop watchdog timer, set WD source to VLO
                                               // The thinking is that maybe the watchdog will request the SMCLK even though it is stopped (this is implied by the datasheet flowchart)
                                               // Since we have to have VLO on for LCD anyway, mind as well point the WDT to it.
                                               // TODO: Test to see if it matters, although no reason to change it.

    initGPIO();

    initLCD();

    initLCDPrecomputedWordArrays();

    // Power up display with a test pattern so we are not showing garbage

    lcd_show_f( 0, digit_segments[0] );
    lcd_show_f( 1, digit_segments[1] );
    lcd_show_f( 2, digit_segments[2] );
    lcd_show_f( 3, digit_segments[3] );
    lcd_show_f( 4, digit_segments[4] );
    lcd_show_f( 5, digit_segments[5] );
    lcd_show_f( 6, digit_segments[6] );
    lcd_show_f( 7, digit_segments[7] );
    lcd_show_f( 8, digit_segments[8] );
    lcd_show_f( 9, digit_segments[9] );
    lcd_show_f(10, digit_segments[0x0a] );
    lcd_show_f(11, digit_segments[0x0b] );


    // Setup the RV3032
    uint8_t rtcLowVoltageFlag = initRV3032();

    if (rtcLowVoltageFlag) {

        // We are starting fresh


        // TODO: Only enable the pull-up once per second when we check it

        mode = START;
        step = LOGICAL_DIGITS_SIZE+3;           // Start the dash animation at the leftmost position

    } else {

        // We have already been triggered and time is already loaded into variables

        // Disable the pull-up

        // Disable the trigger pin and drive low to save power and prevent any more interrupts from it
        // (if it stayed pulled up then it would draw power though the pull-up resistor whenever the pin was inserted)

        CBI( TRIGGER_POUT , TRIGGER_B );      // low
        SBI( TRIGGER_PDIR , TRIGGER_B );      // drive

        CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending interrupt from us driving it low (it could have gone high again since we sampled it)

        // Show the time on the display
        // Note that the time was loaded from the RTC when we initialized it.
        // We only do this ONCE so no need for efficiency.

        lcd_show_f(  6 , digit_segments[ (days / 1      ) % 10 ] );
        lcd_show_f(  7 , digit_segments[ (days / 10     ) % 10 ] );
        lcd_show_f(  8 , digit_segments[ (days / 100    ) % 10 ] );
        lcd_show_f(  9 , digit_segments[ (days / 1000   ) % 10 ] );
        lcd_show_f( 10 , digit_segments[ (days / 10000  ) % 10 ] );
        lcd_show_f( 11 , digit_segments[ (days / 100000 ) % 10 ] );

        lcd_show_f( 4 , digit_segments[ hours % 10 ] );
        lcd_show_f( 5 , digit_segments[ hours / 10 ] );

        lcd_show_f( 2 , digit_segments[ mins % 10 ] );
        lcd_show_f( 3 , digit_segments[ mins / 10 ] );

        lcd_show_f( 0 , digit_segments[ secs % 10 ] );
        lcd_show_f( 1 , digit_segments[ secs / 10 ] );


        // Now start ticking!

        mode = TIME_SINCE_LAUNCH;
        step = 0;           // Start at the top of the first second.
    }

    // Clear any pending interrupts from the RV3032 INT pin
    // We just started the timer so we should not get a real one for 500ms

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );


    // Go into start mode which will wait for the trigger to be inserted.
    // We don;t have to worry about any races here because once we do see the trigger is in then we go into
    // ARMING mode which waits an additional 500ms to debounce the switch


    // Go into LPM sleep with Voltage Supervisor disabled.
    // This code from LPM_4_5_2.c from TI resources
    PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
    PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS
    // LPM4 SVS=OFF

    __bis_SR_register(LPM4_bits | GIE );                 // Enter LPM4

    __no_operation();                                   // For debugger




    int step=0;
    while (1) {

        lcd_show_f(  6 , digit_segments[ step++ % 10 ] );
        __bis_SR_register(LPM4_bits  );                 // Enter LPM4

    }


    // We should never get here

    // TODO: Show an error message


}
