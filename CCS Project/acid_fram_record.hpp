/*
 * acid_fram_record.hpp
 *
 *  Created on: Mar 15, 2023
 *      Author: josh
 */

#ifndef ACID_FRAM_RECORD_HPP_
#define ACID_FRAM_RECORD_HPP_

#include <stdio.h>

// This abstraction gives us ACID (Atomicity, Consistency, Isolation, Durability) access to a variable.
// Using this, we can detect and correct any partial write or corruption to a single value of type unsigned (since this is a 16 bit MCU).

template <typename T>
class acid_FRAM_record_t {


    volatile unsigned update_in_progress_flag=0;
    volatile T protected_data;
    volatile T backup_data;


    // It looks like we have to do this ugliness because of the way the standard choose to implement the copy constructor...
    // https://stackoverflow.com/a/17218983/3152071
    void volatileCopy( volatile T *dest, volatile const T *src ) {

        memcpy( const_cast<T *>( dest ) , const_cast<T *>( src ), sizeof(T) );

    }

public:

    // Writes data atomically to stored data
    void writeData( T const * const data ) {

        update_in_progress_flag=1;
        volatileCopy(&protected_data, data);
        volatileCopy( &protected_data , data);
        update_in_progress_flag=0;

        // Note that by the time we get here, we know that this write is durable,
        // so this backup write is only for the next pass though. If we fail during
        // this write, no problem since the read of the protected data will succeed.
        volatileCopy( &backup_data , data);

    }

    // Fills in passed struct from stored data
    void readData( T *data ) {

        // Did most recent write fail before completion?
        if (update_in_progress_flag!=0) {

            // Restore from backup copy and reset protected flag.
            // Note that protected data is untouched in case this write here fails.

            volatileCopy(&protected_data , &backup_data);
            update_in_progress_flag=0;

        }

        volatileCopy( data , &protected_data);

    }


};

/*
// These two values must be atomically updated so that we can reliably count centuries even in the face
// of a potential battery change that straddles the century roll over.

struct century_counter_t {
    unsigned postmidcentury_flag;       // Set to 1 if we have past the 50 year mark in current century.
    unsigned century_count;             // How many centuries have we been in TSL mode?
};

// Here is the persistent data that we store in "information memory" FRAM that survives power cycles
// and reprogramming.

struct __attribute__((__packed__)) persistent_data_t {

    // Archival use only. Never read by the unit.
    rv3032_time_block_t commisioned_time;       // Calendar time when this unit was commissioned. Set during programming, never read.
    rv3032_time_block_t launched_time;          // Time when this unit was launched relative to commissioned. Set when trigger pulled, never read.

    // State. We have 3 persistent states, (1) first startup fresh from programming, (2) ready to launch, (3) launched.
    unsigned once_flag;                             // Set to 1 first time we boot up after programming
    unsigned launch_flag;                           // Set to 1 when the trigger pin is pulled

    // Used to count centuries since the RTC does not.
    // Note this is complicated because there is no way to atomically write to the FRAM, and in fact any one write
    // could be corrupted if power fails just at the right moment. But since at most one write can fail, we can use
    // a two-step commit with a fall back.

    acid_FRAM_record_t<century_counter_t> acid_century_counter;

};

// Tell compiler/linker to put this in "info memory" at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.
// We have to mark `volatile` so that the compiler will really go to the FRAM every time rather than optimizing out some accesses.
volatile persistent_data_t __attribute__(( __section__(".infoA") )) persistent_data;


void test() {
    century_counter_t century_counter;

    persistent_data.acid_century_counter.readData( &century_counter );

}

*/

#endif /* ACID_FRAM_RECORD_HPP_ */
