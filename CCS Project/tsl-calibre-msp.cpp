#include <msp430.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#include "error_codes.h"
#include "lcd_display.h"

#include "tsl_asm.h"

#define RV_3032_I2C_ADDR (0b01010001)           // Datasheet 6.6

// RV3032 Registers

#define RV3032_HUNDS_REG 0x00    // Hundredths of seconds
#define RV3032_SECS_REG  0x01
#define RV3032_MINS_REG  0x02
#define RV3032_HOURS_REG 0x03
#define RV3032_DAYS_REG  0x05
#define RV3032_MONS_REG  0x06
#define RV3032_YEARS_REG 0x07



// Put the LCD into blinking mode

void lcd_blinking_mode() {
    LCDBLKCTL = LCDBLKPRE__64 | LCDBLKMOD_2;       // Clock prescaler for blink rate, "10b = Blinking of all segments"
}


// TODO: Make this LPM4.5
// TODO: Clearing GIE does not prevent wakeup form LPM4.5
// In LPMx.0 draws 1.38uA with the "First STart" message.
// In LPMx.5 draws 1.13uA with the "First STart" message.

#pragma FUNC_NEVER_RETURNS
void sleepforeverandever(){

    // Mind as well go into deep LPMx.5 sleep since we will never wake up, but in these modes there is no general way to disable
    // Interrupts and any interrupt powers up the CPU. We could carefully disable any possible interrupt sources here, but too much
    // work for saving 0.2uA considering that if we get here then we really don't care about battery life anyway.

    __bis_SR_register(LPM4_bits);                 // Enter LPM4 and sleep forever and ever and ever (no interrupts enabled so we will never get woken up)
}

// TODO: Make this LPM4.5
#pragma FUNC_NEVER_RETURNS
void blinkforeverandever(){
    lcd_blinking_mode();
    sleepforeverandever();
}

