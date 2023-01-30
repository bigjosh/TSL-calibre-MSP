/*
 * error_codes.h
 *
 *  Created on: Jan 20, 2023
 *      Author: passp
 */

#ifndef ERROR_CODES_H_
#define ERROR_CODES_H_


// The main() function continued running after the line that puts the processor to sleep which should never happen.
#define ERROR_MAIN_RETURN     9

// We make the battery error codes non-overlapping with other error codes to avoid any confusion.

// RTC was reset due to lack of power. Happened before unit was launched.
#define BATT_ERROR_PRELAUNCH  2

// RTC was reset due to lack of power. Happened after unit was launched.
#define BATT_ERROR_POSTLAUNCH   3


#endif /* ERROR_CODES_H_ */
