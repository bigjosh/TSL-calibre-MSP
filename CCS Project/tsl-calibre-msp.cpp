#include <msp430.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#define RV_3032_I2C_ADDR (0b01010001)           // Datasheet 6.6


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


// Now we draw the digits using the segments as mapped in the LCD datasheet

struct glyph_segment_t {
    const char nibble_a_thru_d;     // The COM bits for the digit's A-D segments
    const char nibble_e_thru_g;     // The COM bits for the digit's E-G segments
};


constexpr glyph_segment_t digit_segments[] = {

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
    const char lpin_e_thru_g;       // The LPIN for segments E-G
    const char lpin_a_thru_d;       // The LPIN for segments A-D

};

constexpr char LOGICAL_DIGITS_SIZE = 12;

constexpr logical_digit_t logical_digits[LOGICAL_DIGITS_SIZE] {
    { 33 , 32 },        //  0 (LCD 12) - rightmost digit
    { 35 , 34 },        //  1 (LCD 11)
    { 30 , 31 },        //  2 (LCD 10)
    { 28 , 29 },        //  3 {LCD 09)
    { 26 , 27 },        //  4 {LCD 08)
    { 24 , 25 },        //  5 {LCD 07)
    { 17 , 21 },        //  6 (LCD 06)
    { 15 , 16 },        //  7 (LCD 05)
    { 13 , 14 },        //  8 (LCD 04)
    {  1 , 12 },        //  9 (LCD 03)
    {  3 ,  2 },        // 10 (LCD 02)
    {  5 ,  4 },        // 11 (LCD 01) - leftmost digit
    // Rest TBD
};

// Returns the byte address for the specified L-pin
// Assumes MSP430 LCD is in 4-Mux mode
// This mapping comes from the MSP430FR2xx datasheet Fig 17-2

enum nibble_t {LOWER,UPPER};

template <char lpin>
struct lpin_t {
    constexpr static char lcdmem_offset() {
        return (lpin>>1); // Intentionally looses bottom bit since consecutive L-pins share the same memory address (but different nibbles)
    };

    constexpr static nibble_t nibble() {
        return lpin & 0x01 ? UPPER : LOWER;  // Extract which nibble
    }
};

// Each LCD lpin has one nibble, so there are two lpins for each LCD memory address.
// These two functions compute the memory address and the nibble inside that address for a given lpin.

#pragma FUNC_ALWAYS_INLINE
constexpr static char lpin_lcdmem_offset(const char lpin) {
    return (lpin>>1); // Intentionally looses bottom bit since consecutive L-pins share the same memory address (but different nibbles)
}

#pragma FUNC_ALWAYS_INLINE
constexpr static nibble_t lpin_nibble(const char lpin) {
    return lpin & 0x01 ? UPPER : LOWER;  // Extract which nibble
}




// Write the specified nibble. The other nibble at address a is unchanged.

void set_nibble( char * a , const nibble_t nibble_index , const char x ) {

    if ( nibble_index == nibble_t::LOWER  ) {

        *a  = (*a & 0b11110000 ) | (x);     // Combine the nibbles and write back to the address

    } else {


        *a  = (*a & 0b00001111 ) | (x<<4);     // Combine the nibbles and write back to the address

    }

}



// Show the digit x at position p
// where p=0 is the rightmost digit

