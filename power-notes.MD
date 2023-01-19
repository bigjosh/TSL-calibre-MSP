# Power Notes

All measurements taken with display initialized to "bA9876543210".

Vcc= 3.51V.



2.12uA Count switching between 00 and 01 at 2hz. 
2.00uA with ~INT unplugged so no interrupts or display updates, display at 01.

0.39uA just RTC with 2.hz ~INT and direct switching mode.


 

2.05uA with RTC connected
1.67uA without RTC connected

### RTC Backup Modes Power Usage
1.23uA RTC at default power up with clkout 32768
0.17uA RTC with clkout disabled, backup disabled
0.18uA RTC with direct switchover mode enabled

-- So direct switchover mode is very power cheap, so probably not worth adding a diode to save 0.01uA.

#### RTC Periodic Timer Power Usage

0.40uA RTC with 2Hz periodic timer off the 4096Hz base
0.34uA RTC with 1/64Hz periodic timer off the 64Hz base.
0.34uA RTC with 1/4096Hz periodic time off the 1Hz base.

--So the periodic timer uses ~0.16uA even on low base frequencies. Wish I had know that before!

0.18uA RTC with 1Hz square wave on clkout

--So clkout is the efficient way to go. :/




 



 
 
