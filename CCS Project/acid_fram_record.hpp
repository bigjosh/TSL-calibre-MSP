/*
 * acid_fram_record.hpp
 *
 *  Created on: Mar 15, 2023
 *      Author: josh
 */

#ifndef ACID_FRAM_RECORD_HPP_
#define ACID_FRAM_RECORD_HPP_

// This abstraction gives us ACID (Atomicity, Consistency, Isolation, Durability) access to a variable.
// Using this, we can detect and correct an partial write or corruption to a single value of type unsigned (since this is a 16 bit MCU).

template <typename T>
class acid_FRAM_record_t {
    unsigned update_in_progress_flag=0;
    T protected_data;
    T backup_data;

public:


    // Writes data atomically to stored data
    void writeData( T *data ) {

        update_in_progress_flag=1;
        *protected_data=*data;
        update_in_progress_flag=0;

        // Note that by the time we get here, we know that this write is durable,
        // so this backup write is only for the next pass though. If we fail during
        // this write, no problem since the read of the protected data will succeed.
        *backup_data = *data;

    }


    // Fills in passed struct from stored data
    void readData( T *data ) {

        // Did most recent write fail before completion?
        if (update_in_progress_flag!=0) {

            // Restore from backup copy and reset protected counter.
            // Note that protected data is untouched in case this write here fails.

            *protected_data = *backup_data;
            update_in_progress_flag=0;

        }

        *data = protected_data;

    }


};



#endif /* ACID_FRAM_RECORD_HPP_ */