// TODO: Make this LPM4.5
#pragma FUNC_NEVER_RETURNS
void error_mode( byte code ) {
    lcd_show_errorcode(code);
    blinkforeverandever();
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


    // --- TSP7A LCD voltage regulator

    SBI( TSP_IN_POUT , TSP_IN_B );          // Power up
    SBI( TSP_ENABLE_POUT , TSP_ENABLE_B );  // Enable

    // --- Debug pins

    // Debug pins
    SBI( DEBUGA_PDIR , DEBUGA_B );          // Currently used to time how long the ISR takes

    CBI( DEBUGB_PDIR , DEBUGB_B );          // Input
    SBI( DEBUGB_POUT , DEBUGB_B );          // Pull up
    SBI( DEBUGB_PREN , DEBUGB_B );          // Currently checked at power up, if low then we go into testonly mode.

    // --- RV3032

    // ~INT in from RV3032 as INPUT with PULLUP
    // We don't use this for now, so set to drive low.

    SBI( RV3032_INT_PDIR , RV3032_INT_B ); // INPUT

    // Note that we can leave the RV3032 EVI pin as is - tied LOW on the PCB so it does not float (we do not use it)
    // It is hard tied low so that it does not float during battery changes when the MCU will not be running

    // Power to RV3032

    CBI( RV3032_GND_POUT , RV3032_GND_B);
    SBI( RV3032_GND_PDIR , RV3032_GND_B);

    SBI( RV3032_VCC_PDIR , RV3032_VCC_B);
    SBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now we need to setup interrupt on RTC CLKOUT pin to wake us on each rising edge (1Hz)

    CBI( RV3032_CLKOUT_PDIR , RV3032_CLKOUT_B );      // Set input
    SBI( RV3032_CLKOUT_PIE  , RV3032_CLKOUT_B );      // Interrupt on high-to-low edge (Driven by RV3032 CLKOUT pin which we program to run at 1Hz). Falling edge so we wait a full second until first tick (signal starts LOW).
    CBI( RV3032_CLKOUT_PIES , RV3032_CLKOUT_B );      // Enable interrupt on the CLKOUT pin. Calls rtc_isr()

    // --- Trigger

    // By default we set the trigger to drive low so it will not use power regardless if the pin is in or out.
    // We will switch it to pull-up later if we need to (because we have not launched yet).

    CBI( TRIGGER_POUT , TRIGGER_B );      // low
    SBI( TRIGGER_PDIR , TRIGGER_B );      // drive

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

// Turn off power to RV3032
void depower_rv3032() {

    // Make Vcc pin to RV3032 ground.
    CBI( RV3032_VCC_POUT , RV3032_VCC_B);

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

    // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON), Low power waveform
    //LCDCTL0 = LCDDIV_1 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON | LCDLP ;

    // LCD using VLO clock, divide by 4 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. No flicker. Squiggle=1.45uA. Count=2.00uA. I guess not worth the flicker for 0.2uA?
    LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__4 | LCD4MUX | LCDLP ;

    // LCD using VLO clock, divide by 5 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. Visible flicker at large view angles. Squiggle=1.35uA. Count=1.83uA
    //LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__5 | LCD4MUX | LCDLP ;

    // LCD using VLO clock, divide by 6 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. VISIBLE FLICKER at 3.5V
    //LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__6 | LCD4MUX | LCDLP ;


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


    // LCD Operation - Charge pump enable, Vlcd=Vcc , charge pump FREQ=/256Hz (lowest)  2.5uA - Good for testing without a regulator
    //LCDVCTL = LCDCPEN |  LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

    /* WINNER for controlled Vlcd - Uses external TSP7A0228 regulator for Vlcd on R33 */
    // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=/256Hz (lowest). 2.1uA/180uA  @ Vcc=3.5V . Vlcd=2.8V  from TPS7A0228 no blinking.
    LCDVCTL = LCDCPEN |   (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=/64Hz . 2.1uA/180uA  @ Vcc=3.5V . Vlcd=2.8V  from TPS7A0228 no blinking.
    //LCDVCTL = LCDCPEN |   (LCDCPFSEL0 | LCDCPFSEL1 );


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

    LCDBLKCTL = 0x00;       // Disable blinking. We do this because this bit is not cleared on reset so if we are resetting out of at blinking mode then it would otherwise persist.

    LCDCTL0 |= LCDON;                                           // Turn on LCD

}

// The of time registers we read from the RTC in a single block using one i2c transaction.

struct __attribute__((__packed__)) rv3032_time_block_t {
    byte sec_bcd;
    byte min_bcd;
    byte hour_bcd;
    byte weekday_bcd;
    byte date_bcd;
    byte month_bcd;
    byte year_bcd;
};


// Read the time regs in one transaction

void readRV3032time( rv3032_time_block_t *b ) {

    i2c_init();

    i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , b , sizeof( rv3032_time_block_t ) );

    i2c_shutdown();

}


// Write the time regs in one transacton. Clears the hundreths to zero.

void writeRV3032time( const rv3032_time_block_t *b ) {

    i2c_init();

    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , b , sizeof( rv3032_time_block_t ) );

    i2c_shutdown();

}


rv3032_time_block_t rv_3032_time_block_init = {
  0,        // Sec
  0,        // Min
  0,        // Hour
  1,        // Weekday
  1,        // Date
  1,        // Month
  0         // Year
};




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

// Here is the persistent data that we store in "information memory" FRAM that survives power cycles

struct __attribute__((__packed__)) persistent_data_t {

    rv3032_time_block_t commisioned;            // Calendar time when this unit was commissioned.
    rv3032_time_block_t launched;               // Time when this unit was launched relative to commissioned.
    byte once_flag;                             // Set to 1 on initial programming.
    byte launch_flag;                           // Set to 0 on initial programming.
    unsigned days_since_last_century;
    unsigned century_count;
};

// Tell compiler/linker to put this in "info memory" at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.
// We have to mark `volatile` so that the compiler will really go to the FRAM every time rather than optimizing out some accesses.
volatile persistent_data_t __attribute__(( __section__(".infoA") )) persistent_data;


