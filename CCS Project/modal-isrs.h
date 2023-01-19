/*
 * modal-isrs.h
 *
 *  Created on: Dec 27, 2022
 *      Author: passp
 */

#ifndef MODAL_ISRS_H_
#define MODAL_ISRS_H_

extern "C" {
    extern int asmfunc(int a); /* declare external asm function */
    int gvar = 0; /* define global variable */
}

#endif /* MODAL_ISRS_H_ */
