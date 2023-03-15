/*
 * acid_fram_record.hpp
 *
 *  Created on: Mar 15, 2023
 *      Author: passp
 */

#ifndef ACID_FRAM_RECORD_HPP_
#define ACID_FRAM_RECORD_HPP_


// This abstraction gives us ACID (Atomicity, Consistency, Isolation, Durability) access to a varaible.
// Using this, we can detect and correct an partial write or corruption to a single value of type unsigned (since this is a 16 bit MCU).

template <typename T>
class acid_FRAM_record_t {
    unsigned begin_counter=0;
    T protected_data;
    unsigned completed_counter=0;
    T backup_data;

public:


    // Writes data atomically to stored data
    void writeData( T *data ) {

        begin_counter++;
        *protected_data=*data;
        completed_counter=begin_counter;

        // Note that by the time we get here, we know that this write is durable,
        // so this backup write is only for the next pass though. If we fail during
        // this write, no problem since the read of the protected data will succeed.
        *backup_data = *data;

    }


    // Fills in passed struct from stored data
    void readData( T *data ) {

        // Did most recent write complete successfully?
        if (begin_counter!=completed_counter) {

            // Restore from backup copy and reset protected counter.
            // Note that protected data is untouched in case this write here fails.

            begin_counter++;
            *protected_data = *backup_data;
            completed_counter=begin_counter;

        }

        *data = protected_data;

    }


};



#endif /* ACID_FRAM_RECORD_HPP_ */
