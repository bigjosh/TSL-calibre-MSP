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

struct digit_segment_t {
    const char nibble_a_thru_d;     // The COM bits for the digit's A-D segments
    const char nibble_e_thru_g;     // The COM bits for the digit's E-G segments
};


constexpr digit_segment_t digit_segments[] = {

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

};

// This represents a logical digit that we will use to actually display numbers on the screen
// Digit 0 is the rightmost and digit 11 is the leftmost
// LPIN is the name of the pin on the MSP430 that the LCD pin for that digit is connected to.
// Because this is a 4-mux LCD, we need 2 LCD pins for each digit because 2 pins *  4 com lines = 8 segments (7 ofr the 7-segment digit and sometimes one for an indicator)
// These mappings come from our PCB trace layout

struct logical_digit_t {
    const char lpin_e_thru_g;       // The LPIN for segments E-G
    const char lpin_a_thru_d;       // The LPIN for segments A-D

};

constexpr logical_digit_t logical_digits[] {
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
static inline void lcd_show_f( const char pos, const char x ) {

    const char nibble_a_thru_d =  digit_segments[x].nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    const char nibble_e_thru_g =  digit_segments[x].nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

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

void wiggleFlash() {
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

    // --- Debug pins

    // DEBUGA as output
    // SBI( DEBUGA_PDIR , DEBUGA_B );

    // --- RV3032

    // Make the pin connected to RV3032 CLKOUT be input. CLKOUT is disabled, so RV3032 should be driving this LOW
    CBI( RV3032_CLKOUT_PDIR,RV3032_CLKOUT_B);

    // ~INT in from RV3032 as INPUT with PULLUP

    CBI( RV3032_INT_PDIR , RV3032_INT_B ); // INPUT
    SBI( RV3032_INT_POUT , RV3032_INT_B ); // Pull-up
    SBI( RV3032_INT_PREN , RV3032_INT_B ); // Pull resistor enable

    // Power to RV3032

    SBI( RV3032_VCC_PDIR , RV3032_VCC_B);
    SBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now we need to setup interrupt on INT pin to wake us when it goes low

    SBI( RV3032_INT_PIES , RV3032_INT_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)
    SBI( RV3032_INT_PIE  , RV3032_INT_B );          // Enable interrupt on the INT pin high to low edge.


    // --- Trigger Pin

    // Trigger pin as in input with pull-up

    CBI( TRGIGER_SWITCH_POUT , TRGIGER_SWITCH_B );      // Set input
    SBI( TRGIGER_SWITCH_PREN , TRGIGER_SWITCH_B );      // Enable pull resistor
    SBI( TRGIGER_SWITCH_POUT , TRGIGER_SWITCH_B );      // Pull up

    // Now we need to setup interrupt on trigger pin to wake us when it goes low

    SBI( TRIGGER_SWITCH_PIES , TRGIGER_SWITCH_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)
    SBI( TRIGGER_SWITCH_PIE  , TRGIGER_SWITCH_B );          // Enable interrupt on the INT pin high to low edge. Normally pulled-up when pin is inserted (switch lever is depressed)

    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;


    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Clear any pending interrupt from the INT pin
    //CBI( RV3032_INT_PIFG , RV3032_CLKOUT_B );

    RV3032_INT_PIV;   // Read and throw away to clear the PORT1 pin change interrupt flag. Only clears the top one but we only expect to have one.

}



int main( void )
{
    WDTCTL = WDTPW | WDTHOLD;                               // Stop watchdog timer
    if ( SYSRSTIV == SYSRSTIV_LPM5WU )                        // If LPMx.5 wakeup
    {


        initGPIO();

        RV3032_INT_PIV;   // Read and throw away to clear the PORT1 pin change interrupt flag. Only clears the top one but we only expect to have one.

/*
        // toggle low power waveform so we can see if it is visible (turns out only visible from wide viewing angles)
        // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON), Low power waveform
        //LCDCTL0 ^= LCDLP ;

        switch (*Seconds) {

            case 0:
                *Seconds=1;
                lcd_show< 0,1>();
                break;

            case 1:
                *Seconds=2;
                lcd_show< 0,2>();
                break;

            case 2:
                *Seconds=3;
                lcd_show< 0,3>();
                break;

            case 3:
                *Seconds=4;
                lcd_show< 0,4>();
                break;

            case 4:
                *Seconds=5;
                lcd_show< 0,5>();
                break;

            case 5:
                *Seconds=6;
                lcd_show< 0,6>();
                break;

            case 6:
                *Seconds=7;
                lcd_show< 0,7>();
                break;

            case 7:
                *Seconds=8;
                lcd_show< 0,8>();
                break;

            case 8:
                *Seconds=9;
                lcd_show< 0,9>();
                break;

            case 9:
                *Seconds=0;
                lcd_show< 0,0>();
                break;


            default:
                *Seconds=0;
                lcd_show< 0,0>();
                break;

        }
*/
        if (*spin==1) {

            lcd_show< 8,1>();
            *spin=2;

        } else {
            lcd_show< 8,0>();
            *spin=1;

        }

        //RV3032_INT_PIV;   // Read and throw away to clear the PORT1 pin change interrupt flag. Only clears the top one but we only expect to have one.

        /*
            Reset interrupt vector. Generates a value that can be used as address offset for
            fast interrupt service routine handling to identify the last cause of a reset (BOR,
            POR, or PUC). Writing to this register clears all pending reset source flags.
        */

        //SYSRSTIV = 0x00;            // Clear all pending reset sources

        SBI( DEBUGA_POUT , DEBUGA_B );

        // Go into LPM4.5 sleep
        PMMCTL0_H = PMMPW_H;                               // Open PMM Registers for write
        PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF also clears SVS
        // TODO: datasheets say SVS off makes wakeup 10x slower, so need to test when wake works

        __bis_SR_register( LPM3_bits | GIE );                    // Enter LPM4.5


        // We never get here because LPM4.5 reboots on wake


    }
    else
    {

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
        LCDPCTL0 = 0b1111111111111111;  // LCD pins L15-L00, 1=enabled
        LCDPCTL1 = 0b1111111111111111;  // LCD pins L31-L16, 1=enabled
        LCDPCTL2 = 0b0000000000001111;  // LCD pins L37-L32, 1=enabled

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
        //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


        // LCD Operation - Mode 3, internal 2.78v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.0uA from 3.5V Vcc
        //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_3 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


        // LCD Operation - All 3 LCD voltages external. When generating all 3 with regulators, we get 2.48uA @ Vcc=3.5V so not worth it.
        //LCDVCTL = 0;


        // LCD Operation - All 3 LCD voltages external. When generating 1 regulators + 3 1M Ohm resistors, we get 2.9uA @ Vcc=3.5V so not worth it.
        //LCDVCTL = 0;


        // LCD Operation - Charge pump enable, Vlcd=Vcc , charge pump FREQ=256Hz (lowest)  2.5uA - Good for testing without a regulator
        LCDVCTL = LCDCPEN |  LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


        /* WINNER for controlled Vlcd - Uses external TSP7A0228 regulator for Vlcd on R33 */
        // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=256Hz (lowest). 2.1uA/180uA  @ Vcc=3.5V . Vlcd=2.8V  from TPS7A0228 no blinking.
        //LCDVCTL = LCDCPEN |  (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);



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


        lcd_show_f( 0,1 );
        lcd_show_f( 1,0 );
        lcd_show_f( 2,9 );
        lcd_show_f( 3,8 );
        lcd_show_f( 4,7 );
        lcd_show_f( 5,6 );
        lcd_show_f( 6,5 );
        lcd_show_f( 7,4 );
        lcd_show_f( 8,3 );
        lcd_show_f( 9,2 );
        lcd_show_f(10,1 );
        lcd_show_f(11,0 );
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
        //RTCCTL = RTCSS__VLOCLK | RTCIE;                    // Initialize RTC to use Very Low Oscillator and enable RTC interrupt on rollovert
        RTCCTL = 0x00;                    // Disable RTC

*/

/*
        lcd_show< 0,6>();
        P1DIR |= 1<<3;

        while (1) {

            P1OUT |= 1<<3;
            __delay_cycles(100);
            P1OUT &= ~(1<<3);
            __delay_cycles(100);

        }
*/

        // Setup the RV3032




        // Give the RV3230 a chance to wake up before we start pounding it.
        // POR refresh time(1) At power up ~66ms
        __delay_cycles(100000);

        // Initialize our i2c pins as pull-up
        i2c_init();


        // Set all the registers

        // First control reg. Note that turning off backup switchover seems to save ~0.1uA
        //uint8_t pmu_reg = 0b01010001;           // CLKOUT off, Direct backup switching mode, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd
        uint8_t pmu_reg = 0b01000001;           // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd

        i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );


        // Now that CLKOUT is disabled on the RV3032, it is supposed to be driven low, but seems to float sometimes
        // so lets pull it low on the MSP430 to prevent float leakage.
        CBI( RV3032_CLKOUT_POUT , RV3032_CLKOUT_B );
        SBI( RV3032_CLKOUT_PREN , RV3032_CLKOUT_B );


        // Periodic Timer value = 2048 for 500ms

        const unsigned timerValue = 2048;
        uint8_t timerValueH = timerValue / 0x100;
        uint8_t timerValueL = timerValue % 0x100;
        i2c_write( RV_3032_I2C_ADDR , 0x0b , &timerValueL  , 1 );
        i2c_write( RV_3032_I2C_ADDR , 0x0c , &timerValueH  , 1 );

        // Set TIE in Control 2 to 1 t enable interrupt pin for periodic countdown
        uint8_t control2_reg = 0b00010000;
        i2c_write( RV_3032_I2C_ADDR , 0x11 , &control2_reg , 1 );

        // set TD to 00 for 4068Hz timer, TE=1 to enable periodic countdown timer, EERD=1 to disable automatic EEPROM refresh (why would you want that?)
        uint8_t control1_reg = 0b00001100;
        i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );


        // OK, we will now get a 122uS low pulse on INT every 500ms.

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
            lcd_show< 1,0>();
        }

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

        PMMCTL0_H = PMMPW_H;                            // Open PMM Registers for write
        PMMCTL0_L &= ~SVSHE ;                           // disable SVS

        __bis_SR_register( CPUOFF | SCG1 | OSCOFF  );                 // Enter LPM3 with datasheet specified flags.

*/


        /*
            Reset interrupt vector. Generates a value that can be used as address offset for
            fast interrupt service routine handling to identify the last cause of a reset (BOR,
            POR, or PUC). Writing to this register clears all pending reset source flags.
        */


        SYSRSTIV = 0x00;            // Clear all pending reset sources


        __bis_SR_register( LPM4_bits | GIE );                    // Enter LPM4 - 2.12uA with static @ Vcc=3.5V, 2.2uA with simple counting

/*
        The LMPx.5 modes are very not worth it for us since they waste so much power on extended wake up time.
        They would only be worth it if you were sleeping >minutes, not 500ms like we are.

*/
/*
        SBI( DEBUGA_POUT , DEBUGA_B );


        // Go into LPMx.5 sleep
        PMMCTL0_H = PMMPW_H;                               // Open PMM Registers for write
        PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF also clears SVS
        // TODO: datasheets say SVS off makes wakeup 10x slower, so need to test when wake works

//        __bis_SR_register( LPM3_bits | GIE );                    // Enter LPM3.5 - 1.87uA with static @ Vcc=3.5V, , 4.8uA with simple counting
//        __bis_SR_register( LPM4_bits | GIE );                    // Enter LPM4.5 - 1.87uA with static @ Vcc=3.5V, 4.5uA with simple counting

*/
        /*
        PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
        PMMCTL0_L = PMMREGOFF_L | SVSHE;                   // and set PMMREGOFF (go into a x.5 mode on next sleep) and disable low voltage monitor

        __bis_SR_register(LPM3_bits | GIE);                 // Enter LPM3.5
        __no_operation();                                   // For debugger
         */

        while (1) {

            lcd_show< 9,2>();
            __delay_cycles(1000000);
            lcd_show< 9,3>();
            __delay_cycles(1000000);

        }
    }
}


#pragma vector = PORT1_VECTOR

__interrupt void PORT1_ISR(void) {

    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    if ( TBI( TRIGGER_SWITCH_PIFG , TRGIGER_SWITCH_B ) ) {

        // Interrupt from the trigger pin high to low

        // Show the flash
        //wiggleFlashQ1();

        // Note that the flash takes a fraction of a second, so will debounce the trigger pin
        // and the following line will clear all the edges that happened.

        if (*triggerSpin<9) {
            (*triggerSpin)++;
        } else {
            (*triggerSpin)=0;
        }

        lcd_show_f(7, *triggerSpin);

        CBI( TRIGGER_SWITCH_PIFG , TRGIGER_SWITCH_B );      // Clear pending interrupt flag

    }


    if ( TBI( RV3032_INT_PIFG , RV3032_INT_B ) ) {


        if (*spin<9) {
            (*spin)++;
        } else {
            (*spin)=0;
        }

        lcd_show_f(8, *spin);

        CBI( RV3032_INT_PIFG , RV3032_INT_B );      // Clear pending interrupt flag

    }

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