void unlock_persistant_data() {
    SYSCFG0 = PFWP;                     // Write protect only program FRAM. Interestingly it appears that the password is not needed here?
}

void lock_persistant_data() {
    SYSCFG0 = PFWP | DFWP;              // Write protect both program and data FRAM.
}

// Low voltage flag indicates that the RTC has been re-powered and potentially lost its data.

// Initialize RV3032 for the first time
// Clears the low voltage flag
// sets clkout to 1Hz
// Enables backup capacitor

void rv3032_init() {

    // Give the RV3230 a chance to wake up before we start pounding it.
    // POR refresh time(1) At power up ~66ms
    // Also there is Tdeb which is the time it takes to recover from a backup switch over back to Vcc. It is unclear if this is 1ms or 1000ms so lets be safe.
    __delay_cycles(1100000);    // 1 sec +/-10% (we are running at 1Mhz)

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Set all the registers we care about that can get reset by either power-on-reset or recover from backup
    uint8_t clkout2_reg = 0b01100000;        // CLKOUT XTAL low freq mode, freq=1Hz
    i2c_write( RV_3032_I2C_ADDR , 0xc3 , &clkout2_reg , 1 );

    // First control reg. Note that turning off backup switch-over seems to save ~0.1uA
    //uint8_t pmu_reg = 0b01000001;         // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd.
    //uint8_t pmu_reg = 0b01010000;          // CLKOUT off, Direct backup switching mode, no charge pump, 0.6K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.
    //uint8_t pmu_reg = 0b01100001;         // CLKOUT off, Level backup switching mode (2v) , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd. Predicted to use ~200nA more than disabled because of voltage monitor.
    //uint8_t pmu_reg = 0b01000000;         // CLKOUT off, Other disabled backup switching mode, no charge pump, trickle resistor off, trickle charge Vbackup to Vdd

    uint8_t pmu_reg = 0b00011101;          // CLKOUT ON, Direct backup switching mode, no charge pump, 12K OHM trickle resistor, trickle charge Vbackup to Vdd.
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    uint8_t control1_reg = 0b00000100;      // TE=0 so no periodic timer interrupt, EERD=1 to disable automatic EEPROM refresh (why would you want that?).
    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    i2c_shutdown();

}

// Clear the low voltage flags. These flags remember if the chip has seen a voltage low enough to make it loose time.

void rv3032_clear_LV_flags() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    uint8_t status_reg=0x00;        // Set all flags to 0. Clears out the low voltage flag so if it is later set then we know that the chip lost power.
    i2c_write( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    i2c_shutdown();

}


// Turn off the RTC to save some power. Only use this if you will NEVER need the RTC again (like if you are going into error mode).

void rv3032_shutdown() {

    i2c_init();

    uint8_t pmu_reg = 0b01000000;         // CLKOUT off, backup switchover disabled, no charge pump
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    i2c_shutdown();

}


// Returns  0=above time variables have been set to those held in the RTC
//          1=RTC has lost power so valid time not available.
//          2=Bad data from RTC

uint8_t RV3032_read_state() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // First check if the RV3032 just powered up, or if it was already running. Start with 0xff so if the i2c read fails then we assume not running.
    uint8_t status_reg=0xff;
    i2c_read( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    i2c_shutdown();

    if ( (status_reg ) == 0xff ) {               //

            // Signal to caller that we did not get an expected value from RTC.
            return 2;

    }


    if ( (status_reg & 0x03) != 0x00 ) {               //If power on reset or low voltage drop detected then we have not been running continuously

        // Signal to caller that we have had a low power event.
        return 1;

    }

    return 0;

}

// Time from RTC - these should always match the time in the RTC registers (but we do decoding from d/m/y to elapsed days)
// During ready-to-launch mode, this is the time since initial power up at the factory
// During time-since-launch mode, this is how long we have been counting.

uint8_t secs=0;
uint8_t mins=0;
uint8_t hours=0;
uint32_t days=0;       // needs to be able to hold up to 1,000,000. I wish we had a 24 bit type here. MSPX has 20 bit addresses, but not available on our chip.


