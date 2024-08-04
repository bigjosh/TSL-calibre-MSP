#include <msp430.h>
#include <limits.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#include "error_codes.h"
#include "lcd_display.h"

#include "ram_isrs.h"

#include "tsl_asm.h"

//#include "timeblock.h"
#include "persistent.h"

#define RV_3032_I2C_ADDR (0b01010001)           // Datasheet 6.6

/*
// DEBUG VERSIONS
#define DEBUG_PULSE_ON()     SBI( DEBUGA_POUT , DEBUGA_B )
#define DEBUG_PULSE_OFF()    CBI( DEBUGA_POUT , DEBUGA_B )
*/

// PRODUCTION VERSIONS
#define DEBUG_PULSE_ON()     {}
#define DEBUG_PULSE_OFF()    {}

// Which PCB?
//#define C2_IS_10K       // For PCBs after 9/2024 which have C2 replaced with a 10K resistor.

// Put the LCD into blinking mode

void lcd_blinking_mode() {
    LCDBLKCTL = LCDBLKPRE__64 | LCDBLKMOD_2;       // Clock prescaler for blink rate, "10b = Blinking of all segments"
}


// Turn off power to RV3032 (also takes care of making the IO pin not float and disabling the interrupt)
void depower_rv3032() {

    // Make Vcc pin to RV3032 ground. NOte that the RTC will likely keep running for a while off the backup
    // capacitor, but this should put it into backup mode where CLKOUT is disabled.
    CBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now the CLKOUT pin is floating, so we will pull it
    SBI( RV3032_CLKOUT_PREN , RV3032_CLKOUT_B );    // Enable pull resistor (does not matter which way, just keep pin from floating to save power)

    CBI( RV3032_CLKOUT_PIE , RV3032_CLKOUT_B );     // Disable interrupt.
    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );    // Clear any pending interrupt.

}

// Goes into LPM3.5 to save power since we can never wake from here.
// In LPMx.0 draws 1.38uA with the "First STart" message.
// In LPMx.5 draws 1.13uA with the "First STart" message.

// Clearing GIE does not prevent wakeup from LPMx.5 so all interrupts must have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void sleepforeverandever(){


    /*

    // Manually disconnect the x.5 regulator switch Note that this does not seem to save any power over leaving automatic on.
    // Maybe this can save some power to leave the switch open when we are actually running the ISP, as long as we only write to the LCD
    // once? If we write more than once then we need to close the switch because LCD can only run at 40KHz on the small regulator.


    PM5CTL0 = LPM5SM                     // "Manual mode. The LPM3.5 switch is specified by LPM5SW bit setting in software."
                                         // "0b = LPMx.5 switch disconnected". Now the RTC and LCD are only powered from the tiny LPM3.5 regulator, so you can only Access them slowly (we do not access them at all from now on).
              | LOCKLPM5;                // Lock I/O pin and other LPMx.5 relevant (for example, RTC) configurations upon
                                         // entry to or exit from LPMx.5. After the LOCKLPM5 bit is set, it can be cleared
                                         // only by software or by a power cycle.
                                         // This bit is reset by a power cycle.

    */

    /*
    // These enable the LPMx.5 mode

    // First start message in LPM3   Mode = 1.25uA
    // First start message in LPM3.5 Mode = 1.56uA, but takes 350us to wake up compared to 10us.

    PMMCTL0 = PMMPW                    // Open PMM Registers for write
                                          // We do not include SVSHE so supervisor is off.
              | PMMREGOFF_L                // Set PMMREGOFF. "Regulator is turned off when going to LPM3 or LPM4. System enters LPM3.5 or LPM4.5, respectively."
              ;
*/
    // Make sure no pending interrupts here or else they might wake us!
    // Note that there is no power difference between LPM3 and LPM4 here, probably because OSC is already off because we are using VLO for the LCD?
    __bis_SR_register(LPM4_bits);     // // Enter LPM4 and sleep forever and ever and ever (no interrupts enabled so we will never get woken up)

}

// Assumes all interrupts have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void blinkforeverandever(){
    lcd_blinking_mode();
    sleepforeverandever();
}

// Assumes all interrupts have been individually disabled.
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

// Just gets all IO pins into an idle state
// Powers up the LCD bias voltage generator
// Powers up the RTC and configures MCU pin RV3032_CLKOUT as interrupt, but does not enable the interrupt.
// Does not enable interrupts on any pins


void initGPIO() {

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
    SBI( TSP_IN_POUT , TSP_IN_B );          // Power up TSP
    SBI( TSP_IN_PDIR , TSP_IN_B );

    SBI( TSP_ENABLE_POUT , TSP_ENABLE_B );  // Enable TSP
    SBI( TSP_ENABLE_PDIR , TSP_ENABLE_B );

    // --- Debug pins

    // Debug pins
    SBI( DEBUGA_PDIR , DEBUGA_B );          // DEBUGA=Output. Currently used to time how long the ISR takes

    CBI( DEBUGB_PDIR , DEBUGB_B );          // DEBUGB=Input
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
    // Note that we do not *enable* it yet.

    CBI( RV3032_CLKOUT_PDIR , RV3032_CLKOUT_B );    // Set input
    CBI( RV3032_CLKOUT_PIES , RV3032_CLKOUT_B );    // Interrupt on rising edge (Driven by RV3032 CLKOUT pin which we program to run at 1Hz).

    /* RV3230 App Guide Section 4.21.1:
     *  the 1 Hz clock can be enabled
     *  on CLKOUT pin. The positive edge corresponds to the 1 Hz tick for the clock counter increment (except for
     *  the possible positive edge when the STOP bit is cleared).
     */

    // Note that we do not enable the CLKOUT interrupt until we enter RTL or TSL mode

    // --- Trigger

    // By default we set the trigger to drive low so it will not use power regardless if the pin is in or out.
    // When the trigger is out, the pin is shorted to ground.
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
    // Note if you change these then you also have to adjust lcd_show_squigle_animation()

    LCDM4 =  0b01001000;  // L09=MSP_COM2  L08=MSP_COM3
    LCDM5 =  0b00010010;  // L10=MSP_COM0  L11=MSP_COM1

    LCDBLKCTL = 0x00;       // Disable blinking. We do this because this bit is not cleared on reset so if we are resetting out of at blinking mode then it would otherwise persist.

    LCDCTL0 |= LCDON;                                           // Turn on LCD

}


