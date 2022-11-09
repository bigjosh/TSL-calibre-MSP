# TSL

PCB and firmware for the CW&T Time Since Launch project.

## Design goals:

* Run for decades on 2xAA batteries.
* Stay accurate to to within ±2ppm over the lifetime.  
* Replaceable batteries, with continued timekeeping during a reasonable battery swap time period.  

## Critical parts:

* MSP430FR4133 processor for LCD driving and supervision
* RV3032-C7 RTC for precision timekeeping
* Custom 12-digit, dynamic LCD glass 
* 2x Energizer Ultimate Lithium AA batteries
* Optional TPS7A0230 3V regulator for generating LCD bias voltage

## EEPROM usage

We use a block at the start of EEPROM to ...

1. Initially set the current real time on first power up at the factory.
2. Remember that we set the current time at the factory. 
3. Remember if/when the trigger was pulled.
4. Remember if the RTC has ever lost power. 
 
Here is the format of that block...

    #define EEPROM_ADDRESS_STARTIME     EEPROM_ADDRESS( 0)  // Set by the at the Factory to real time GMT when initially programmed. RX8900 register block layout. Values are BCD.
    #define EEPROM_ADDRESS_STARTFLAG    EEPROM_ADDRESS( 8)  // Set 0x00 to indicate that STARTTIME block has time in it, set to 0x01 when STARTTIME set to the RTC the first time we power up

    #define EEPROM_ADDRESS_TRIGGERTIME  EEPROM_ADDRESS(10)  // Set to RTC time when the trigger pin is pulled. RX8900 register block layout. Values are BCD.
    #define EEPROM_ADDRESS_TRIGGERFLAG  EEPROM_ADDRESS(18)  // Set to 0x01 when the trigger pin is pulled and the RTC time is aves to the TRIGGER_TIME block.


	#define EEPROM_ADDRESS_LOWVOLTFLAG  EEPROM_ADDRESS(20)  // Set to 0x01 if we ever power up and find a low voltage condition.



The times are in RX8900 register block format....

| Address | Function |
| - | - |
| 00 |  SEC | 
|01  | MIN | 
|02  | HOUR |
|03  | WEEK |
|04  | DAY |
|05  | MONTH | 
|06  | YEAR |
|07  | RAM |

We ignore the `WEEK` register, but set it to a sensical value (1-7) since the datasheet warns of problems otherwise. 

We use the `RAM` location to hold our century interlock. 

### Century interlock

The RX8900 only saves 2 digits for the year, so when we roll from 2099 to 2100, the year will go from 99 to 0.
This would make us loose 100 years worth of days on the first battery change of the new century when we go to
compute how long since the pin was pulled.

To buy ourselves extra centuries, we use a century interlock stored in the RAM register of the RX8900.
When we first program a unit (presumably in the 1st half of the 2000 century), we set the interlock to '00' BCD (0x00).

