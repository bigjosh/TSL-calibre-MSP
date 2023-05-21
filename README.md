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

### The leap year problem

The RX8900 counts any year ending in `00` as a leap year, but in real life 2100 is not a leap year.

This means that if a unit is programmed in the 2000's and triggered on 3/1/2100 then the trigger date in EEPROM will be 2/29/2100, and any day after that will be 1 day behind the actual calendar date when the trigger was pulled. The count will still always be right, and the only way you'd know about this is if you inspect the trigger time with the diagnostic mode or if you have to reset the RTC.

This divergence will increase by 1 for each non-leap year ending in `00` after 2000, including 2200 and 2300 (2400 is a leap). 

Why don't we just correct for this in the firmware? Well because as far as the RX8900 is concerned 2/29/2100 actually happened, so a trigger could happen on that day.

Since this problem is predicable we can account for it when resetting the RTC in units triggered after 2/28/2100. 

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

## Interesting twists

We are using the MSP430's Very Low Power Oscilator (VLO) to drive the LCD since it is actually lower power than the 32Khz XTAL. It is also slower, so more power savings. 

We do NOT use the MSP430's "LPMx.5" extra low power modes since they end up using more power than the "LPM4" mode that we are using. This is becuase it takes 250us to wake from the "x.5" modes and durring this time, the MCU pulls about 200uA. Since we wake 2 times per second, this is just not worth it. If we only woke every, say, 15 seconds then we could likely save ~0.3uA by using the "x.5" modes. 

We use the RTC's CLKOUT push-pull signal directly into an MSP430 io pin. This would be a problem durring battery changes since the RTC would try to power  
the MSP430 though the protection diodes durring battery changes. We depend on the fact that the RTC will go into backup mode durring battery changes, which will
float the CLKOUT pin. 

Using CLKOUT also means that we get 2 interrupts each second (one on rising, one falling edge). We quickly ignore ever other one in the ISR. It would seem more power efficient to use the periodic timer function of the RTC to generate a 0.5Hz output, but enabling the periodic timer uses an additional 0.2uA (undocumented!), so not worth it. 

To make LCD updates as power efficient as possible, we precomute the LCDMEM values for every second and minute update and store them in tables. Because we were careful to put all the segments making up both seconds digits into a single word of memory (minutes also), we can do a full update with a single 16 bit write. We further optimize but keeping the pointer to the next table lookup in a register and using the MSP430's post-decrement addressing mode to also increment the pointer for free (zero cycles). This lets us execute a full update on non-rollover seconds in only 4 instructions (not counting ISR overhead). This code is here...
[CCS%20Project/tsl_asm.asm#L91](CCS%20Project/tsl_asm.asm#L91)

## Backup mode

The RTC is set up to enter backup mode when it sees the voltage form the batteries drop lower than the voltage form its internal backup capacitors. This should only happen durring a battery change. Note that this can cuase unexpected behaivor if you do a battery change and replace the existing batteries with ones that have a lower total voltage. In this case the RTC will not come out of backup mode when the new batteries are inserted, andso it will not appear to be working to the MSP$#) when it boots up and will generate an "Error Code 1" (ERROR_BAD_CLOCK). If this happens, pull the batteries out for about 30 seconds to let the backup capacitor voltage run down so it will then be less than the batteries. Now reinsert the batteries and the RTC show now see a battery voltage higher than the capacitor voltage and wake up and operate normally. 

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