// Read time from RTC into our global time variables

void RV_3032_read_time() {

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Read time

    rv3032_time_block_t t;

    readRV3032time(&t);

    secs  = bcd2c(t.sec_bcd);
    mins  = bcd2c(t.min_bcd);
    hours = bcd2c(t.hour_bcd);

    uint8_t c;
    uint8_t y;
    uint8_t m;
    uint8_t d;

    d = bcd2c(t.date_bcd);
    m = bcd2c(t.month_bcd);
    y = bcd2c(t.year_bcd);

    c=0; // TODO: How to get this from RV_3032?

    days = date_to_days(c, y, m, d);

    i2c_shutdown();

}

/*

// Test to see if the RV3032 considers (21)00 a leap year
// Note: Confirms that year "00" is a leap year with 29 days.

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


    }



}


*/



enum mode_t {
    LOAD_TRIGGER,                  // Waiting for pin to be inserted (shows dashes)
    ARMING,                 // Hold-off after pin is inserted to make sure we don't inadvertently trigger on a switch bounce (lasts 0.5s)
    READY_TO_LAUNCH,        // Pin inserted, waiting for it to be removed (shows squiggles)
    TIME_SINCE_LAUNCH       // Pin pulled, currently counting (shows count)
};

mode_t mode;

unsigned int step;          // LOAD_TRIGGER      - Used to keep the position of the dash moving across the display (0=leftmost position, only one dash showing)
                            // READY_TO_LAUNCH   - Which step of the squigle animation


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

    lcd_show_fast_secs(secs);

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    CBI( DEBUGA_POUT , DEBUGA_B );
}

// Copies interrupt vector table to RAM and then sets the RTC vector to point to the Time Since Launch mode ISR

void beginTimeSinceLaunchMode() {

}

// Terminate after one day
bool testing_only_mode = false;

// Registers R4-R10 are preserved across function calls, so we can even make calls out of the ISRs and stuff will
// not get messed up.

/*


enter_tslmode_asm

            ; We get secs in R12 for free by the C passing conventions

            mov         &secs_lcdmem_word,R_S_LCD_MEM       ;; Address in LCD memory to write seconds digits

            mov         #secs_lcd_words,R_S_TBL
            mov         R_S_TBL , R_S_PTR
            mov         #SECS_PER_MIN,R_S_CNT
*/

/*

void enter_tsl_mode() {
    asm("R_S_LCD_MEM    .set      R4                                  ;; word address in LCDMEM that holds the segments for the 2 seconds digits");
    asm("R_S_TBL        .set      R6                                  ;; base address of the table of data words to write to the seconds word in LCD memory");
    asm("R_S_PTR        .set      R7                                  ;; ptr into next word to use in the table of data words to write to the seconds word in LCD memory");
    asm("R_S_CNT        .set      R8                                  ;; Seconds counter, starts at 60 down to 0.");
    asm("");
    asm("               mov.w     &secs_lcdmem_word, R_S_LCD_MEM     ");
    asm("               mov.w     #secs_lcd_words,R_S_TBL            ");
    asm("               mov.w     R_S_TBL , R_S_PTR                  ");
    //asm("               mov.w     #SECS_PER_MIN,R_S_CNT              ");

    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger

}
*/
// Called on RV3032 CLKOUT pin rising edge (1Hz)

// Note that we do not make this an "interrupt" function even though it is an ISR.
// We can do this because we know that there is nothing being interrupted since we just
// sleep between interrupts, so no need to save any registers. We do need to do our own
// RETI (return from interrupt) at the end since the function itself will only have a
// normal return.

