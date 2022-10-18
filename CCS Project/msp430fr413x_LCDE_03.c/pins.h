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

// I2C data connection to RV3203 on pin P1.3 which is pin number 25 on MSP430

#define I2C_DTA_PREN P1REN
#define I2C_DTA_PDIR P1DIR
#define I2C_DTA_POUT P1OUT
#define I2C_DTA_PIN  P1IN
#define I2C_DTA_B (3)       // Bit

// I2C clock connection to RV3203 on pin P1.2 which is pin number 26 on MSP430

#define I2C_CLK_PREN P1REN
#define I2C_CLK_PDIR P1DIR
#define I2C_CLK_POUT P1OUT
#define I2C_CLK_PIN  P1IN

#define I2C_CLK_B (2)

#endif /* PINS_H_ */
