/*
 * error_codes.h
 *
 *  Created on: Jan 20, 2023
 *      Author: passp
 */

#ifndef ERROR_CODES_H_
#define ERROR_CODES_H_


// We got bad data from the RV3032 checking the status reg on startup. Either defective chip or PCB connection?
// Note that you can get this in the case where you pull batteries and replace them with ones that had *lower* voltage
// since then the RTC will still be in backup mode. If this happens, you can just wait a few minutes for the backup
// voltage to drop and then pull the batteries again and reinsert them and it should restart and be OK.
#define ERROR_BAD_CLOCK       1

// The main() function continued running after the line that puts the processor to sleep which should never happen.
#define ERROR_MAIN_RETURN     9

// We make the battery error codes non-overlapping with other error codes to avoid any confusion.


// RTC was reset due to lack of power. Happened before unit was launched.
#define BATT_ERROR_PRELAUNCH  2

// RTC was reset due to lack of power. Happened after unit was launched.
#define BATT_ERROR_POSTLAUNCH   3




#endif /* ERROR_CODES_H_ */