// Just to make sure that everything lines up with the actual RV3032 hardware. Note we cannot put thin in the header file because the assembler chokes on it.

static_assert( sizeof( rv3032_time_block_t ) == 7 , "The size of this structure must match the size of the block actually read from the RTC");


// RV3032 Registers

#define RV3032_HUNDS_REG 0x00    // Hundredths of seconds
#define RV3032_SECS_REG  0x01
#define RV3032_MINS_REG  0x02
#define RV3032_HOURS_REG 0x03
#define RV3032_DAYS_REG  0x05
#define RV3032_MONS_REG  0x06
#define RV3032_YEARS_REG 0x07

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

// We write this to the RTC at the moment the trigger pin is pulled, so it starts counting up from here.
// We never read it or use it for anything after that. The write is mostly just to reset the seconds prescaller back to zero so that count
// starts right at the pull rather than being off by a partial second.

rv3032_time_block_t rv_3032_time_block_init = {
  0,        // Sec
  0,        // Min
  0,        // Hour
  1,        // Weekday
  1,        // Date
  1,        // Month
  0         // Year
};


static const unsigned minutes_per_hour = 60;
static const unsigned hours_per_day = 24;
static const unsigned minutes_per_day = (minutes_per_hour * hours_per_day);

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

// Tell compiler/linker to put this in "info memory" that we set up in the linker file to live at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.
persistent_data_t __attribute__(( __section__(".persistant") )) persistent_data;

// Here we pull out the address of the mins counter in the persistent data structure for no other reason than to pass it to the ASM
// code. We have to do this because the ASM code can not get this address directly since the assembler seems to choke on nested structs.
volatile unsigned *persistant_mins_ptr = &persistent_data.mins;

// Note that we do *not* need the password here. This fact is hidden in a hard find footnote in 1.16.2.1 in the application manual "These bits have no affect on MSP430FR413x, MSP430FR203x devices."
inline void unlock_persistant_data() {
    SYSCFG0 &= ~DFWP;                     // 0b = Data (Information) FRAM write enable. Compiles to a single instruction.
}

inline void lock_persistant_data() {
    SYSCFG0 |= DFWP;                      // 1b = Data (Information) FRAM write protected (not writable). Compiles to a single instruction.
}

// Initialize RV3032 for the first time
// sets clkout to 1Hz
// disables backup capacitor
// disabled the automatic refresh of config registers from EEPROM.
// Does not enable any interrupts

// Note that we use CLKOUT at 1Hz rather than using a 1 sec periodic timer because the periodic timer uses *much* more power all the time on the RTC 9not documented!) and slightly more power on the MCU because it is open collector pulling on a pull-up resistor.
// It would have been slightly nice to set the periodic timer to 2Hz and then interrupt on both edges. :/

void rv3032_init() {

    // Give the RV3230 a chance to wake up before we start pounding it.
    // POR refresh time(1) At power up ~66ms
    // Also there is Tdeb which is the time it takes to recover from a backup switch over back to Vcc. It is unclear if this is 1ms or 1000ms so lets be safe.
    __delay_cycles(1100000);    // 1 sec +/-10% (we are running at 1Mhz)

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Set all the registers we care about that can get reset by either power-on-reset or recover from backup
    //uint8_t clkout2_reg = 0b00000000;        // CLKOUT XTAL low freq mode, freq=32768Hz
    //uint8_t clkout2_reg = 0b00100000;        // CLKOUT XTAL low freq mode, freq=1024Hz
    uint8_t clkout2_reg = 0b01100000;        // CLKOUT XTAL low freq mode, freq=1Hz
    i2c_write( RV_3032_I2C_ADDR , 0xc3 , &clkout2_reg , 1 );

    // First control reg. Note that turning off backup switch-over seems to save ~0.1uA
    //uint8_t pmu_reg = 0b01000001;         // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd.
    //uint8_t pmu_reg = 0b01010000;          // CLKOUT off, Direct backup switching mode, no charge pump, 0.6K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.
    //uint8_t pmu_reg = 0b01100001;         // CLKOUT off, Level backup switching mode (2v) , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd. Predicted to use ~200nA more than disabled because of voltage monitor.
    //uint8_t pmu_reg = 0b01000000;         // CLKOUT off, Other disabled backup switching mode, no charge pump, trickle resistor off, trickle charge Vbackup to Vdd

    #ifdef C2_IS_10K
        uint8_t pmu_reg = 0b00000000;          // CLKOUT ON, backup switching disabled
    #else
        uint8_t pmu_reg = 0b00011101;        // CLKOUT ON, Direct backup switching mode, no charge pump, 12K OHM trickle resistor, trickle charge Vbackup to Vdd.
    #endif
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    uint8_t control1_reg = 0b00000100;      // TE=0 so no periodic timer interrupt, EERD=1 to disable automatic EEPROM refresh (why would you want that?).
    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    i2c_shutdown();

}

// Switch the CLKOUT from 1Hz to 64Hz

void rv3032_switchto_64Hz() {
    // Initialize our i2c pins as pull-up
    i2c_init();

    uint8_t clkout2_reg = 0b01000000;        // CLKOUT XTAL low freq mode, freq=64Hz
    i2c_write( RV_3032_I2C_ADDR , 0xc3 , &clkout2_reg , 1 );

    i2c_shutdown();
}


