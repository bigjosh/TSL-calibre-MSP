/*
 * modal-isrs.h
 *
 *  Created on: Dec 27, 2022
 *      Author: passp
 */

#ifndef TSL_ASM_H_
#define TSL_ASM_H_

extern "C" {
    #pragma FUNC_NEVER_RETURNS

    // Note the order here is reversed so that the 32 bit "days" value lands in registers rather than on the stack as per CCS calling conventions SLAA140A section 1.3
    extern int enter_tslmode_asm( unsigned s ); /* declare external asm function */
}

#endif /* TSL_ASM_H_ */