//#pragma vector = RV3032_CLKOUT_VECTOR
__attribute__((ramfunc))
__attribute__((retain))
void tsl_isr(void) {

    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    asm("               mov.w       &(secs_lcd_words+6),&(LCDM0W_L+16)");
//    asm("               dec         R_S_CNT");
    asm("               JNE         ISR_DONE1");
//    asm("               mov         R_S_TBL,R_S_PTR");
//    asm("               mov         #SECS_PER_MIN,R_S_CNT");
    asm("ISR_DONE1:");
    asm("               BIC.B     #2,&PAIFG_L+0         ; Clear the interrupt flag that got us here");
    asm("    mov.w  #ISR_DONE1,r8");
    //asm(" mov.w #%dh,r7" , 0x1234);

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    CBI( DEBUGA_POUT , DEBUGA_B );
    asm("          RETI");                            // We did not mark this as an "interrupt" function, so we are responsible for the return. Note that this will just put us back in LPM sleep.

}

#warning
#pragma vector = RV3032_CLKOUT_VECTOR
__attribute__((ramfunc))
__attribute__((retain))
__attribute__((naked))
void rtc_isr(void) {

    // Wake time measured at 48us
    // TODO: Make sure there are no avoidable push/pops happening at ISR entry (seems too long)

    SBI( DEBUGA_POUT , DEBUGA_B );      // See latency to start ISR and how long it runs

    if (  __builtin_expect( mode == TIME_SINCE_LAUNCH , 1 ) ) {            // This is where we will spend most of our life, so optimize for this case

        // 502us
        // 170us

        // Show current time

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

                        for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

                            // The long now

                            lcd_show_long_now();
                            blinkforeverandever();

                        }



                    } else {

                        if (testing_only_mode) {
                            lcd_show_testing_only_message();
                            blinkforeverandever();
                        }

                        days++;

                        // TODO: Optimize
                        lcd_show_digit_f(  6 , (days / 1      ) % 10  );
                        lcd_show_digit_f(  7 , (days / 10     ) % 10  );
                        lcd_show_digit_f(  8 , (days / 100    ) % 10  );
                        lcd_show_digit_f(  9 , (days / 1000   ) % 10  );
                        lcd_show_digit_f( 10 , (days / 10000  ) % 10  );
                        lcd_show_digit_f( 11 , (days / 100000 ) % 10  );

                    }


                }

                lcd_show_digit_f( 4 , hours % 10  );
                lcd_show_digit_f( 5 , hours / 10  );

            }

            lcd_show_digit_f( 2 ,  mins % 10  );
            lcd_show_digit_f( 3 ,  mins / 10  );

        }

        // Show current seconds on LCD using fast lookup table. This is a 16 bit MCU and we were careful to put the 4 nibbles that control the segments of the two seconds digits all in the same memory word,
        // so we can set all the segments of the seconds digits with a single word write.

        // Wow, this compiler is not good. Below we can remove a whole instruction with 3 cycles that is completely unnecessary.

        // *secs_lcdmem_word = secs_lcd_words[secs];

        asm("        MOV.B     &secs+0,r15           ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        RLAM.W    #1,r15                ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        MOV.W     secs_lcd_words+0(r15),(LCDM0W_L+16) ; [] |../tsl-calibre-msp.cpp:1390|");


    } else if ( mode==READY_TO_LAUNCH  ) {       // We will be in this mode potentially for a very long time, so also be power efficient here.

        // We depend on the trigger ISR to move us from ready-to-launch to time-since-launch

        // Do the increment and normalize first so we know it will always be in range
        step++;
        step &=0x07;

        lcd_show_squiggle_frame(step );

        static_assert( SQUIGGLE_ANIMATION_FRAME_COUNT == 8 , "SQUIGGLE_ANIMATION_FRAME_COUNT should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07


    } else if ( mode==ARMING ) {

        // This phase is just to add a 1000ms debounce to when the trigger is initially inserted at the factory to make sure we do
        // not accidentally fire then.

        // We are currently showing "Arming" message, and we will switch to squiggles on next tick if the trigger is still in

        // If trigger is still inserted 1 second later  (since we enetered arming mode) , then go into READY_TO_LAUNCH were we wait for it to be pulled

        if ( TBI( TRIGGER_PIN , TRIGGER_B )  ) {        // Trigger still pin inserted? (switch open, pin high)

            // Now we need to setup interrupt on trigger pull to wake us when pin  goes low
            // This way we can react instantly when they pull the pin.

            SBI( TRIGGER_PIE  , TRIGGER_B );          // Enable interrupt on the INT pin high to low edge. Normally pulled-up when pin is inserted (switch lever is depressed)
            SBI( TRIGGER_PIES , TRIGGER_B );          // Interrupt on high-to-low edge (the pin is pulled up by MSP430 and then RV3032 driven low with open collector by RV3032)

            // Clear any pending interrupt so we will need a new transition to trigger
            CBI( TRIGGER_PIFG , TRIGGER_B);

            mode = READY_TO_LAUNCH;

        } else {

            // In the unlikely case where the pin was inserted for less than 500ms, then go back to the beginning and show dash scroll and wait for it to be inserted again.
            // This has the net effect of making an safety interlock that the pin has to be inserted for a full 1 second before we will arm.

            mode = LOAD_TRIGGER;
            step = 0;

        }


    } else { // if mode==LOAD_TRIGGER

        // Check if trigger is inserted (pin high)

        if ( TBI( TRIGGER_PIN , TRIGGER_B ) ) {       // Check if trigger has been inserted yet

            // Trigger is in, we can arm now.

            lcd_show_arming_message();

            //Start showing the ready to launch squiggles on next tick after that

            mode = ARMING;

            // note that here we wait a full tick before checking that the pin is out again. This will debounce the switch for 1s when the pin is inserted.

        } else {


            SBI( DEBUGB_POUT , DEBUGB_B );

            // trigger still out
            // Show message so person knows why we are waiting (yes, this is redundant, but heck the pin is out so we are burning fuel against the trigger pull-up anyway
            // We do not display this message in the case where the pin is already in at startup since it would be confusing then.
            lcd_show_load_pin_message();

            step++;

            if (step==LOAD_PIN_ANIMATION_FRAME_COUNT) {
                step=0;
            }

            lcd_show_load_pin_animation(step);

            CBI( DEBUGB_POUT , DEBUGB_B );

        }
    }


    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    CBI( DEBUGA_POUT , DEBUGA_B );

    asm("     reti");            // Since this function is not flagged as an `interrupt`, we have to do the `reti` ourselves.

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
    // This will filter glitches since the pin will not still be low when we sample it after this delay. We want to be really sure!
    __delay_cycles( 1000 );


    // Then clear any interrupt flag that got us here.
    // Timing here is critical, we must clear the flag *before* we capture the pin state or we might miss a change
    CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending trigger pin interrupt flag


    // Grab the current trigger pin level
    unsigned triggerpin_level = TBI( TRIGGER_PIN , TRIGGER_B );

    // Now we can check to see if the trigger pin was still low
    // If it is not still low then we will return and this ISR will get called again if it goes low again or
    // if it went low between when we cleared the flag and when we sampled it (VERY small race window of only 1/8th of a microsecond, but hey we need to be perfect, it could be a once in a lifetime moment and we dont want to miss it!)

    if ( !triggerpin_level ) {      // trigger pulled?

        // WE HAVE LIFTOFF!

        // We need to get everything below done within 0.5 seconds so we do not miss the first tick (not hard)

        // Disable the trigger pin and drive low to save power and prevent any more interrupts from it
        // (if it stayed pulled up then it would draw power though the pull-up resistor whenever the pin was out)

        CBI( TRIGGER_POUT , TRIGGER_B );      // low
        SBI( TRIGGER_PDIR , TRIGGER_B );      // drive

        CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending interrupt (it could have gone high again since we sampled it). We already triggered so we don't care anymore.


        // Show all 0's on the LCD to quickly let the user know that we saw the pull
        lcd_show_zeros();

        // Do the actual launch, which will...
        // 1. Read the current RTC time and save it to FRAM
        // 2. Set the launchflag in FRAM so we will forevermore know that we did launch already.
        // 3. Reset the current RTC time to midnight 1/1/00.

        // Initialize our i2c pins as pull-up
        i2c_init();

        // First get current time and save it to FRAM for archival storage.
        unlock_persistant_data();
        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , (void *)  &persistent_data.launched , sizeof( rv3032_time_block_t ) );
        persistent_data.launch_flag=1;
        lock_persistant_data();

        // Then zero out the RTC to start counting over again, starting now. Note that writing any value to the seconds register resets the sub-second counters to the beginning of the second.
        i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &rv_3032_time_block_init , sizeof( rv3032_time_block_t ) );

        i2c_shutdown();

        // We will get the next tick in 1000ms. Make sure we return from this ISR before then.

        // Clear any pending RTC interrupt so we will not tick until the next pulse comes from RTC in 1000ms
        // There could be a race where an RTC interrupt comes in just after the trigger was pulled, in which case we would
        // have a pending interrupt that would get serviced immediately after we return from here, which would look ugly.
        // We could also see an interrupt if the CLKOUT signal was low when trigger pulled (50% likelihood) since it will go high on reset.

        CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear any pending interrupt from CLKOUT

        // Flash lights

        flash();

        // Start ticking from... now!

        secs=0;
        mins=0;
        hours=0;
        days=0;

        mode = TIME_SINCE_LAUNCH;

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