// Turn off the RTC to save some power. Only use this if you will NEVER need the RTC again (like if you are going into error mode).

void rv3032_shutdown() {

    i2c_init();

    uint8_t pmu_reg = 0b01000000;         // CLKOUT off, backup switchover disabled, no charge pump
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    i2c_shutdown();

}

// During time-since-launch mode, this is how long we have been counting.
// These are global so we can easily share them into the ASM code (If we passed via the call convention then we'd have to mess around with the stack).
// Note that the ASM code only takes a snapshot of these into registers and then uses the registers for canonical storage.
// Note that there is no `tsl_days` variable, the `days_digits[]` are the canonical storage for the running day count, and these are only ever updated
// in C inside `tsl_next_day()`

unsigned tsl_secs=0;
unsigned tsl_mins=0;
unsigned tsl_hours=0;

//**** INTERRUPT STUFFS

// We spend most of our lives in "Time Since Launch" mode, so we program the default vector for the CLKOUT tick ISR in FRAM to point to that handler.
// For the times we spend with that vector doing other things, we switch over to the RAM-based vector table.

// Shortcuts for setting the RAM vectors. Note we need the (void *) casts because the compiler won't let us make the vectors into `near __interrupt (* volatile vector)()` like it should.

#define SET_CLKOUT_VECTOR(x) do {RV3032_CLKOUT_VECTOR_RAM = (void *) x;} while (0)
#define SET_TRIGGER_VECTOR(x) do {TRIGGER_VECTOR_RAM = (void *) x;} while (0)

// Terminate after one day
bool testing_only_mode = false;

// Called when trigger pin changes high to low, indicating the trigger has been pulled and we should start ticking.
// Note that this interrupt is only enabled when we enter ready-to-launch mode, and then it is disabled and also the pin in driven low
// when we then switch to time-since-lanuch mode, so this ISR can only get called in ready-to-lanuch mode.

__interrupt void trigger_isr(void) {

    DEBUG_PULSE_ON();

    // First we delay for about 1000 cycles / 1.1Mhz  = ~1ms
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

    if ( !triggerpin_level ) {      // trigger still pulled?

        // WE HAVE LIFTOFF!

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

        unlock_persistant_data();
        // First get current time and save it to FRAM for archival purposes.
        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , (void *)  &persistent_data.launched_time , sizeof( rv3032_time_block_t ) );
        // Also update the persistent storage to reflect that we launched now.
        persistent_data.mins=0;
        persistent_data.days=0;
        persistent_data.update_flag=0;
        persistent_data.launched_flag=0x01;
        lock_persistant_data();

        // Then zero out the RTC to start counting over again, starting now. Note that writing any value to the seconds register resets the sub-second counters to the beginning of the second.
        // "Writing to the Seconds register creates an immediate positive edge on the LOW signal on CLKOUT pin."
        i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &rv_3032_time_block_init , sizeof( rv3032_time_block_t ) );

        i2c_shutdown();

        // Note that the tsl_* variables will already be initialized to zero from power up

        // We will get the next tick with a rising edge on CLKOUT in 1000ms. Make sure we return from this ISR before then.

        // Clear any pending RTC interrupt so we will not tick until the next pulse comes from RTC in 1000ms
        // There could be a race where an RTC interrupt comes in just after the trigger was pulled, in which case we would
        // have a pending interrupt that would get serviced immediately after we return from here, which would look ugly.
        // We could also see an interrupt if the CLKOUT signal was low when trigger pulled (50% likelihood) since it will go high on reset.

        CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear any pending interrupt from CLKOUT

        // Flash lights

        flash();

        // Start ticking from... now!
        // (We rely on the secs, mins, hours, and days_digits[] all having been init'ed to zeros.)

        // Begin TSL mode on next tick
        SET_CLKOUT_VECTOR( &TSL_MODE_BEGIN );

    }

    DEBUG_PULSE_OFF();

}

/*

 1024hz RTL pattern readings with Energytrace over 5 mins
 FRPWR is cleared on each ISR call with the following line at the very top...
      BIC.W     #FRPWR,(GCCTL0) ;           // Disable FRAM. Writing to the register enables or disables the FRAM power supply.  0b = FRAM power supply disabled.
 It would be nice if we could permanently disable the FRAM, but does not seem possible on this chip because it is automatically enabled when exiting LPM.

 RAMFUNC  FRPWR  CLKOUT   I
 =======  =====  =====    =======
   Y        0     1kHz     0.354mA
   Y        0     1kHz     0.351mA
   Y        1     1kHz     0.385mA
   Y        1     1kHz     0.371mA
   Y        1     1kHz     0.372mA
   Y        1     1Hz      0.0021mA
   Y        1     1Hz      0.0021mA
   Y        0     1Hz      0.0021mA
   N        1     1Hz      0.0027mA
   N        1     1Hz      0.0027mA

*/


enum mode_t {
    LOAD_TRIGGER,           // Waiting for pin to be inserted (shows dashes)
    ARMING,                 // Hold-off after pin is inserted to make sure we don't inadvertently trigger on a switch bounce (lasts 0.5s)
};

mode_t mode;

unsigned int step;          // LOAD_TRIGGER      - Used to keep the position of the dash moving across the display (0=leftmost position, only one dash showing)
                            // READY_TO_LAUNCH   - Which step of the squigle animation

