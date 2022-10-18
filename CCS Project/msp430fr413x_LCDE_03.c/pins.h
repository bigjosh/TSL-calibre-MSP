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

// Clock in comes from the RV3203 INT pin

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


// Power the RV3032 out on pin P1.6 which is pin number 22 on MSP430

#define RV3032_VCC_PREN P1REN
#define RV3032_VCC_PDIR P1DIR
#define RV3032_VCC_POUT P1OUT
#define RV3032_VCC_PIN  P1IN

#define RV3032_VCC_B (6)

// Debug out on pin P1.4 which is pin number 24 on MSP430

#define DEBUGA_PREN P1REN
#define DEBUGA_PDIR P1DIR
#define DEBUGA_POUT P1OUT
#define DEBUGA_PIN  P1IN

#define DEBUGA_B (4)


// Debug out on pin P1.4 which is pin number 24 on MSP430

#define DEBUGB_PREN P1REN
#define DEBUGB_PDIR P1DIR
#define DEBUGB_POUT P1OUT
#define DEBUGB_PIN  P1IN

#define DEBUGB_B (4)



#endif /* PINS_H_ */
