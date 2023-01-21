/*
 * modal-isrs.h
 *
 *  Created on: Dec 27, 2022
 *      Author: passp
 */

#ifndef TSL_ASM_H_
#define TSL_ASM_H_

extern "C" {
    extern int tslmode_asm(int a); /* declare external asm function */
    int gvar = 0; /* define global variable */
}

#endif /* TSL_ASM_H_ */