// Handle startup activities like loading the pin. Terminates into ready-to-launch mode
__interrupt void startup_isr(void) {

    DEBUG_PULSE_ON();

    if ( mode==ARMING ) {

        // This phase is just to add a 1000ms debounce to when the trigger is initially inserted at the factory to make sure we do
        // not accidentally fire then.

        // We are currently showing "Arming" message, and we will switch to squiggles on next tick if the trigger is still in

        // If trigger is still inserted 1 second later  (since we entered arming mode) , then go into READY_TO_LAUNCH were we wait for it to be pulled

        if ( TBI( TRIGGER_PIN , TRIGGER_B )  ) {        // Trigger still pin inserted? (switch open, pin high)

            // Now we need to setup interrupt on trigger pull to wake us when pin  goes low
            // This way we can react instantly when the user pulls the pin.

            SBI( TRIGGER_PIE  , TRIGGER_B );          // Enable interrupt on the pin attached to the trigger switch.
            SBI( TRIGGER_PIES , TRIGGER_B );          // Interrupt on high-to-low edge. Normally pulled-up by the MSP while pin is inserted (switch lever is depressed)

            // Clear any pending interrupt so we will need a new transition to trigger
            CBI( TRIGGER_PIFG , TRIGGER_B);

            // Next tick will enter the ready-to-launch mode animation which will continue until the trigger is pulled.
            SET_CLKOUT_VECTOR( &RTL_MODE_BEGIN );

            // When the trigger is pulled, it will generate a hardware interrupt and call this ISR which will start time since launch mode.
            SET_TRIGGER_VECTOR(trigger_isr);

            // Finally we set the persistent flag so we remember that we now have been officially commissioned and are ready to launch.

            unlock_persistant_data();
            persistent_data.launched_flag=0xff;
            persistent_data.commisisoned_flag=0x01;           // Ready for next step in launch sequence
            lock_persistant_data();

        } else {

            // In the unlikely case where the pin was inserted for less than 500ms, then go back to the beginning and show dash scroll and wait for it to be inserted again.
            // This has the net effect of making an safety interlock that the pin has to be inserted for a full 1 second before we will arm.

            mode = LOAD_TRIGGER;
            step = 0;

        }

    } else { // if mode==LOAD_TRIGGER

        // Check if trigger is inserted (pin high)

        if ( TBI( TRIGGER_PIN , TRIGGER_B ) ) {       // Check if trigger has been inserted yet

            // Trigger pin is in, we can arm now.

            lcd_show_arming_message();

            //Start showing the ready to launch squiggles on next tick after that

            mode = ARMING;

            // note that here we wait a full tick before checking that the pin is out again. This will debounce the switch for 1s when the pin is inserted.

        } else {

            // trigger pin still out
            // Show message so person knows why we are waiting (yes, this is redundant, but heck the pin is out so we are burning fuel against the trigger pull-up anyway
            // We do not display this message in the case where the pin is already in at startup since it would be confusing then.
            lcd_show_load_pin_message();

            step++;

            if (step==LOAD_PIN_ANIMATION_FRAME_COUNT) {
                step=0;
            }

            lcd_show_load_pin_animation(step);

        }
    }

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

    DEBUG_PULSE_OFF();

}



// Spread the 6 day counter digits out to minimize superfluous digit updates and avoid mod and div operations.
// These are set either when trigger is pulled or on power up after a battery change.
// day_digits[0] is the ones digit, day_digits[5] is the 100 thousands digit.

unsigned days_digits[6];


// Called by the ASM TSL_MODE_ISR when it rolls over from 23:59:59 to 00:00:00
// Since it is called from ASM, we need the `extern "C"` to keep the name from getting mangled.

extern "C" void tsl_new_day() {

    unsigned long days = persistent_data.days;          // Copy to a local variable for faster access (the persistent variable is volatile)

    // Update the count in persistent data to reflect that yet another day has passed.
    unlock_persistant_data();
    persistent_data.backup_mins=persistent_data.mins;
    persistent_data.backup_days=days;
    persistent_data.update_flag = 1;                // BEGIN TRANSACTION
    persistent_data.mins = 0;                       // We hard set the mins to zero. It should be at 24*60 when we get here.
    days++;
    persistent_data.days = days;
    persistent_data.update_flag = 0;                // END TRANSACTION
    lock_persistant_data();


    if ((days &  ~127) == days ) {

        lcd_save_screen_buffer_t lcd_screen_save_buffer;
        lcd_save_screen(&lcd_screen_save_buffer);
        lcd_show_centesimus_dies_message();
        // Switch the MCU over to the very slow ~10KHz VLO clock. Things will take forever, but low power.
        CSCTL4 = SELMS__VLOCLK;
        __delay_cycles(5000UL);
        // Switch the MCU back to DCO clock which uses more power per time, but more than compensates for it by getting even more done per time.
        CSCTL4 = 0;
        lcd_restore_screen(&lcd_screen_save_buffer);

    }

    // Update the actual display to reflect the new day
    // We could have done this with meta code in a template, but I think that would have been even uglier! At least this is clear.

    days_digits[0]++;

    if (days_digits[0] == 10 ) {

        days_digits[0]=0x00;

        days_digits[1]++;

        if (days_digits[1] == 10 ) {

            days_digits[1]=0x00;

            days_digits[2]++;

            if (days_digits[2] == 10 ) {

                days_digits[2]=0x00;

                days_digits[3]++;

                if (days_digits[3] == 10 ) {

                    days_digits[3]=0x00;

                    days_digits[4]++;

                    if (days_digits[4] == 10 ) {

                        days_digits[4]=0x00;

                        days_digits[5]++;

                        if (days_digits[5] == 10 ) {

                            // If we get here then we just passed 1 million days, so go into long now mode.
                            lcd_show_long_now();
                            blinkforeverandever();

                            // unreachable

                        }
                        lcd_show_digit_f( 11 , days_digits[5] );
                    }
                    lcd_show_digit_f( 10 , days_digits[4] );
                }
                lcd_show_digit_f(  9 , days_digits[3] );
            }
            lcd_show_digit_f(  8 , days_digits[2] );
        }
        lcd_show_digit_f(  7 , days_digits[1] );
    }
    lcd_show_digit_f(  6 , days_digits[0] );
}


