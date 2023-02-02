/*
 * ram_isrs.h
 *
 *  Created on: Feb 1, 2023
 *      Author: passp
 */

#ifndef RAM_ISRS_H_
#define RAM_ISRS_H_

#include "msp430.h"

// All the RAM-based interrupt vectors. Ideally, these would be pointers to `__interrupt` functions, but the compiler can't handle that.
// Note these are void pointers rather than function pointers because if you make them
// function pointers then the compiler will try to make trampolines to them and fail.
// There does not seem to be a way to specify that a pointer is "small" with the TI compiler.
// Note these need to be `volatile` so the compiler always writes the values into the actual memory locations rather than potentially caching them.

extern void * volatile ram_vector_LCD_E;
extern void * volatile ram_vector_PORT2;
extern void * volatile ram_vector_PORT1;
extern void * volatile ram_vector_ADC;
extern void * volatile ram_vector_USCI_B0;
extern void * volatile ram_vector_USCI_A0;
extern void * volatile ram_vector_WDT;
extern void * volatile ram_vector_RTC;
extern void * volatile ram_vector_TIMER1_A1;
extern void * volatile ram_vector_TIMER1_A0;
extern void * volatile ram_vector_TIMER0_A1;
extern void * volatile ram_vector_TIMER0_A0;
extern void * volatile ram_vector_UNMI;
extern void * volatile ram_vector_SYSNMI;
//__attribute__((section(".ram_int59"))) void *ram_vector_RESET;          // This one is dumb because the SYSRIVECT bit gets cleared on reset so this can never happen.


// Activate the RAM ISR table (replaces the default FRAM table)
// Make sure you have assigned functions to any of the above vectors that might get called.
#define ACTIVATE_RAM_ISRS() do {SYSCTL |= SYSRIVECT;} while (0)

#endif /* RAM_ISRS_H_ */