/*
__attribute__((ramfunc))
__interrupt void tsl_isr(void) {

}
*/

int main( void )
{

    WDTCTL = WDTPW | WDTHOLD | WDTSSEL__VLO;   // Give WD password, Stop watchdog timer, set WD source to VLO
                                               // The thinking is that maybe the watchdog will request the SMCLK even though it is stopped (this is implied by the datasheet flowchart)
                                               // Since we have to have VLO on for LCD anyway, mind as well point the WDT to it.
                                               // TODO: Test to see if it matters, although no reason to change it.

    // Disable the Voltage Supervisor (SVS=OFF) to save power since we don't care if the MSP430 goes low voltage
    // This code from LPM_4_5_2.c from TI resources
    PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
    PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS


    // Disable the FRAM controller automatically turning on when we wake.
    // "GCCTL0.FRLPMPWR 0b = FRAM startup is delayed to the first FRAM access after LPM exit"
    // Since we are moving the ISR vector table to RAM and our ISRs are in RAM, this will prevent the
    // FRAM controller from ever starting up, which saves power.

#warning
    //CBI( GCCTL0 , FRLPMPWR );

    initGPIO();

    initLCD();

    // Power up display with a nice dash pattern
    lcd_show_dashes();

    rv3032_init();        // Initialize the RV3032 with proper clkout & backup settings. Note that we must do this every time we power up since these settings are lost when we drop to backup mode when the battery is pulled.


    initLCDPrecomputedWordArrays();

#warning
    if ( TBI( DEBUGB_PIN , DEBUGB_B ) == 0 ) {       // If debug B pin is tied low at power up...

        unlock_persistant_data();
        persistent_data.once_flag=1;            // Trigger a first time power up
        lock_persistant_data();

        lcd_show_testing_only_message();
        blinkforeverandever();

    }


    // Check if this is the first time we've ever been powered up.
    if (persistent_data.once_flag==1) {

        // First clear the low voltage flag on the RV3032 so from now on we will care if it looses time.
        // We check if these flags have been set on each power up to know if the RTC still knows what time it is, or if the battery was out for too long.

        rv3032_clear_LV_flags();

        // Now remember that we did our start up. From now on, the RTC will run on its own forever.

        unlock_persistant_data();
        persistent_data.once_flag=0;             // Remember that we already started up once and never do it again.
        persistent_data.launch_flag=0;           // Clear the launch flag even though we should never have to
        lock_persistant_data();

        lcd_show_first_start_message();
        sleepforeverandever();

        // After first start up, unit must be re-powered to enter normal operation.

    }

    // Read the state of the RV3032. Returns an error if the chip has lost power since it was set (so the time is invalid) or if bad data from the RTC
    uint8_t rtcState = RV3032_read_state();

    if (rtcState==2) {

        // We got bad data from the RTC. Either defective chip or PCB connection?

        // Mind as well turn it off since we have no idea what it is doing.
        depower_rv3032();
        error_mode( ERROR_BAD_CLOCK );

    }

    if (rtcState==1) {

        // RTC lost power at some point so nothing we can do except show an error message forever.

        if (persistent_data.launch_flag) {
            lcd_show_batt_errorcode( BATT_ERROR_PRELAUNCH );
        } else {
            lcd_show_batt_errorcode( BATT_ERROR_POSTLAUNCH );
        }

        rv3032_shutdown();          // We can't use the RTC, so shut it down to save some power while we show error message, otherwise it would be in it's default powerup 32768 mode that uses like 3uA. NOte that we do not charge the backup cap in this mode becuase there is no point.
        blinkforeverandever();

    }

    // Wait for any clkout transition so we know we have at least 500ms to read out time and activate interrupt before time changes so we dont miss any seconds.
    // This should always take <500ms
    // This also proves the RV3032 is running and we are connected on clkout
    unsigned start_val = TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B );
    while (TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B )==start_val); // wait for any transition

    // Clear any pending interrupts from the RV3032 clkout pin
    // We we should not get a real one for 500ms so we have time to do our stuff

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );

    // If we get here then we know the RTC is good and that it has good clock data (ie it has been continuously powered since it was commissioned at the factory)

    // Read the time from the RTC into our global time variables. This will be either (1) the time since we were commissioned if not launched, or (2) the time since launch if we were launched.
    RV_3032_read_time();

    if ( persistent_data.launch_flag == 0 ) {

        // We have never launched!

        // First we need to make sure that the trigger pin is inserted because
        // we would not want to just launch because the pin was out when batteries were inserted.
        // This also lets us test both trigger positions during commissioning at the factory.

        CBI( TRIGGER_PDIR , TRIGGER_B );      // Input
        SBI( TRIGGER_PREN , TRIGGER_B );      // Enable pull resistor
        SBI( TRIGGER_POUT , TRIGGER_B );      // Pull up


        mode = LOAD_TRIGGER;                  // Go though the state machine to wait for trigger to be loaded
        step =0;                              // Used to slide a dash indicator pointing to the pin location

        // We will go into "load pin" mode when next second ticks. This gives the pull-up a chance to take effect and also avoids any bounce aliasing right at power up.

    } else {

        // We have already been triggered and time is already loaded into variables by the RV3032_read_time() function.

        // Show the time on the display
        // Note that the time was loaded from the RTC when we initialized it.
        // We only do this ONCE per set of batteries so no need for efficiency.

        lcd_show_digit_f(  6 , (days / 1      ) % 10  );
        lcd_show_digit_f(  7 , (days / 10     ) % 10  );
        lcd_show_digit_f(  8 , (days / 100    ) % 10  );
        lcd_show_digit_f(  9 , (days / 1000   ) % 10  );
        lcd_show_digit_f( 10 , (days / 10000  ) % 10  );
        lcd_show_digit_f( 11 , (days / 100000 ) % 10  );

        lcd_show_digit_f( 4 , hours % 10  );
        lcd_show_digit_f( 5 , hours / 10  );
        lcd_show_digit_f( 2 , mins % 10  );
        lcd_show_digit_f( 3 , mins / 10  );
        lcd_show_digit_f( 0 , secs % 10  );
        lcd_show_digit_f( 1 , secs / 10  );

        // Now start ticking at next second tick interrupt
        mode = TIME_SINCE_LAUNCH;
    }


    // Wait for interrupt to fire at next clkout low-to-high change to drive us into the state machine (in either "pin loading" or "time since lanuch" mode)

    __bis_SR_register(LPM4_bits | GIE );                 // Enter LPM4
    // BIS.W    #248,SR

    __no_operation();                                   // For debugger

    // We should never get here

    error_mode( ERROR_MAIN_RETURN );                    // This would be very weird if we ever saw it.


}




