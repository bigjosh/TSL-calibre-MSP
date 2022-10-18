/*
 * pins.h
 *
 * Defines all of the pins we are using
 *
 *  Created on: Oct 15, 2022
 *      Author: passp
 */

#ifndef PINS_H_
#define PINS_H_

// Clock in comes from the RV3203 and is a 1Hz square that we use to keep time

#define CLOCK_IN_PREN P1REN
#define CLOCK_IN_PDIR P1DIR
#define CLOCK_IN_POUT P1OUT
#define CLOCK_IN_PIN  P1IN
#define CLOCK_IN_PIE  P1IE      // Interrupt enable
#define CLOCK_IN_PIV  P1IV      // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define CLOCK_IN_PIFG P1IFG     // Interrupt flag (bit 1 for each pin that interrupted)


#define CLOCK_IN_B (0)       // Bit


#endif /* PINS_H_ */