On every power up (typically due to battery change) every 10 - 30 years(? We'll see!), we check the current year. If it is greater than 50 and the century interlock is even, then we increment the century interlock. If it is less than 50  and the century interlock is odd, then we increment the century interlock. This also has the effect of re-writing the EEPROM every few decades, which [may or may not extend the retention](https://electronics.stackexchange.com/questions/411616/for-maximum-eeprom-readability-into-the-future-is-it-better-to-write-once-and-le). 


When we want to compute the total days elapsed since epoch Jan 1, 2000 then we divide the century interlock by 2 and drop the remainder. We when multiply this by the number of days in an RX8900 century and we will get the correct value.

This will get us an additional 127 centuries of run time, which puts us at the year 14700. We will need to patch the firmware before then if we want to continue to keep accurate count.

### The leap year problem

The RX8900 counts any year ending in `00` as a leap year, but in real life 2100 is not a leap year.

This means that if a unit is programmed in the 2000's and triggered on 3/1/2100 then the trigger date in EEPROM will be 2/29/2100, and any day after that will be 1 day behind the actual calendar date when the trigger was pulled. The count will still always be right, and the only way you'd know about this is if you inspect the trigger time with the diagnostic mode or if you have to reset the RTC.

This divergence will increase by 1 for each non-leap year ending in `00` after 2000, including 2200 and 2300 (2400 is a leap). 

Why don't we just correct for this in the firmware? Well because as far as the RX8900 is concerned 2/29/2100 actually happened, so a trigger could happen on that day.

Since this problem is predicable we can account for it when resetting the RTC in units triggered after 2/28/2100. 

## Method of Operation


### Factory power up
The factory burns the firmware into flash and also stores the current GMT time into `STARTTIME` in the EEPROM block. 

On initial power up, firmware sees a new `STARTTIME` block in EEPROM and uses it to set the RTC. It then sets the `STARTFLAG` so that the time is not programmed again on subsequent power ups and executes a software reset to being normal operation with the new start time.  

If the trigger pin is out at start up we go into clock mode and show the current GMT time until the pin is inserted. THIS CLOCKMODE IS MEANT FOR TESTING ONLY. It is very high power (like 50x normal count!), so be sure to insert the pin as soon as you have verified the correct time. DO NOT USE A TSL IN CLOCKMODE AS A CLOCK. If you really want a TSL clock, we can make you power efficient firmware for that. 

Once the pin is in, we go into Ready To Launch mode. 
 
## Display modes

### Warm up mode

Every time the MCU powers up (on initial programming or after battery change), it will display a pretty sinewave pattern for 2 seconds while it waits for the RX8900 RTC to oscillator to stabilize. 

Do we need this? Well we certainly do not need it if the RTC is already running, but it is unclear if we can poll the RTC to see if it is running before the oscillator has settled. The datasheets are unclear. 
> 
> "Please perform initial setting only tSTA (oscillation start time), 
> when the built-in oscillation is stable."
> 

...so we wait. 

### Reset flags mode

For testing only - removed from production firmware.

Displayed for 2 seconds after each reset. Shows the reset flags currently set. We clear the flags after testing them, so there should only be one flag set indicating the reason for the most recent reset. 

Shows the word `rESEt` on the left LCD and the following possible flags on the right LCD...

| Letter | Name | Description |
| - | - | - | 
| P | Power On | Initial power up |
| E | External | The RESET pin was pulled low<br>(We disable this pin)  |
| b | Brown out | Brown out voltage reached<br>(We disable the brown out detector) |
| d | Download | PDI download triggered reset |
| S | Software | Software generated reset<br>(We trigger this after setting the time) |
| U | Undefined | One of the two undefined flags were set |  

### Set Clock mode

Shows "SEt CLoC" on the LCD.  

Shown when we initially set the start time from EEPROM into the RTC during factory programming. 

After this we execute a software reset.  

### Clock mode

Shows current real time as MMDDYY HHMMSS with blinking ":"'s.

This time comes from the RTC and should be correct GMT ±2ppm if it was programmed correctly at the factory. 

This mode is shown at start up if we have a good start time, but no trigger time and the pin is not inserted. 

We existing this mode when the pin is inserted and goto Ready To Launch mode.     

WARING: Clock mode uses about 50x as much power as normal Time Since Launch mode, so don't leave it on for too long! For diagnostics only! 

### Ready To Launch mode 

Shows a mesmerizing winding pattern with 1Hz update rate. 

The purpose of this pattern is to show the user that we are ready and willing, and also make apparent any bad LCD segments stuck either on or off. 

We enter this mode on power up if the trigger pin has never been pulled before, and we exit it when the pin is pulled. When the pin is finally pulled, we blink the flashbulb LEDs and save the current time from the RTC into EPPROM in the `TRIGGERTIME` block to remember the trigger time and set the `TRIGGERFLAG`.  

### Time Since Launch Mode

Here we count up the days, hours, minutes, and seconds since the trigger was pulled. 

When we reach 1,000,000 days we switch to Long Now mode. 

### Where Has the Time Gone mode

Shows the time the trigger pin was pulled in MMDDYY HHMMSS blinking at 0.5Hz. 

Indicates that the trigger pin was pulled and we marked the moment shown on the display, but since then the real time was lost (RTC lost power) and needs to be set before we can show Time Since Launch mode. 

The unit can be factory serviced and set with the correct real time and it will pick up where it left off. 

### Clock Error mode

Shows `cLoc Error` blinking forever.

Indicates that the trigger pin has never been pulled, and that the real time was lost. The start time must be set again before we can go to Ready To Launch mode.    

This likely means that the unit was stored as old new stock for 100+ years and the batteries we allowed to go completely dead before first use. The unit can be factory serviced to set the real time and then will be ready for first trigger.   

### Low Battery detection

We can run down to 1.8V but the display starts to become hard to read at about 2.5V, so depend on the user noticing that the display is getting dimmer to knwo it is time for a battery change, which is not predicted to happen for at least a century. Once the screen goes completely blank from all but extreem angles, the user still will have several decades to change the batteries before the time is lost. 

Since the RTC is only powered by the backup capacitors while the batteries are out, you should work quickly. Target time is 60 seconds hold over, but TBD. 

### Long Now mode

Shows `999999 235959` blinking forevermore. 

Indicates that the trigger was more than 1 million days (~2740 years) ago so we can not display it accurately. 

The idea here is to avoid problems of people trying to ebay old TSL units that have rolled over by misrepresenting their true age. 

### Bad Interrupt Mode

Shows "bAd Int" blinking forever.

This means that an unexpected interrupt happened. This would be very unexpected indeed.  

The only interrupts that are ever enabled are for the trigger pin and the 2Hz FOUT tick (risgin and falled edge of 1Hz) coming from the RTC, and we turn off the trigger pin after the pin is pulled.

### EEPROM Error Mode

Shows `EEPro ErrorX` blinking forever.

This is shown on start-up if the EEPROM is in an inconsistent state. This usually means either that the EEPROM was never programmed (it defaults to 0xff's), or it got corrupted somehow. 

The `X` after `EEPro` is a code that tells you the first problem found (they are checked in order). 

| Code | Reason 
| - | - |
| 1 | Invalid `LOW VOLTAGE FLAG` |
| 2 | Invalid `START FLAG` |
| 3 | Invalid `TRIGGER FLAG` |
| 4 | `TRIGGER FLAG` set but `START FLAG` not set |
| 5 | `TRIGGER TIME` is in the future | 
| 6 | _[elided]_ |
| 7 | Invalid `START TIME` | 
| 8 | Invalid `TRIGGER TIME` | 

#### Codes 1-3

All flags must have a value of either 0 (not set) or 1 (set).

#### Code 4

How could the user have pulled the trigger if we never set the start time? Impossible. 

#### Code 5

To find the Time Since Launch, we need to subtract the time now from the `TRIGGERTIME`, so the `TRIGGERTIME` must be *before* now. 

#### Code 6

[elided] 

#### Code 7 

`STARTTIME` failed validity checks on start up. (i.e. month was greater than 12)

#### Code 8 

`TRIGGERTIME` failed validity checks on start up. (i.e. month was greater than 12)

## `STARTTIME` delay offset

When programming the `STARTTIME` into the EEPROM, be sure to account for (1) the time it takes to generate the EEPROM file and program it into the unit, and (2) the 1 second warm up delay when the unit first comes up. 

## Diagnostics

### Factory program

On the initial power-up after the time as been set to the value supplied in the `STARTTIME` EEPROM block, the display will show the Clock mode for as long as the trigger pin is out. 

This is handy for verifying the correct time was programmed. 

### Field service 

There are two test pins on the 6-pin ISP connector on the board labeled `B` and `T`. They are internally pulled up, so you connect them the to the ground pin (labeled `G`) to activate them.

The reset pins are checked at start up and at the end of each hour. 

#### `B` Pin

Grounding the `B` pin will show a series of three display phases, switching once per second.

The sequence will repeat if pin `B` is still grounded after phase 3 is displayed.  

##### Phase 1

The LCD displays the `STARTTIME` block from EEPROM. 

Left `:` is on to differentiate this display. 


##### Phase 2

Will show the stored trigger time. The left & right `:`'s are lit to differentiate this display.

The display will show "no triG" if the EEPROM trigger flag is not set.   


The right `:` is lit to differentiate this display.

(Note this changed in ver 1.04)


##### Phase 3

The left LCD module shows `XXVOLt` where `XX` is the current Vcc voltage as read by the XMEGA analog to digital converter. There is an implied decimal point between the two digits, so `31VOLT` means the XMEGA just sampled the Vcc as 3.1 volts. Note that this voltage can be slightly lower than what it will be when running normally because this diagnostics screen uses much more current than the normal modes do.    

The right LCD module shows `VErXXX` where `XXX` is the current firmware version as defined in the TSL.c source code. There is an implied decimal point between the first and second digits so `VER101` indicates firmware version 1.01.    
   

## Interesting twists

We are using the MSP430's Very Low Power Oscilator (VLO) to drive the LCD since it is actually lower power than the 32Khz XTAL. It is also slower, so more power savings. 

We do NOT use the MSP430's "LPMx.5" extra low power modes since they end up using more power than the "LPM4" mode that we are using. This is becuase it takes 250us to wake from the "x.5" modes and durring this time, the MCU pulls about 200uA. Since we wake 2 times per second, this is just not worth it. If we only woke every, say, 15 seconds then we could likely save ~0.3uA by using the "x.5" modes. 

We are using the "peridoc timer" mode of the RV3032 to generate a 122us pulse on the INT pin. The MCU pulls this pin high with a ~50K OHM resistor so we are burning power durring the low pulse, but it is very little - [worst case is 0.0427uA](https://frinklang.org/fsp/frink.fsp?fromVal=%28%283.5V%29+%2F+%2820000+ohm%29%29+*+%28+%28122+micro+s%29%2F%28500+milli+s%29%29&toVal=micro+amp#calc) assuming Vcc=3.5V and the internal pull-up is only 20K ohm. 

We have to run the periodic timer at 2Hz rather than 1Hz becuase the width of the pulse is dependant on the period and at 1000ms period, the pulse width increases to 8ms which would burn more power than waking and ignoring every other tick. 

Using the CLKOUT push-pull signal directly would use less power than using the INT signal against the pull-up, but if we used CLKOUT then the RV3032 would try to power 
the MSP430 though the protection diodes durring battery changes.The SCL pin is input and SDA pins isopen collector to the RV3032, and they are also disabled when Vcc drops below the voltage on the backup capacitors, so we do not have to worry about them durring battery change. We tie the EVT pin to GND to avoid having it float durring battery changes. 

## Build notes

## Current Usage

THESE ARE PRELIM

| Mode | Vcc=3.55V| Vcc=2.6V |
| - | -: | -: | 
| Ready to Launch | 1.8uA | 1.8uA | 
| Time Since Launch | 2.3uA | 2.3uA |
| Long Now | TBD | TBD | 
| quiescent | TBD | TBD |

Quiescent mode means that all LCD segments are turned off, interrupts are disabled, and the MCU is sleeping.

3.55V is approximately the voltage of a pair of fresh Energizer Ultra batteries.
2.6V is approximately the voltage when the screen starts to become hard to read. 

Note that voltage drop over time is not expected to be linear with Energizer Ultra cells. These batteries are predicted to spend most of their lives towards the higher end of the voltage range and only start dropping when they get near to their end of life.  

### Color

Comparing Time Since Launch mode to Long Now mode lets us see how much power is used in the the timekeeping code.  

Comparing Low Battery mode to Long Now mode lets us see how much of the power is dependent on just how many LCD segments are lit.

Comparing maximum and minimum operating voltages for different modes lets us see how much of the power usage is dependent on supply voltage, and how that relates to time spent in sleep versus active mode (Time Since Launch spend a lot of time in active, whereas Long Now and Low Battery spend almost none).

Comparing Low Battery mode to quiescent mode lets us see how much power used by the interrupt ISRs (confounded slightly by those tiny little battery icon segments and the single blink instructions in Low Battery mode).    

### Conclusions

There is not much to be gained by elimination the ISR overhead. This is surprising. Those unnecessary PUSHes and POPs still bug the crap out of me. 

There are gains of up to 1uA possible from having fewer LCD segments lit. Not sure how actionable this is. We could, say, save 0.5uA by blinking the Time Since Launch mode screen off every other second. It is likely that Ready To Launch mode's low power relative to Time Since Launch mode is due to the fact that it has only 1 segment lit per digit. 

Probably the best place to focus effort is on the Time Since Launch update code since there is about 1uA on the table and the device spends the vast majority of its live here.  
  

### Measurement conditions


Production board from initial batch. Bare PCB on my desk (not in tube).  
68F
80%RH

(Higher humidity and temperature tends to increase current consumption. Need some different weather conditions to quantify this!) 

## Status

Firmware is Feature complete but not optimized. I hope to at least recode the seconds update code into a state machine with only a couple of instructions per tick.  