// Reference version of the ready to launch animation. This has been replaced with optimized ASM in tsl_asm.asm
void ready_to_launch_reference() {
    // We depend on the trigger ISR to move us from ready-to-launch to time-since-launch

    // Do the increment and normalize first so we know it will always be in range
    step++;
    step &=0x07;

    lcd_show_squiggle_frame(step );

    static_assert( SQUIGGLE_ANIMATION_FRAME_COUNT == 8 , "SQUIGGLE_ANIMATION_FRAME_COUNT should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07

}

// Note that we do not make this an "interrupt" function even though it is an ISR.
// We can do this because we know that there is nothing being interrupted since we just
// sleep between interrupts, so no need to save any registers. We do need to do our own
// RETI (return from interrupt) at the end since the function itself will only have a
// normal return.

// -- normal naked ISR
// 24us
// 2.04uA with zeros
// 161uA peak
// 29.44us wake time

// -- ramfunc, FRAM controller on
// 21us
// 1.49uA with dashes
// 2.17uA with zeros
// 26.62us wake time

// -- ramfunc, FRAM controller off
// 21us
// 2.04uA with zeros
// peak 119uA
// 26us wake time

// -- normal naked ISR + 500 cycles
// 500us
// 2.15uA with zeros (1.9uA @ 1.8mA range, 2.15uA @ auto)
// 240uA peak (3mA on auto)

// -- ramfunc, FRAM controller off + 500 cycles
// 499us
// 2.15uA with zeros (2.15uA auto)
// 4.9mA peak on auto. Zoomed in shows 245uA durring the 500us


// TODO: Wait for Vcc to drop to 3V and then check again some time later to make sure it has not fallen too low which would indicate current draw too high.



/*

unsigned int measureVcc(void)
{
    // Configure ADC
    ADCCTL0 |= ADCSHT_2 | ADCON;                // ADCON, S&H=16 ADC clks
    ADCCTL1 |= ADCSHP;                          // ADCCLK = MODOSC; sampling timer
    ADCCTL2 |= ADCRES;                          // 10-bit conversion results
    ADCMCTL0 |= ADCSREF_1 | ADCINCH_1;          // ADC input ch A1 => 1.5V ref

    // Configure internal reference
    PMMCTL0_H = PMMPW_H;                        // Unlock PMM registers
    PMMCTL2 |= INTREFEN | REFVSEL_0;            // Enable internal 1.5V reference

    while(!(PMMCTL2 & REFGENRDY));              // Poll till internal reference settles

    // Sampling and conversion start
    ADCCTL0 |= ADCENC | ADCSC;                  // Sampling and conversion start
    while (!(ADCCTL1 & ADCIFG));                // Wait for conversion to complete
    unsigned int result = ADCMEM0;              // Read ADC conversion result

    // Calculate Vcc
    unsigned long vcc = ((unsigned long)result * 15000) / 1023;

    return (unsigned int)vcc;                   // Return Vcc in millivolts
}

*/

// Read the current supply voltage times 100 (so 3V = 3000)
// Returns with ADC turned off


unsigned batt_v_x1000() {

    // Configure internal reference
    PMMCTL0_H = PMMPW_H;                 // Unlock the PMM registers
    PMMCTL2 = INTREFEN ;      // Enable internal reference, set to 1.5V

    while(!(PMMCTL2 & REFGENRDY));       // Wait for reference generator to settle


    ADCCTL0 &= ~ADCENC;                     // Disable ADC

    ADCCTL0 |= ADCON ;

    ADCCTL1 = ADCSHP;                       // ADCCLK = MODOSC; sampling timer
    ADCCTL2 = ADCRES_0;                     // 8-bit conversion results

//    ADCCTL1 = ADCSHS_2;         // Select SMCLOCK because (I think) we already get it because it is derived off the REF clock that the CPU runs off.
    ADCCTL1 = ADCSHS_2;         // Select SMCLOCK because (I think) we already get it because it is derived off the REF clock that the CPU runs off.

    ADCMCTL0 = ADCSREF_0        // "{VR+ = AVCC and VR– = AVSS }"
               | ADCINCH_13     // "The 1.5-V reference is internally connected to ADC channel 13."
    ;

    if ( ADCCTL1 & ADCBUSY ) {
        lcd_show_digit_f( 7 , 1 );
    } else {
        lcd_show_digit_f( 7 , 0 );

    }

    ADCCTL0 |= ADCENC | ADCSC ;       // Turn on the ASDC, enable conversion, start a conversion. "ADCSC and ADCENC may be set together with one instruction."

    if ( ADCCTL1 & ADCBUSY ) {
        lcd_show_digit_f( 6 , 1 );
    } else {
        lcd_show_digit_f( 6 , 0 );
    }


    lcd_show_digit_f( 8 , 8 );

//    while ( (ADCIFG & ADCIFG0) == 0 );               // "The ADCIFG0 is set when an ADC conversion is completed. This bit is reset when the ADCMEM0 get read, or it may be reset by software."

    while ( ADCCTL1 & ADCBUSY );                        // "ADC busy. This bit indicates an active sample or conversion operation."

    lcd_show_digit_f( 9 , 9 );

    unsigned result = ADCMEM0; // 10 bit result.

    // DVCC = (1023 × 1.5 V) ÷ 1.5-V reference ADC result

    ADCCTL0 &= ~ADCON;       // Turn off the ASDC

    return result;

}


// Turn on the ADC, set it to measure the 1.5V reference against Vcc voltage with 8 bit result.

void adc_vcc_init() {

    // Configure reference module located in the PMM
    PMMCTL0_H = PMMPW_H;        // Unlock the PMM registers
    PMMCTL2 = INTREFEN ;        // Enable internal 1.5V reference
    // Lets do some other work and then we will check to make sure the REF is ready later. This will give it some time to warm up.

    ADCCTL0 &= ~ADCENC;                     // Disable ADC/ "With few exceptions, the ADC control bits can be modified only when ADCENC = 0."
    ADCCTL0 = ADCSHT_0 | ADCON;             // ADCON, Sample for 4 ADC clks, which is the shortest time available. Short time=less power and seems to be long enough for our purposes.
//    ADCCTL1 = ADCSHP;                       // ADCCLK = MODOSC; sampling timer
    ADCCTL1 =
            ADCSHP              // "1b = SAMPCON signal is sourced from the sampling timer", which is specified by the ADCSHTx bits.
            | ADCSHS_0          // Select MODCLOCK because SMCLOCK does not work and I do not want to try to figure out why.
    ;

    ADCCTL2 = ADCRES_0;                     // 8-bit conversion results. We do not need much. Readings on the way down: High threshold 3.3V 115=3.326V 116=3.297V, low threshold 2.3V 166=2.304V 167=2.290V

    ADCMCTL0 = ADCSREF_0        // "{VR+ = AVCC and VR– = AVSS }"
               | ADCINCH_13     // "The 1.5-V reference is internally connected to ADC channel 13."
    ;


    ADCCTL0 |= ADCENC;                               // Enable ADC

    while(!(PMMCTL2 & REFGENRDY));          // Poll till internal reference settles


}

void adc_shutdown() {



    // Configure reference module located in the PMM
    PMMCTL0_H = PMMPW_H;        // Unlock the PMM registers
    PMMCTL2 &= ~INTREFEN ;      // Disable internal 1.5V reference

    ADCCTL0 &= ~ADCENC;            // Disable ADC
    ADCCTL0 &= ~ADCON;             // ADC off

}

// Samples ADC

unsigned int adc_measure()
{

    ADCCTL0 |= ADCSC;                               // Start conversion.

    while ( ADCCTL1 & ADCBUSY );                    // "ADC busy. This bit indicates an active sample or conversion operation."

    unsigned result = ADCMEM0; // 10 bit result.

    return result;
}

// What would the VccADC result be (in 8 bit mode) for the given number of millivolts?
constexpr unsigned adc_mv_to_vcc_adc8bit(unsigned mv) {
    // 255 is the full range 100% reading for the ADC, 1500 is the 1.5V reference voltage we are measuring, expressed in mV
    return  (255UL * 1500UL) / mv;
}


// We keep a RAM copy because update-in-place to FRAM requires two writes and a read. To store a value to FRAM is just a single write.
static volatile unsigned power_rundown_counter_ram =0;

// Called at 64Hz by the CLKOUT from the RTC while we are powering down to measure how long we can keep running and thus much current we are using.
// 3.93uA@3.3V with all LCD segments lit and ISR running at 64Hz.
// 3.25uA@1.92V " " "
// 2.02uA just LCD, no interrupts.
__interrupt void POWERDOWN_TEST_ISR(void) {

    power_rundown_counter_ram+=1;
    unlock_persistant_data();
    persistent_data.porsoltCount=power_rundown_counter_ram;        // Note that this is an atomic update.
    lock_persistant_data();

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear pending interrupt from CLKOUT

    // Go back to sleep.
}


// Indirectly measure how much power this unit uses by measuring how long it takes to use up the energy stored in the decoupling cap.
// The power supply should be disconnected after this function is called. This function never returns, the number of 0.1s cycles we were
// able to keep running for is stored into `persistent_data.porsoltCount`.
// Never returns.


void power_rundown_test() {

    // Set up the ADC to indirectly measure the Vcc voltage

    adc_vcc_init();

    // Enable the high-side voltage supervisor. This will reliably put us into reset at 1.8V on the way down if we are sleeping when we cross the threshold. Does use slightly more power, but I am not sure we can rely on the BOR or when that kicks in?
    PMMCTL0 = PMMPW                  // Open PMM Registers for write
            | SVSHE                  // "1b = SVSH is always enabled."
        ;

    unlock_persistant_data();
    persistent_data.porsoltCount = 0;       // Reset our deathwatch counter. Will be peridocically incremented in the RTC ISR
    lock_persistant_data();

    // Call our ISR when CLKOUT clicks
    SET_CLKOUT_VECTOR( &POWERDOWN_TEST_ISR );
    ACTIVATE_RAM_ISRS();

    // Set up the RTC to wake us up at 64Hz and reset the prescaler to the top of the second
    rv3032_switchto_64Hz();


    // First make sure we start with a high enough voltage
    if ( !( adc_measure() <= (adc_mv_to_vcc_adc8bit(3300) +1 ) ) ) {
        lcd_show_lo_volt_message( adc_measure()  );
        blinkforeverandever();
    };

    // Now wait for the voltage to drop, which indicates that the power supply is disconnected

    while ( adc_measure() <= (adc_mv_to_vcc_adc8bit(3300) +1 ) );    // The MSP-EZ supplies 3.325V, so when we drop below 3.3V then we are starting the slow decline into power death.
                                                                     // Note that since we are measuring the 1.5V reference against the Vcc voltage that the measurement result will up down as the voltage goes down (as Vcc approaches Vref)

    // When we get here, the Vcc voltage is lower than 3.3V and on the way down.
    // So now we will count how long we stay alive before dying to measure how much current we are using while we wait to die.

    // Note that we are not sure where the RV3032 prescaller is when we hit here, so this introduces up to 1/64th of a second of jitter to our reading.

    // Don't need the ADC anymore, and leaving it on would increase power and shorten our glide time.
    adc_shutdown();

    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // We we should not get a real one for 500ms so we have time to do our stuff


    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );
    SBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );

    // Put all 8's on the display. This will lite every segment so any short on any segment will show up as power drain.
    // This also lets the operator know that we know that we are dying. If the display stays on "First Start" after power is pulled, then we know that a Blotzman Battery has formed in the circuit.
    lcd_show_all_8s_message();


    // Wait for RV3032 CLKOUT interrupts to fire on clkout. Our ISR will count how many we see before we run out of juice.
    // Note if we use LPM3_bits then we burn 18uA versus <2uA if we use LPM4_bits.
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger

}


