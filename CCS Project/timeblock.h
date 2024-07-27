/*
 * timeblock.h
 *
 *  Created on: Jul 18, 2024
 *      Author: passp
 */

#ifndef TIMEBLOCK_H_
#define TIMEBLOCK_H_


// The of time registers we read from the RTC in a single block using one i2c transaction.
// This is dependant on the foramt the time is stored inside the RV3032 registers.
// The python program that generates the files to program a unit depends on this format so it can save the current wall clock time.

struct rv3032_time_block_t {
    unsigned char sec_bcd;
    unsigned char min_bcd;
    unsigned char hour_bcd;
    unsigned char weekday_bcd;
    unsigned char date_bcd;
    unsigned char month_bcd;
    unsigned char year_bcd;
};


#endif /* TIMEBLOCK_H_ */
