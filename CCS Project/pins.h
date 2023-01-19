/*
 * pins.h
 *
 * Defines all of the pins we are using
 *
 *  Created on: Oct 15, 2022
 *      Author: josh
 */

#ifndef PINS_H_
#define PINS_H_

// Clock in comes from the RV3203 INT pin which is open collector on RV3032 side

#define RV3032_INT_PREN   P1REN
#define RV3032_INT_PDIR   P1DIR
#define RV3032_INT_POUT   P1OUT
#define RV3032_INT_PIN    P1IN
#define RV3032_INT_PIE    P1IE          // Interrupt enable
#define RV3032_INT_PIV    P1IV          // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define RV3032_INT_PIFG   P1IFG         // Interrupt flag (bit 1 for each pin that interrupted)
#define RV3032_INT_PIES   P1IES         // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define RV3032_INT_VECTOR PORT1_VECTOR  // ISR vector


#define RV3032_INT_B (4)       // Bit

// I2C data connection to RV3032 on pin P1.5 which is pin number 23 on MSP430

#define I2C_DTA_PREN P1REN
#define I2C_DTA_PDIR P1DIR
#define I2C_DTA_POUT P1OUT
#define I2C_DTA_PIN  P1IN
#define I2C_DTA_B (5)       // Bit

// I2C clock connection to RV3032 on pin P1.0 which is pin number 28 on MSP430

#define I2C_CLK_PREN P1REN
#define I2C_CLK_PDIR P1DIR
#define I2C_CLK_POUT P1OUT
#define I2C_CLK_PIN  P1IN

#define I2C_CLK_B (0)


// RV3032 clock out on pin P1.1 which is pin number 27 on MSP430

#define RV3032_CLKOUT_PREN P1REN
#define RV3032_CLKOUT_PDIR P1DIR
#define RV3032_CLKOUT_POUT P1OUT
#define RV3032_CLKOUT_PIN  P1IN
#define RV3032_CLKOUT_B (1)


// RV3032 Event input pin is hardwired to ground to avoid having it float during battery changes (uses lots of power)


// Power the RV3032 on pin P1.2 which is pin number 26 on MSP430

#define RV3032_VCC_PREN P1REN
#define RV3032_VCC_PDIR P1DIR
#define RV3032_VCC_POUT P1OUT
#define RV3032_VCC_PIN  P1IN

#define RV3032_VCC_B (2)

// Ground the RV3032 on pin P1.3 which is pin number 23 on MSP430
// Having the RV30332 ground come from the MCU should give extra isolation since it runs though the MSP decoupling cap
// and this also makes sure their grounds are close together.

#define RV3032_GND_PREN P1REN
#define RV3032_GND_PDIR P1DIR
#define RV3032_GND_POUT P1OUT
#define RV3032_GND_PIN  P1IN

#define RV3032_GND_B (3)



// Trigger switch pin P2.1 which is pin number 41 on MSP430. This pin uses ISR vector

#define TRIGGER_PREN   P2REN
#define TRIGGER_PDIR   P2DIR
#define TRIGGER_POUT   P2OUT
#define TRIGGER_PIN    P2IN
#define TRIGGER_PIE    P2IE           // Interrupt enable
#define TRIGGER_PIV    P2IV           // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define TRIGGER_PIFG   P2IFG          // Interrupt flag (bit 1 for each pin that interrupted)
#define TRIGGER_PIES   P2IES          // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define TRIGGER_VECTOR PORT2_VECTOR   // ISR vector

#define TRIGGER_B (1)



// Debug out A on pin P1.7 which is pin number 21 on MSP430

#define DEBUGA_PREN P8REN
#define DEBUGA_PDIR P8DIR
#define DEBUGA_POUT P8OUT
#define DEBUGA_PIN  P8IN

#define DEBUGA_B (2)


// Debug out B on pin P8.3 which is pin number 19 on MSP430

#define DEBUGB_PREN P8REN
#define DEBUGB_PDIR P8DIR
#define DEBUGB_POUT P8OUT
#define DEBUGB_PIN  P8IN

#define DEBUGB_B (3)


// Q1 Top flash LED

#define Q1_TOP_LED_PREN P4REN
#define Q1_TOP_LED_PDIR P4DIR
#define Q1_TOP_LED_POUT P4OUT
#define Q1_TOP_LED_PIN  P4IN
#define Q1_TOP_LED_B (0)       // Bit

// Q2 Bottom flash LED

#define Q2_BOT_LED_PREN P4REN
#define Q2_BOT_LED_PDIR P4DIR
#define Q2_BOT_LED_POUT P4OUT
#define Q2_BOT_LED_PIN  P4IN
#define Q2_BOT_LED_B (1)        // Bit


// TSP voltage regulator

#define TSP_ENABLE_PREN P7REN
#define TSP_ENABLE_PDIR P7DIR
#define TSP_ENABLE_POUT P7OUT
#define TSP_ENABLE_PIN  P7IN
#define TSP_ENABLE_B (0)       // Bit

#define TSP_IN_PREN P4REN
#define TSP_IN_PDIR P4DIR
#define TSP_IN_POUT P4OUT
#define TSP_IN_PIN  P4IN
#define TSP_IN_B (2)       // Bit




#endif /* PINS_H_ */