int main( void )
{

    WDTCTL = WDTPW | WDTHOLD | WDTSSEL__VLO;   // Give WD password, Stop watchdog timer, set WD source to VLO
                                               // The thinking is that maybe the watchdog will request the SMCLK even though it is stopped (this is implied by the datasheet flowchart)
                                               // Since we have to have VLO on for LCD anyway, mind as well point the WDT to it.
                                               // TODO: Test to see if it matters, although no reason to change it.


    initGPIO();
    initLCD();

    // Disable the voltage supervisor. This would normally monitor the voltage and put us into reset if it got low, but there is nothing we can do if it does get low
    // so no point wasting power on it.

    PMMCTL0 = PMMPW                  // Open PMM Registers for write
                                     // We do not include SVSHE so supervisor is off.
        ;


    // Power up display with a nice dash pattern
    lcd_show_dashes();

    // Initialize the RV3032 with proper clkout & backup switchover settings. Leaves the i2c connection shutdown.
    // Note that once we get the RTC to give us our ticks then we should never need to touch it again.
    rv3032_init();


    // Initialize the lookup tables we use for efficiently updating the LCD
    initLCDPrecomputedWordArrays();

    // TEST CODE GOES HERE

    if (persistent_data.initalized_flag!=0x01) {

        // This is the first time we have ever powered up

        // Flash the LEDs to prove they work.
        flash();


        // Now remember that we did our start up. From now on, the RTC will run on its own forever.
        unlock_persistant_data();
        persistent_data.tsl_powerup_count=0;
        persistent_data.commisisoned_flag=0xff;           // Ready for next step in setup sequence
        persistent_data.initalized_flag=0x01;             // Remember that we already started up once and never do it again.
        lock_persistant_data();


        // Next we will do a power usage proving test to check to make sure this unit does not draw more current than expected.
        // We never return form this, but we will be able to check the results in FRAM next time we are powered up.

        __delay_cycles( 500000 );       // Delay 500ms to let the voltage recover after the flash pulled it down.

        lcd_show_first_start_message();


        power_rundown_test();

        // unreachable

    }

    unlock_persistant_data();
    if (persistent_data.tsl_powerup_count< UINT_MAX ) {     // Stop at 65535. Do not roll over.
        persistent_data.tsl_powerup_count++;                // The body keeps score.
    }
    lock_persistant_data();

    // Wait for any clkout transition so we know we have at least 500ms until next transition so we dont miss any seconds.
    // This should always take <500ms
    // This also proves the RV3032 is running and we are connected on clkout
    unsigned start_val = TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B );
    while (TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B )==start_val); // wait for any transition

    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // We we should not get a real one for 500ms so we have time to do our stuff

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );
    SBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );

    if ( persistent_data.commisisoned_flag != 0x01 ) {


        #ifdef C2_IS_10K            // Only check power usage on the new resistor version



            // First lets check how long we stayed alive after the power was pulled when we were first programmed. This helps to weed out any units that
            // have defects that use too much power.

            // This value represents how many 1/64ths of a second it took for us to go from 3.3V to 1.8V = a drop of 1.5V. This is running off of a 1uF decoupling capacitor.

            unsigned porsoltCount = persistent_data.porsoltCount;


            if ( porsoltCount < 39 ) {  // Empirically determined that all test units with nominal current draw score 40 or above, so this seems like a good starting point.
                                        // 40 represents a a drain of 2.4uA with SVS and all LCD segments on. https://www.google.com/search?q=%281+microfarad%29+%2F+%2840%2F64+second%29+*+%281.5+volt%29++in+microamps

                // If we are drawing more than 2.4uA then something is probably wrong, so reject this unit.

                // Show the operator what the count was
                lcd_show_amps_hi_message(porsoltCount);
                // ...and abort.
                blinkforeverandever();

            }

            if ( porsoltCount > 60 ) {  // The represents a drain of 1.6uA with all LCD segments on. https://www.google.com/search?q=%281+microfarad%29+%2F+%2860%2F64+second%29+*+%281.5+volt%29++in+microamps

                // If we are drawing less than 1.6uA then something is probably wrong, so reject this unit.

                // Show the operator what the count was
                lcd_show_amps_lo_message(porsoltCount);
                // ...and abort.
                blinkforeverandever();

            }

        #endif

        // We just had batteries inserted for the first time ever, so we need to commission ourselves and get ready

        // Set the RTC with the time when we were programmed, which should be about 1 second ago since this is the first time we are powering up from the reset after programming finished.
        // It will have the correct wall clock time until the first battery change in about 150 years. We use the RTC time to copy into `launched_time` when the trigger pin is pulled.
        writeRV3032time(&persistent_data.programmed_time);


        // Make sure that the trigger pin is inserted because
        // we would not want to just launch because the pin was out when batteries were inserted.
        // This also lets us test both trigger positions during commissioning at the factory.

        CBI( TRIGGER_PDIR , TRIGGER_B );      // Input
        SBI( TRIGGER_PREN , TRIGGER_B );      // Enable pull resistor
        SBI( TRIGGER_POUT , TRIGGER_B );      // Pull up

        mode = LOAD_TRIGGER;                  // Go though the state machine to wait for trigger to be loaded
        step =0;                              // Used to slide a dash indicator pointing to the pin location

        // Set us up to run the loading/ready-to-launch sequence on the next tick
        // Note that the trigger ISR will be activated when we get into ready-to-launch
        SET_CLKOUT_VECTOR( &startup_isr);

        // We will go into "load pin" mode when next second ticks. This gives the pull-up a chance to take effect and also avoids any bounce aliasing right at power up.

    } else {

        if ( persistent_data.launched_flag != 0x01 ) {

            // We have already been commissioned, but then we got repowered before we launched.
            // We will treat this as an error condition since either (1) the user pulled the batteries, (2) there is something wrong.
            // Hopefully we will be able to tell if it was (1) with the tamper detect seals.

            lcd_show_batt_errorcode(BATT_ERROR_PRELAUNCH);
            blinkforeverandever();

        }

        // We just powered up after a proper battery change.
        // We have been previously launched.
        // Retrieve the time since launch from persistent memory.

        unsigned retrieved_mins;
        unsigned long retrieved_days;

        // First check if there was an update in progress when we lost power

        if (persistent_data.update_flag) {

            // We somehow managed to power down exactly in the middle of an update. This is extremely unlikely, but if something *can* happen, then we have to be ready for it.

            retrieved_mins = persistent_data.backup_mins;
            retrieved_days = persistent_data.backup_days;

            // roll-back the in-progress update
            unlock_persistant_data();
            persistent_data.mins = retrieved_mins;
            persistent_data.days = retrieved_days;
            persistent_data.update_flag = 0;
            lock_persistant_data();

        } else {

            // No update in progress, so we can directly use the primary (non-backup) values
            retrieved_mins = persistent_data.mins;
            retrieved_days = persistent_data.days;

        }

        // Now we have to normalize the minutes+days because of the very edge case condition where we
        // lost power just after we ticked the last minute of the day but had not yet started to update the day counter.

        if ( retrieved_mins == minutes_per_day ) {
            // Commit the normalization update
            unlock_persistant_data();
            retrieved_days++;
            retrieved_mins=0;
            persistent_data.backup_mins=retrieved_mins;
            persistent_data.backup_days=retrieved_days;
            persistent_data.update_flag = 1;                // Yep, we have to do this - what if we lost power _right here_??
            persistent_data.mins = retrieved_mins;
            persistent_data.days = retrieved_days;
            persistent_data.update_flag = 0;
            lock_persistant_data();
        }

        // Break out the days into digits for more efficient updating while running.
        // We only do this ONCE per set of batteries so no need for efficiency here.
        // Note that there is no `tsl_days` variable, the `days_digits[]` are the canonical storage for the running day count.

        days_digits[0]= (retrieved_days / 1      ) % 10 ;
        days_digits[1]= (retrieved_days / 10     ) % 10 ;
        days_digits[2]= (retrieved_days / 100    ) % 10 ;
        days_digits[3]= (retrieved_days / 1000   ) % 10 ;
        days_digits[4]= (retrieved_days / 10000  ) % 10 ;
        days_digits[5]= (retrieved_days / 100000 ) % 10 ;

        // Show the days since launch on the display

        lcd_show_digit_f(  6 , days_digits[0] );
        lcd_show_digit_f(  7 , days_digits[1] );
        lcd_show_digit_f(  8 , days_digits[2] );
        lcd_show_digit_f(  9 , days_digits[3] );
        lcd_show_digit_f( 10 , days_digits[4] );
        lcd_show_digit_f( 11 , days_digits[5] );

        // Break out the persistent minutes into hours for display.

        tsl_hours = retrieved_mins / minutes_per_hour;

        tsl_mins = retrieved_mins - ( tsl_hours * minutes_per_hour );

        tsl_secs = 0;           // We always fall back to the beginning of the minute. This means we can lose up to 59 secs of count time, but
                                // that should happen less than once per century so it is worth it since we save power not needing to update the persistent counter every second.

        lcd_show_digit_f( 4 , tsl_hours % 10  );
        lcd_show_digit_f( 5 , tsl_hours / 10  );
        lcd_show_digit_f( 2 , tsl_mins  % 10  );
        lcd_show_digit_f( 3 , tsl_mins  / 10  );
        lcd_show_digit_f( 0 , tsl_secs  % 10  );
        lcd_show_digit_f( 1 , tsl_secs  / 10  );

        // Now start ticking at next second tick interrupt
        // The TSL_MODE_BEGIN ISR will initialize the TSL mode counting registers from
        // the `hours`, `mins`, and `secs` globals. Note that the ISR does not know about `days`
        // because it calls back to the C `tsl_next_day()` routine when days increment.
        // We do not set up the trigger ISR since it can never come. We will tick like this
        // forever (or at least until we loose power).

        // Note that TSL_MODE_BEGIN will set up those registers and then go to sleep to wait for the next tick. It should never return.

        // Remember that the TSL_MODE_ISR in tsl.asm is the default ISR on the CLKOUT pin interrupt so we do not have to do anything special.

        // Set us up to run TSL_MODE_BEGIN on the first tick
        // This will set up the registers and then do the first update.
        // It also switches over to directly run TSL_MODE_ISR on the next tick.
        SET_CLKOUT_VECTOR( &TSL_MODE_BEGIN );
    }

    // Activate the RAM-based ISR vector table (rather than the default FRAM based one).
    // We use the RAM-based one so that we do not have to unlock FRAM every time we want to
    // update the CLKOUT ISR entry. This RAM vector was appropriately set in the code above by the time we get here.

    ACTIVATE_RAM_ISRS();

    // Wait for interrupt to fire at next clkout low-to-high change to drive us into the state machine (in either "pin loading" or "time since launch" mode)
    // Could also enable the trigger pin change ISR if we are in RTL mode.
    // Note if we use LPM3_bits then we burn 18uA versus <2uA if we use LPM4_bits.
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger

    // We should never ever get here

    // Disable all interrupts since any interrupt will wake us from LPMx.5 and execute a reset, even if interrupts are disabled.
    // Since we never expect to get here, be safe and disable everything.

    CBI( RV3032_CLKOUT_PIE , RV3032_CLKOUT_B );     // Disable interrupt.
    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );    // Clear any pending interrupt.

    CBI( TRIGGER_PIE , TRIGGER_B );                 // Disable interrupt
    CBI( TRIGGER_PIFG , TRIGGER_B );                // Clear any pending interrupt.

    error_mode( ERROR_MAIN_RETURN );                    // This would be very weird if we ever saw it.

}