template <char pos, char x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show() {

    constexpr char nibble_a_thru_d =  digit_segments[x].nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    constexpr char nibble_e_thru_g =  digit_segments[x].nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    constexpr char lpin_a_thru_d = logical_digits[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    constexpr char lpin_e_thru_g = logical_digits[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

    constexpr char lcdmem_offset_a_thru_d = (lpin_t< lpin_a_thru_d >::lcdmem_offset()); // Look up the memory address for the low segment bits
    constexpr char lcdmem_offset_e_thru_g = lpin_t< lpin_e_thru_g >::lcdmem_offset(); // Look up the memory address for the high segment bits

    char * const lcd_mem_reg = LCDMEM;

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

        const char lpin_a_thru_d_nibble = lpin_t< lpin_a_thru_d >::nibble();

        if ( lpin_a_thru_d_nibble == nibble_t::LOWER  ) {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (char) ( nibble_e_thru_g << 4 ) | (nibble_a_thru_d);     // Combine the nibbles, write them to the mem address

        } else {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (char) ( nibble_a_thru_d << 4 ) | (nibble_e_thru_g);     // Combine the nibbles, write them to the mem address

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
static inline void lcd_show_f( const char pos, const glyph_segment_t segs ) {

    const char nibble_a_thru_d =  segs.nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    const char nibble_e_thru_g =  segs.nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    const char lpin_a_thru_d = logical_digits[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    const char lpin_e_thru_g = logical_digits[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

    const char lcdmem_offset_a_thru_d = lpin_lcdmem_offset( lpin_a_thru_d ); // Look up the memory address for the low segment bits
    const char lcdmem_offset_e_thru_g = lpin_lcdmem_offset( lpin_e_thru_g ); // Look up the memory address for the high segment bits

    char * const lcd_mem_reg = LCDMEM;

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

        const char lpin_a_thru_d_nibble = lpin_nibble( lpin_a_thru_d );

        if ( lpin_a_thru_d_nibble == nibble_t::LOWER  ) {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (char) ( nibble_e_thru_g << 4 ) | (nibble_a_thru_d);     // Combine the nibbles, write them to the mem address

        } else {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (char) ( nibble_a_thru_d << 4 ) | (nibble_e_thru_g);     // Combine the nibbles, write them to the mem address

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

volatile unsigned char * Seconds = &BAKMEM0_L;               // Store seconds in the backup RAM module
volatile unsigned char * Minutes = &BAKMEM0_H;               // Store minutes in the backup RAM module
volatile unsigned char * Hours = &BAKMEM1_L;                 // Store hours in the backup RAM module
volatile unsigned char * spin = &BAKMEM1_H;                 // Store hours in the backup RAM module
volatile unsigned char * triggerSpin = &BAKMEM2_L;          // Current state of the trigger pin. Normally pulled up (lever depressed), low means pin pulled

inline void initGPIO() {

    // TODO: use the full word-wide registers
    // Initialize all GPIO pins for low power OUTPUT + LOW
    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    // --- Flash bulbs off

    // Set Q1 & Q2 transistor pins to output (will be driven low so LEDs off)
    SBI( Q1_TOP_LED_PDIR , Q1_TOP_LED_B );
    SBI( Q2_BOT_LED_PDIR , Q2_BOT_LED_B );


    // --- TSP LCD voltage regulator

    SBI( TSP_IN_POUT , TSP_IN_B );          // Power up
    SBI( TSP_ENABLE_POUT , TSP_ENABLE_B );  // Enable

    // --- Debug pins

    // DEBUGA as output
    // SBI( DEBUGA_PDIR , DEBUGA_B );

    // --- RV3032

    CBI( RV3032_CLKOUT_PDIR,RV3032_CLKOUT_B);      // Make the pin connected to RV3032 CLKOUT be input since the RV3032 will power up with CLKOUT running. We will pull it low after RV3032 initialized to disable CLKOUT.

    // ~INT in from RV3032 as INPUT with PULLUP

    CBI( RV3032_INT_PDIR , RV3032_INT_B ); // INPUT
    SBI( RV3032_INT_POUT , RV3032_INT_B ); // Pull-up
    SBI( RV3032_INT_PREN , RV3032_INT_B ); // Pull resistor enable


    // Note that we can leave the RV3032 EVI pin as is - tied LOW on the PCB so it does not float (we do not use it)

    // Power to RV3032

    CBI( RV3032_GND_POUT , RV3032_GND_B);
    SBI( RV3032_GND_PDIR , RV3032_GND_B);

    SBI( RV3032_VCC_PDIR , RV3032_VCC_B);
    SBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now we need to setup interrupt on INT pin to wake us when it goes low

    SBI( RV3032_INT_PIES , RV3032_INT_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)
    SBI( RV3032_INT_PIE  , RV3032_INT_B );          // Enable interrupt on the INT pin high to low edge.


    // --- Trigger Pin

    // Trigger pin as in input with pull-up

    CBI( TRGIGER_SWITCH_PDIR , TRIGGER_SWITCH_B );      // Set input
    SBI( TRGIGER_SWITCH_PREN , TRIGGER_SWITCH_B );      // Enable pull resistor
    SBI( TRGIGER_SWITCH_POUT , TRIGGER_SWITCH_B );      // Pull up

    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;


    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

}


// Sets the TE bit in the control1 register which...
// "The Periodic Countdown Timer starts from the preset Timer Value in the registers 0Bh and 0Ch when
//  writing 1 to the TE bit. The countdown is based on the Timer Clock Frequency selected in the TD field."

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

char next_dash_step = 0;        // Used in UNARMED mode to show the sliding dash

char next_squiggle_step = SQUIGGLE_SEGMENTS_SIZE-1 ;     // Used in Ready to Launch mode, indicates the step of the squiggle in the leftmost digit.


char secs=0;        // Start a 1 seconds on first update after launch
char mins=0;
char hours=0;
unsigned long days=0;       // needs to be able to hold up to 1,000,000. I wish we had a 24 bit type here.
                            // TODO: Make a 24 bit type.
char halfsec=0;     // We tick at 2Hz, so keep track of half seconds

int main( void )
{
    WDTCTL = WDTPW | WDTHOLD | WDTSSEL__VLO;   // Give WD password, Stop watchdog timer, set source to VLO
                                               // The thinking is that maybe the watchdog will request the SMCLK even though it is stopped (this is implied by the datasheet flowchart)
                                               // Since we have to have VLO on anyway, mind as well point the WDT to it.
                                               // TODO: Test to see if it matters, although no reason to change it.
    #warning
    if (SYSRSTIV == SYSRSTIV_LPM5WU)        // MSP430 just woke up from LPMx.5
    {

        initGPIO();
        SBI( DEBUGA_POUT , DEBUGA_B);

        CBI( RV3032_INT_PIFG     , RV3032_INT_B    );
        CBI( TRIGGER_SWITCH_PIFG , TRIGGER_SWITCH_B);

        (*Seconds)++;
        if (*Seconds & 0x01 ) {
            lcd_show_f( 0, digit_segments[3] );
        } else {
            lcd_show_f( 0, digit_segments[4] );
        }

        // Go into LPMx.5 sleep with Voltage Supervisor disabled.


        // This code from LPM_4_5_2.c from TI resources
        PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
        //PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS
        PMMCTL0_L |= PMMREGOFF;             // and set PMMREGOFF
        // LPM4.5 SVS=OFF 1.471uA
        // TODO: datasheets say SVS off makes wakeup 10x slower, so need to test when wake works


        // LPM4 - 2.10uA with static @ Vcc=3.47V


        /*
            The LMPx.5 modes are very not worth it for us since they waste so much power on extended wake up time.
            They would only be worth it if you were sleeping >minutes, not 500ms like we are. So we just settle for
            LMP4.
        */

        #warning
        CBI( DEBUGA_POUT , DEBUGA_B);
        __bis_SR_register(LPM4_bits | GIE);                 // Enter LPM
        __no_operation();

    }


    initGPIO();

    /*

    // At startup, all IO pins are hi-Z. Here we init them all and then unlock them for the changes to take effect.

    //TODO: Init our own stack save the wasteful init
    //STACK_INIT();


    // TODO: use the full word-wide registers
    // Initialize all GPIO pins for low power OUTPUT + LOW
    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    // ~INT in from RV3032 as INPUT with PULLUP

    CBI( RV3032_INT_PDIR , RV3032_INT_B ); // INPUT
    SBI( RV3032_INT_POUT , RV3032_INT_B ); // Pull-up
    SBI( RV3032_INT_PREN , RV3032_INT_B ); // Pull resistor enable


    // Debug pins
    SBI( DEBUGA_PDIR , DEBUGA_B );

    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    */

/*
    SBI( P1DIR , 1 );
    SBI( P1OUT , 1 );

*/

    /*
        // This code is for testing LCD individual segments, it outputs out of phase square waves on MSP430 pins 1 and 2.
        // Connect one to SEG and one to COM and the segment should lite.
        while (1) {
            P7OUT = 0b10101010;
            __delay_cycles( 10000 );    // 100Hz
            P7OUT = 0b01010101;
            __delay_cycles( 10000 );    // 100Hz
        }
    */


    //RTCCTL = RTCSS__VLOCLK ;                    // Initialize RTC to use VLO clock

    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;                                 // LCD R13/R23/R33/LCDCAP0/LCDCAP1 pins enabled

    // TODO: We can make a template to compute these from logical_digits
    LCDPCTL0 = 0b1111111111111110;  // LCD pins L15-L01, 1=enabled
    LCDPCTL1 = 0b1111111111100011;  // LCD pins L31-20, L17-L16, 1=enabled
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
    LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;

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

/*
    // For MSP430FR4133-EXP
     *
    LCDCSSEL0 = 0x000F;                                 // Configure COMs and SEGs
    LCDCSSEL1 = 0x0000;                                 // L0, L1, L2, L3: COM pins
    LCDCSSEL2 = 0x0000;

    LCDM0 = 0x21;                                       // L0 = COM0, L1 = COM1
    LCDM1 = 0x84;                                       // L2 = COM2, L3 = COM3
*/

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
    //wiggleFlash();

    *Seconds=00;        // Make it so we can see if we woke


/*

    // Set up RTC



    RTCMOD = 10000;                                     // Set RTC modulo to 10000 to trigger interrupt each second on 10Khz VLO clock. This will get loaded into the shadow register on next trigger or reset
//        RTCMOD = 10000;                                     // Set RTC modulo to 1000 to trigger interrupt each 0.1 second on 10Khz VLO clock. This will get loaded into the shadow register on next trigger or reset

    RTCCTL |=  RTCSR;                    // reset RTC which will load overflow value into shadow reg and reset the counter to 0,  and generate RTC interrupt

    RTCIV;                      // Clear any pending RTC interrupt

    // TRY THIS, MUST ALSO ADJUST LCDSEL
    // Use VLO instead of XTAL
    //RTCCTL = RTCSS__VLOCLK | RTCIE;                    // Initialize RTC to use Very Low Oscillator and enable RTC interrupt on rollover
    RTCCTL = 0x00;                    // Disable RTC

*/



    // Setup the RV3032

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

    // Give the RV3230 a chance to wake up before we start pounding it.
    // POR refresh time(1) At power up ~66ms
    __delay_cycles(100000);

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Set all the registers

    // First control reg. Note that turning off backup switch-over seems to save ~0.1uA
    //uint8_t pmu_reg = 0b01000001;         // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd.
    uint8_t pmu_reg = 0b01010001;         // CLKOUT off, Direct backup switching mode, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.
    //uint8_t pmu_reg = 0b01100001;         // CLKOUT off, Level backup switching mode (2v) , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd

    //uint8_t pmu_reg = 0b01110001;         // CLKOUT off, Other disabled backup switching mode  , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd

    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );


    // Now that CLKOUT is disabled on the RV3032, it is supposed to be driven low by that chip, but seems to float sometimes
    // so lets pull it low on the MSP430 to prevent float leakage.
    CBI( RV3032_CLKOUT_POUT , RV3032_CLKOUT_B );
    SBI( RV3032_CLKOUT_PREN , RV3032_CLKOUT_B );


    // Periodic Timer value = 2048 for 500ms
    // It would be nice if we could set it to 1s, but then the pulse is like 7ms wide and that would waste too much
    // power against the pull-up that whole time. We could disable the pull-up on each INT, but then we'd need to wake again
    // to reset it back so mind as well just wake on the 500ms timer.
    // We could also pick a period of 4095/4096ths of a second, but then we'd need to sneak in the extra second every 4095 seconds
    // and that would just look ugly having it jump.
    // We could also maybe use the TI counter and only wake every 2 INTs but that means keeping the MSP430 timer hardware powered up which
    // probably uses more power than just servicing the extra INT every second.

//    const unsigned timerValue = 2048;
#warning for testing only
    const unsigned timerValue = 200;

    uint8_t timerValueH = timerValue / 0x100;
    uint8_t timerValueL = timerValue % 0x100;
    i2c_write( RV_3032_I2C_ADDR , 0x0b , &timerValueL  , 1 );
    i2c_write( RV_3032_I2C_ADDR , 0x0c , &timerValueH  , 1 );

    // Set TIE in Control 2 to 1 t enable interrupt pin for periodic countdown
    uint8_t control2_reg = 0b00010000;
    i2c_write( RV_3032_I2C_ADDR , 0x11 , &control2_reg , 1 );

    // set TD to 00 for 4068Hz timer, TE=1 to enable periodic countdown timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?). Bit 5 not used but must be set to 1.
    uint8_t control1_reg = 0b00011100;
    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    // OK, we will now get a 122uS low pulse on INT every 500ms.


    // Next check if the RV3032 just powered up, or if it was already running and show a 1 in digit 10 if it was running (0 if not)
    uint8_t status_reg=0xff;
    i2c_read( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    if ( (status_reg & 0x03) == 0 ) {               //If power on reset or low voltage drop detected then we have not been running continuously

        // RESET or errormessage here

    }
    status_reg =0x00;   // Clear all flags for next time we check
    i2c_write( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );


    // Next check if the RV3032 just powered up, or if it was already running and show a 1 in digit 10 if it was running (0 if not)
    uint8_t sec_reg=0xff;
    i2c_read( RV_3032_I2C_ADDR , 0x01 , &sec_reg , 1 );

    /*
    lcd_show_f( 11 , sec_reg / 0x10  );
    lcd_show_f( 10 , sec_reg & 0x0f );
    */

    i2c_write( RV_3032_I2C_ADDR , 0x01 , &sec_reg , 1 );


    // We are done setting stuff up with the i2c connection, so drive the pins low
    // This will hopefully reduce leakage and also keep them from floating into the
    // RV3032 when the MSP430 looses power during a battery change.

    i2c_shutdown();

    // TODO: Why doesn't this work?
    //LCDMEMCTL |= LCDCLRM;                               // Clear LCD memory command (executed one shot)
    //while (LCDMEMCTL & LCDCLRM);                        // Wait for clear to complete before moving on
/*
    lcd_show< 0,0>();
    lcd_show< 1,1>();
    lcd_show< 2,2>();
    lcd_show< 3,3>();
    lcd_show< 4,4>();
    lcd_show< 5,5>();
    lcd_show< 6,6>();
    lcd_show< 7,7>();
    lcd_show< 8,8>();
    lcd_show< 9,9>();
    lcd_show<10,0>();
    lcd_show<11,1>();
*/

    /*
    unsigned f = PMMIFG;        // Read reset flags

    if (f & PMMLPM5IFG) {       // Wake from LPMx.5
        lcd_show< 7,1>();
    }


    if (f & SVSHIFG) {          // SVS low side interrupt flag (power up)
        lcd_show< 6,1>();
    }


    if (f & PMMPORIFG) {
        lcd_show< 5,1>();
    }

    if (f & PMMPORIFG) {
        lcd_show< 4,1>();
    }

    if (f & PMMRSTIFG) {            // Reset pin
          lcd_show< 3,1>();
      }


    if (f & PMMBORIFG) {
          lcd_show< 2,1>();
    }


    SYSRSTIV = 0x00;
    unsigned iv = SYSRSTIV;

    if (iv!=0x0000) {
        lcd_show< 8,1>();
    }

    */

    //Enter LPM3 with SVS off
    //PMMCTL0_L &= ~SVSHE ;                           // disable SVS (prob ~0.2uA, default is on)
    //TODO:Turn off LPM3.5 switch before sleeping? //LPM5SW

    // Datasheet says this will be 1.25uA with LCD...
    // Low-power mode 3, VLO, excludes SVS test conditions:
    // Current for watchdog timer clocked by VLO included. RTC disabled. Current for brownout included. SVS disabled (SVSHE = 0).
    // CPUOFF = 1, SCG0 = 1 SCG1 = 1, OSCOFF = 0 (LPM3),

    // "the WDTHOLD bit can be used to hold the WDTCNT, reducing power consumption."


    //WDTCTL = WDTPW | WDTSSEL__VLO | WDTHOLD;                               // Select VLO for WDT clock, halt watchdog timer


    // "After reset, RTCSS defaults to 00b (disabled), which means that no clock source is selected."



/*
    // Disabling the System Voltage Supervisor is supposed to save us about 2uA, but no effect found in testing. If anything, seems to add about 0.01uA.

    PMMCTL0_H = PMMPW_H;            // Open PMM Registers for write
    CBI( PMMCTL0_L  , SVSHE ) ;     // 0b = High-side SVS (SVSH) is disabled in LPM2, LPM3, LPM4, LPM3.5, and LPM4.5 (prob ~0.2uA, default is on)

*/


    // TODO: Try switching MCLK to VLO. We will run really slow.... but maybe save power?
    // CSCTL4 = SELMS__VLOCLK;
    // Nope, uses MORE power 4.5uA vs 2.3uA. Maybe the FLO clock is still running?



    // Clear any pending interrupts from the RV3032 INT pin
    // We just started the timer so we should not get a real one for 500ms

    CBI( RV3032_INT_PIFG     , RV3032_INT_B    );



    // Go into ready to launch mode (Show squiggles until trigger pin pulled)
    // We always start in START mode even if the trigger pin is inserted. This gives 500ms for the pull-on to actually pull it up,
    // durring which we show a test pattern on the LCD.
    // If we see the pin is still low on our first pass after 500ms, then we will go into ARMING, ARMED, and then READY_TO_LAUNCH. (500ms between each)

    mode = START;

    // Switch to MCLK = VLO

    // With VLO         10kHz , the ISR for the scroll pattern takes 91ms at 80.7uA
    // With DCOCLKDIV 1000kHz , the ISR for the scroll pattern takes  1ms at 281uA
    // CSCTL4 =  SELA__REFOCLK | SELMS__VLOCLK;            /* ACLK Source Select REFOCLK , MCLK and SMCLK Source Select VLOCLK */
    // TODO: Maybe we should go even faster? ISR currently takes about 0.5uA so might help but we can also optimize that down alot.


    // All LPM power tests done with all digits '0' on the display, no interrupts, RTC disconnected, Vcc=3.47V

/*

    // Go into LPMx.5 sleep with Voltage Supervisor left enabled.
    PMMCTL0_H = PMMPW_H;                               // Open PMM Registers for write
    PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF also clears SVS
    // LPM4.5 SVS=ON 1.625uA

*/


/*
    // Go into LPMx.5 sleep with Voltage Supervisor disabled.
    // This code from LPM_4_5_2.c from TI resources
    PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
    PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS
    PMMCTL0_L |= PMMREGOFF;             // and set PMMREGOFF
    // LPM4.5 SVS=OFF 1.471uA
    // TODO: datasheets say SVS off makes wakeup 10x slower, so need to test when wake works
*/

    // LPM4 - 2.10uA with static @ Vcc=3.47V




    // Go into LPMx.5 sleep with Voltage Supervisor disabled.
    // This code from LPM_4_5_2.c from TI resources
    PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
    PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS
    // LPM4 SVS=OFF


    /*
        The LMPx.5 modes are very not worth it for us since they waste so much power on extended wake up time.
        They would only be worth it if you were sleeping >minutes, not 500ms like we are. So we just settle for
        LMP4.
    */
    #warning
    CBI( DEBUGA_POUT , DEBUGA_B);
    __bis_SR_register(LPM4_bits | GIE);                 // Enter LPM
    __no_operation();                                   // For debugger


    while (1) {

        lcd_show< 9,5>();
        __delay_cycles(1000000);
        lcd_show< 9,6>();
        __delay_cycles(1000000);

    }

}


#pragma vector = PORT1_VECTOR

__interrupt void PORT1_ISR(void) {

    // Wake time measured at 48us
    // TODO: Make sure there are no avoidable push/pops happening at ISR entry (seems too long)

    // TODO: Disable pull-up while in ISR


    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    if (mode == TIME_SINCE_LAUNCH) {

        if (!halfsec) {

            halfsec=1;

            #warning this is just to visualize the half ticks
            lcd_show_f( 11 , left_tick_segments );

        } else {

            #warning this is just to visualize the half ticks
            lcd_show_f( 11 , right_tick_segments );


            // Show current time

            halfsec = 0;

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

                            for( char i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

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


            lcd_show_f( 0 , digit_segments[ secs % 10 ] );
            lcd_show_f( 1 , digit_segments[ secs / 10 ] );


        }


    } else if ( mode==READY_TO_LAUNCH ) {

        // First grab the instantaneous trigger pin level

        unsigned triggerpin_level = TBI( TRGIGER_SWITCH_PIN , TRIGGER_SWITCH_B );

        // Then clear any interrupt flag that got us here.
        // Timing here is critical, we must clear the flag *after* we capture the pin state or we might mess a change

        CBI( TRIGGER_SWITCH_PIFG , TRIGGER_SWITCH_B );      // Clear any pending trigger pin interrupt flag

        // Now we can check to see if the trigger pin was set
        // Note there is a bit of a glitch filter here since the pin must stay low long enough after it sets the interrupt flag for
        // us to also see it low above after the ISR latency - otherwise it will be ignored. In practice I have seen glitches on this pin that are this short.

        if ( !triggerpin_level ) {      // was pin low when we sampled it?

            // WE HAVE LIFTOFF!

            // We need to get everything below done within 0.5 seconds so we do not miss the first tick.

            // Disable the trigger pin and drive low to save power and prevent any more interrupts from it
            // (if it stayed pulled up then it would draw power though the pull-up resistor whenever the pin was inserted)

            CBI( TRGIGER_SWITCH_POUT , TRIGGER_SWITCH_B );      // low
            SBI( TRGIGER_SWITCH_PDIR , TRIGGER_SWITCH_B );      // drive

            CBI( TRIGGER_SWITCH_PIFG , TRIGGER_SWITCH_B );      // Clear any pending interrupt from us driving it low (it could have gone high again since we sampled it)


            // Show all 0's on the LCD to instantly let the use know that we saw the pull

            //#pragma UNROLL( LOGICAL_DIGITS_SIZE )

            for( char i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                lcd_show_f( i , digit_segments[0] );

            }


            // Start counting from right..... now

            restart_rv3032_periodic_timer();

            // We will get the next tick in 500ms

            // Flash lights

            flash();

            // TODO: Record timestamp

            mode = TIME_SINCE_LAUNCH;

        } else {

            // note that if the pin was not still low when we sampled it, then it will generate another interrupt next time it
            // goes low and there is no race condition.


            char current_squiggle_step = next_squiggle_step;

            for( char i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                lcd_show_f( i , squiggle_segments[ current_squiggle_step ]);

                if ( current_squiggle_step == 0 ) {
                    current_squiggle_step = SQUIGGLE_SEGMENTS_SIZE-1;
                } else {
                    current_squiggle_step--;
                }

            }


            if ( next_squiggle_step == 0 ) {
                next_squiggle_step = SQUIGGLE_SEGMENTS_SIZE-1;
            } else {
                next_squiggle_step--;
            }

        }

    } else if ( mode==ARMING ) {

        // This phase is just to add a 500ms debounce to when the pin is initially inserted at the factory to make sure we do
        // not accidentally fire then.

        // We are currently showing dashes, and we will switch to squiggles on next tick if the pin is in now

        // If pin is still inserted (switch open, pin high), then go into READY_TO_LAUNCH were we wait for it to be pulled

        if ( TBI( TRGIGER_SWITCH_PIN , TRIGGER_SWITCH_B )  ) {

            // Now we need to setup interrupt on trigger pin to wake us when it goes low
            // This way we can react instantly when they pull the pin.

            SBI( TRIGGER_SWITCH_PIE  , TRIGGER_SWITCH_B );          // Enable interrupt on the INT pin high to low edge. Normally pulled-up when pin is inserted (switch lever is depressed)
            SBI( TRIGGER_SWITCH_PIES , TRIGGER_SWITCH_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)

            // Clear any pending interrupt
            CBI( TRIGGER_SWITCH_PIFG , TRIGGER_SWITCH_B);

            mode = READY_TO_LAUNCH;

        } else {

            // In the unlikely case where the pin was inserted for less than 500ms, then go back to the beginning and show dash scroll and wait for it to be inserted again.
            // This has the net effect of making an safety interlock that the pin has to be inserted for a full 1 second before we will arm.

            mode = START;


        }


    } else { // if mode==START

        // Check if pin was inserted (pin high)

        if ( TBI( TRGIGER_SWITCH_PIN , TRIGGER_SWITCH_B ) ) {       // Check if trigger pin is inserted

            // Show a little full dash screen to indicate that we know you put the pin in
            // TODO: make this constexpr

              for( char i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                  lcd_show_f( i , dash_segments );

              }


              //Start showing the ready to launch squiggles on next tick

              mode = ARMING;

              // note that here we wait a full tick before checking that the pin is out again. This will debounce the switch for 0.5s when the pin is inserted.

        } else {


            // trigger pin still out
            // show a little dash sliding towards the trigger pin

            char current_dash_step = next_dash_step;

            if (next_dash_step==0) {

                next_dash_step = LOGICAL_DIGITS_SIZE-1;

            } else {

                next_dash_step--;

            }


            for( char i=0 ; i<LOGICAL_DIGITS_SIZE; i++ ) {

                if (i==current_dash_step || i==next_dash_step ) {
                    lcd_show_f( i , dash_segments );
                } else {
                    lcd_show_f( i , blank_segments );
                }

            }

        }

    }


    CBI( RV3032_INT_PIFG , RV3032_INT_B );      // Clear any RV3032 INT pending interrupt flag (if we just triggered, then we reset the timer so it will not fire first tick for 500ms anyway)

    CBI( DEBUGA_POUT , DEBUGA_B );

}


#pragma vector = RTC_VECTOR

__interrupt void RTC_ISR(void) {

    lcd_show< 9,1>();

    SBI( DEBUGA_POUT , DEBUGA_B );

    // Currently we never make it here in LPM3.5

    //RTCIV;
    //__no_operation();                                   // For debugger


    //(LCDMEM[pos4])++;//= digit[4];

    /*

    (LCDMEM[pos2])= digit[1];


    PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
    PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
    __bis_SR_register(LPM3_bits | GIE);                 // Re-enter LPM3.5

    (LCDMEM[pos2])= digit[6];

    */

    /*

    Inc_RTC();

    (LCDMEM[pos4]) = digit[4];

    PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
    PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
    __bis_SR_register(LPM3_bits | GIE);                 // Re-enter LPM3.5
*/

    CBI( DEBUGA_POUT , DEBUGA_B );

}

