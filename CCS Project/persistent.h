/*
 * persistent.h
 *
 * This is the structure for the data that we store in the "InfoA" section of flash memory. This data persists through power cycles.
 * We break it out into a header file solely so that the ASM code can include it to find the address of `mins`, which the ISR needs to increment once per minute.
 *
 *  Created on: Jul 18, 2024
 *      Author: passp
 */

#ifndef PERSISTENT_H_
#define PERSISTENT_H_

#include "timeblock.h"

// Here is the persistent data that we store in "information memory" FRAM that survives power cycles
// and reprogramming.

struct __attribute__((__packed__)) persistent_data_t {

    rv3032_time_block_t programmed_time;       // Calendar time when this unit was Programmed.Used to initialize the RTC on first power up.
    rv3032_time_block_t launched_time;          // Time when this unit was launched relative to programmed time. Set when trigger pulled, never read.


    // State. We have 3 persistent states, (1) first startup fresh from factory programming, (2) ready to launch, (3) launched.
    // We make these volatile to ensure that the compiler actually writes changes to memory rather than trying to cache in a register

    volatile unsigned initalized_flag;                         // Set to 1 after first time we boot up after programming. At this step, we check for excess current draw when we power down.
    volatile unsigned commisisoned_flag;                       // Set to 1 after we commission, which involves inserting the batteries and trigger pin.
    volatile unsigned launched_flag;                           // Set to 1 when the trigger pin is pulled.

    volatile unsigned porsoltCount;                            // How many 0.1 seconds did the unit stay alive after power was removed during initialization?

    volatile unsigned tsl_powerup_count;                           // How many times have we booted up since we initialized (Max 65535. Should be 1 until first battery change in about 150 years.)

    // Time since launch
    volatile unsigned mins;             // It would be too much work to update this every second, so once a minute is good. Note that it is possible for this value to end up at `minutes_per_day`, in which case you must normalize it and increment days.
    volatile unsigned long days;        // Has to be long since 2^16 days is only ~180 years and we plan on working for much longer than that.

    // We need to be able to update the above values atomically so we use an interlock here so we can do the right thing in case we get interrupted exactly in the middle of an update.
    volatile unsigned update_flag;             // Set to 1 when an update is in progress (backup values are valid)
    volatile unsigned backup_mins;             // These backup values are captured before we set `update_flag`.
    volatile unsigned long backup_days;

};

// Tell compiler/linker to put this in "info memory" that we set up in the linker file to live at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.

extern persistent_data_t persistent_data;


#endif /* PERSISTENT_H_ */
