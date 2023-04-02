### Time Flow in the TSL

Naively, we should just be able to program the RTC to give us one tick per second and then just count those seconds and be done. Unfortunately, we need to be able to change the batteries, and while the batteries are out the MPU is not running so it misses counts. This leads to much complication. Here is how we solve it.

## Pre-launch

We want to be able to tell when a unit launched so we can restart it just in case there is some failure.

To do this, when we commission the TSL in the factory, we write a block to FRAM with the timedate that the RTC was first powered up. Now by adding the accumulated time to the stored time, can can determine the real world timedate. Note that we also store this commission timedate in a database , so the one in FRAM is just a backup in case that database is lost over the eons. Also note that this recorded commissioning time is never used in the operation of the unit - it is only there for forensic inspection in the case of a failure.  

There is a complication- the RTC does not store centuries so if a user waits 101 years before pulling the trigger then we can not tell if this was 1 year or 101 years by looking only at the data. We resolve this for TSLs that have had the triggered pulled (the only case where this matters) by looking to the time since launch to disambiguate how many centuries have past since manufacturing and then allocate them between pre- and post- launch periods. 

# Ready to Launch mode

When the trigger is pulled, we read the current timedate from the RTC (which represents the time since commissioning modulo 100 years) and write it to an FRAM block as the "launch" time. Note that this is only here for forensic purposes and is never used by the unit. We then reset the RTC to 1/1/00 00:00:00 and let it start counting. This way the time in the RTC represents the amount of time since launch. After that, the unit starts counting the ticks and updating the display. Note that we do not ever use the timeday being maintained inside the RTC during operation - the MCU keeps its own timedate based on counting ticks.

The MCU keeps the seconds, minutes, and hours in registers for efficiency. It keeps the day count as an unsigned long in FRAM.  


# Battery change event

During a battery change, the MCU is powered down but the RTC continues timekeeping using a backup capacitor that can keep it running for at least a minute.

So when we power back up again after a battery change, the seconds, minutes, and hours registers are lost - but we can read these from the RTC. 

We do still know the day count because that was stored in FRAM, but there is a complication. What if the battery is pulled at 23:59:59 and then reinserted at 00:00:30? Now we will have the correct seconds, minutes, and hours but will will have missed a day. 

To resolve this, we keep an extra flag in FRAM called `bottom_of_day_flag`. We set this flag when hours goes from 11 to 12 and clear it when we count a new day (hours goes from 23 to 0).




In Ready To Launch mode  

      

 