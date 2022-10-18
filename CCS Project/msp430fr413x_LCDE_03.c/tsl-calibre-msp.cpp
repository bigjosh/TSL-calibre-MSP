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
    const char lpin_a_thru_d;       // The LPIN for segments A-D
    const char lpin_e_thru_g;       // The LPIN for segments E-G
};

constexpr logical_digit_t logical_digits[] {
    { 35 , 34 },        //  0 - corresponds to LCD digit 12 and is the rightmost digit
    { 33 , 32 },        //  1 (LCD 11)
    { 31 , 30 },        //  2 (LCD 10)
    { 29 , 28 },        //  3 {LCD 09)
    { 27 , 26 },        //  4 {LCD 08)
    { 25 , 24 },        //  5 {LCD 07)
    { 21 , 17 },        //  6 (LCD 06)
    { 16 , 15 },        //  7 (LCD 05)
    { 14 , 13 },        //  8 (LCD 04)
    { 12 ,  1 },        //  9 (LCD 03)
    {  2 ,  3 },        // 10 (LCD 02)
    {  4 ,  4 },        // 11 (LCD 01)
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

// Write the specified nibble. The other nibble at address a is unchanged.

void set_nibble( char * a , nibble_t nibble_index , char x ) {

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

    char * lcd_mem_reg = LCDMEM;

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


volatile unsigned char * Seconds = &BAKMEM0_L;               // Store seconds in the backup RAM module
volatile unsigned char * Minutes = &BAKMEM0_H;               // Store minutes in the backup RAM module
volatile unsigned char * Hours = &BAKMEM1_L;                 // Store hours in the backup RAM module
volatile unsigned char * spin = &BAKMEM1_H;                 // Store hours in the backup RAM module



int main( void )
{
    WDTCTL = WDTPW | WDTHOLD;                               // Stop watchdog timer
    if ( 0 /* && SYSRSTIV == SYSRSTIV_LPM5WU */)                        // If LPM3.5 wakeup
    {

        SBI( P1DIR , 1 );
        SBI( P1OUT , 1 );
        CBI( P1OUT , 1 );

        /*
         * When an overflow occurs, the RTCIFG bit in the RTCCTL register is set until it is cleared by a read of the
         * RTCIV register. At the same time, an interrupt is submitted to the CPU for post-processing, if the RTCIE
         * bit in the RTCCTL register is set. Reading RTCIV register clears the interrupt flag.
         */

        RTCIV; // Clear RTC interrupt. If we do not do this then the RTC ISR will be needlessly called when drop into LPM3.5


        // Initialize GPIO pins for low power
        // Configure all GPIO to Output Low
        P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
        P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

       CLOCK_IN_PDIR &= ~_BV( CLOCK_IN_B );
       // CLOCK_IN_POUT &= ~_BV( CLOCK_IN_B );
       CLOCK_IN_PREN |= _BV( CLOCK_IN_B );


       // Configure LCD pins
       SYSCFG2 |= LCDPCTL;


        // Disable the GPIO power-on default high-impedance mode
        // to activate previously configured port settings
        PM5CTL0 &= ~LOCKLPM5;


        //CLOCK_IN_PIV;   // Read and throw away to clear the PORT1 pin change interrupt flag. Only clears the top one but we only expect to have one.
        P1IFG = 0x00;

        CLOCK_IN_PIE |= _BV( CLOCK_IN_B );          // Enable interrupt on the clock_in pin. By default this will be low-to-high triggered


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


        if (TBI(CLOCK_IN_PIN,CLOCK_IN_B)) {
            lcd_show<11,1>();
        } else {
            lcd_show<11,0>();

        }

        PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
        PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
        __bis_SR_register(LPM3_bits | GIE);                 // Re-enter LPM3.5

        // We never get here because LPM3.5 reboots on wake


    }
    else
    {

        //TODO: INit our own stack save the wastefull init
        //STACK_INIT();

        // Initialize GPIO pins for low power
        P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
        P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
        // Configure all GPIO to Output Low
        P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
        P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

        // Disable the GPIO power-on default high-impedance mode
        // to activate previously configured port settings
        PM5CTL0 &= ~LOCKLPM5;

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


        // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz, 3.4uA
        //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

        // LCD Operation - internal V1 regulator=3.32v , charge pump 256Hz
        // LCDVCTL = LCDCPEN | LCDREFEN | VLCD_12 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


        // LCD Operation - Pin R33 is connected to external Vcc, charge pump 256Hz, 1.7uA
        //LCDVCTL = LCDCPEN | LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


        // LCD Operation - Pin R33 is connected to internal Vcc, no charge pump
        //LCDVCTL = LCDSELVDD;


        // LCD Operation - Pin R33 is connected to external V1, charge pump 256Hz, 1.7uA, Low Power Waveform
        //LCDVCTL = LCDCPEN | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

        // LCD Operation - Charge pump enable, Vlcd=Vcc , charge pump FREQ=256Hz (lowest)
        LCDVCTL = LCDCPEN |  LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

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

/*
        // Set up RTC



//        RTCMOD = 10000;                                     // Set RTC modulo to 10000 to trigger interrupt each second on 10Khz VLO clock. This will get loaded into the shadow register on next trigger or reset
        RTCMOD = 10000;                                     // Set RTC modulo to 1000 to trigger interrupt each 0.1 second on 10Khz VLO clock. This will get loaded into the shadow register on next trigger or reset

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

        i2c_init();

        uint8_t pmu_reg = 0b00000000;
        i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 ); // Default reg, Enable CLKOUT

        uint8_t save_flags_reg = 0b01100000;
        i2c_write( RV_3032_I2C_ADDR , 0xc3 , &save_flags_reg , 1 ); // Switch CLKOUT to 1 Hz

        // set TD to 00 for 4069Hz timer, TE=1 to enable periodic countdown timer
        uint8_t control1 = 0b00001000;
        i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1 , 1 ); // Switch CLKOUT to 1 Hz

        // Set TIE in Control 2 to 1 t enable interrupt pin for periodic countdown
        uint8_t control2 = 0b00010000;
        i2c_write( RV_3032_I2C_ADDR , 0x11 , &control2 , 1 ); // Switch CLKOUT to 1 Hz


        // Periodic Timer value = 2048 for 500ms

        const unsigned timerValue = 4069;
        uint8_t timerValueH = timerValue / 0x100;
        uint8_t timerValueL = timerValue % 0x100;
        i2c_write( RV_3032_I2C_ADDR , 0x0b , &timerValueL  , 1 );
        i2c_write( RV_3032_I2C_ADDR , 0x0c , &timerValueH  , 1 );

        // Set TD= 01 for 4096Hz




        //uint8_t pmu_reg = 0b01010000;       // No CLKOUT, Switchover when VDD < VBACKUP, Trickle Charger off
        //i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 ); // Disable CLKOUT



        // OK now the RTC is sending a 1Hz clock out
        // Now we need to setup interrupt on clock_in pin to wake us




       CLOCK_IN_PDIR &= ~_BV( CLOCK_IN_B );
       CLOCK_IN_POUT |= _BV( CLOCK_IN_B );// Pull-up
       CLOCK_IN_PREN |= _BV( CLOCK_IN_B );


       P1IFG = 0x00;

       CLOCK_IN_PIE |= _BV( CLOCK_IN_B );          // Enable interrupt on the clock_in pin. By default this will be low-to-high triggered



        // TODO: Why doesnt this work?
        //LCDMEMCTL |= LCDCLRM;                               // Clear LCD memory command (executed one shot)
        //while (LCDMEMCTL & LCDCLRM);                        // Wait for clear to complete before moving on

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
        lcd_show<11,5>();

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


        // Go into LPM3.5 sleep
        //PMMCTL0_H = PMMPW_H;                               // Open PMM Registers for write
        //PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF also clears SVS
        // TODO: datasheets say SVS off makes wakeup 10x slower, so need to test when wake works

        __bis_SR_register( LPM4_bits | GIE );                    // Enter LPM3.5

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

    CLOCK_IN_PIV;   // Read and throw away to clear the PORT1 pin change interrupt flag. Only clears the top one but we only expect to have one.


    if (*spin==1) {

        lcd_show< 8,1>();
        *spin=2;

    } else {
        lcd_show< 8,2>();
        *spin=1;

    }

    SBI( P1DIR , 2 );
    SBI( P1OUT , 2 );
    CBI( P1OUT , 2 );

}


#pragma vector = RTC_VECTOR

__interrupt void RTC_ISR(void) {

    lcd_show< 9,1>();

    SBI( P1DIR , 2 );
    SBI( P1OUT , 2 );
    CBI( P1OUT , 2 );



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
}

