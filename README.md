# TSL

PCB and firmware for the CW&T Time Since Launch project.

![image](https://github.com/bigjosh/TSL-calibre-MSP/assets/5520281/20763a34-e9c7-4478-9f9a-e5587940cfa9)


Buy it here...
https://cwandt.com/products/time-since-launch?variant=19682206089275


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

## Method of Operation

### Ready To Launch mode 

Shows a mesmerizing winding pattern with 1Hz update rate. 

The purpose of this pattern is to show the user that we are ready and willing, and also make apparent any bad LCD segments stuck either on or off. 

We enter this mode on power up if the trigger pin has never been pulled before, and we exit it when the pin is pulled. 

### Time Since Launch Mode

Here we count up the days, hours, minutes, and seconds since the trigger was pulled. 

When we reach 1,000,000 days we switch to Long Now mode. 

### Long Now mode

Shows `999999 999999` blinking forevermore. 

Indicates that the trigger pull was more than 1 million days (~2740 years) ago so we can not display it accurately. 

The idea here is to avoid problems of people trying to ebay old TSL units that have rolled over by misrepresenting their true milage.

#### Error Codes 

Descibed here...
[CCS%20Project/error_codes.h](CCS%20Project/error_codes.h)

### Commisioning

To commisison a new unit, we go though these steps...

1. Firmware is programmed and the display shows `FIRST START` message.
2. Power is removed from the unit.
3. When the unit sees voltage is dropping, it starts counting how many 1/64ths of a second it can keep running before it shuts down. It displays all 8's on the LCD durring this time to check for shorts on the LCD lines.
4. Batteries are insterted and the unit powers up.
5. Firmware checks to see how many 1/64ths of a second it survived during the previous power down. If this value is out of bounds (power level too low or too high) then it shows an `AMPS LO` or `AMPS HI` error.
6. The pin is inserted, depressing the trigger lever. This tests that the pin can cycle though both states.
7. Unit is ready to launch! I displays a segment pattern on the LCD that shoud update at 2Hz. This tests both that the RTC is generating interrupts correectly and also that all LCD are good. 

## Interesting twists

We are using the MSP430's Very Low Power Oscilator (VLO) to drive the LCD since it is actually lower power than the 32Khz XTAL. It is also slower, so more power savings. 

We do NOT use the MSP430's "LPMx.5" extra low power modes since they end up using more power than the "LPM4" mode that we are using. This is becuase it takes 250us to wake from the "x.5" modes and durring this time, the MCU pulls about 200uA. Since we wake 2 times per second, this is just not worth it. If we only woke every, say, 15 seconds then we could likely save ~0.3uA by using the "x.5" modes. 

To make LCD updates as power efficient as possible, we precomute the LCDMEM values for every second and minute update and store them in tables. Because we were careful to put all the segments making up both seconds digits into a single word of memory (minutes also), we can do a full update with a single 16 bit write. We further optimize but keeping the pointer to the next table lookup in a register and using the MSP430's post-decrement addressing mode to also increment the pointer for free (zero cycles). This lets us execute a full update on non-rollover seconds in only 4 instructions (not counting ISR overhead). This code is here...
[CCS%20Project/tsl_asm.asm#L91](CCS%20Project/tsl_asm.asm#L91)

## Battery changes

Based on power projections, we do not expect to need a battery change for at least 100 years. When the batteries get near thier end of life, the LCD will start to get dim. At this point, as long as the unit has been triggered, it will simply stop counting durring the time it takes to change the batteries, and will start counting again
from where it left off once the new batteries are installed. To make this possible, we keep counters of both days and minutes elapsed since trigger in FRAM. These counters are updated each minute and day respectively. Do note that this means that every time you remove the batteries, the time since launch will round down to the most recent minute. Also note that it is _possible_ to lose power exactly at the momentthat happens once per day when both the minute and day counters are updates, so there is lock code to make sure this pair of updates is atomic. 

Note that if the batteries are pulled before the unit is triggered then it will go into `BATT_ERROR_PRELAUNCH` mode ("ERROR 1"), so do not wait *too* long before triggering your TSL. :)

## Hardware revisions

The 2nd hardware revision was produced mid-2024 and removes the backup capacitors connected to the RTC and adds a new 10K OHM resistor on one of the pads that these capcitors used. Previously, these capacitors would keep the RTC continuously operating durring batter changes, but there some defects on those capacitors and we decided it would be easier to just have the count pause durring battery changes. To support this change, there is a change to the firmware that is not compatible with the old versions of the hardware. This change disables the "backup power" option on the RTC. To compile the firmware to work with this new version of the hardware, you must include `#define C2_IS_10K` in the top of `tsl-calibre-msp.cpp`. 

## Build notes

## Current Usage

| Mode | Vcc=3.55V| Vcc=2.6V |
| - | -: | -: | 
| Ready to Launch Running | 1.3uA | 1.2uA | 
| Ready to Launch Static | 1.1uA | 1.0uA | 
| Time Since Launch | 1.8uA | 1.7uA |

3.55V is approximately the voltage of a pair of fresh Energizer Ultra batteries.
2.6V is approximately the voltage when the screen starts to become hard to read. 

Note that voltage drop over time is not expected to be linear with Energizer Ultra cells. These batteries are predicted to spend most of their lives towards the higher end of the voltage range and only start dropping when they get near to their end of life.  

There are gains of up to 5uA possible from having fewer LCD segments lit. Not sure how actionable this is. We could, say, save 0.5uA by blinking the Time Since Launch mode screen off every other second. It is likely that Ready To Launch mode's low power relative to Time Since Launch mode is due to the fact that it has only 1 segment lit per digit. 

### Measurement conditions

Production board from initial batch. Bare PCB on my desk (not in tube).  
68F
80%RH

(Higher humidity and temperature tends to increase current consumption. Need some different weather conditions to quantify this!) 
