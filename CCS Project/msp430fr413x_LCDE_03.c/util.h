/*
 * util.h
 *
 * Some handy stuffs
 *
 *  Created on: Oct 15, 2022
 *      Author: passp
 */

#ifndef UTIL_H_
#define UTIL_H_


#define _BV(x) (1<<x)               // bit value

#define SBI(x,b) ((x) |= _BV(b))      // set bit b in x
#define CBI(x,b) ((x) &= ~_BV(b))      // clear bit b in x
#define TBI(x,b) ((x) & _BV(b)!=0)      // test bit b in x


#endif /* UTIL_H_ */
