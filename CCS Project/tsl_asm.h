/*
 * tsl-asm.h
 *
 *  The TSL spends nearly all of its life in these functions, so we optimize them in assembly.
 *  This header holds the entry points for the ISRs for ready-to-launch and time-since-lanuch
 *  modes. To enter either mode, set the ISR vector to point to one of these functions and then the
 *  next time the interrupt fires, that function will set everything up for that mode and then reassign
 *  the vector to point to the long-term ISR.
 *
 */

#ifndef TSL_ASM_H_
#define TSL_ASM_H_

extern "C" {

    struct tslmode_state_t {
        unsigned int s;
        unsigned int m;
        unsigned int h;
        unsigned int d_1_10;
        unsigned int d_100_1K;
        unsigned int d_10k_100K;
        unsigned int d_10k_1M;
    };

    #pragma FUNC_NEVER_RETURNS
    // Note the order here is reversed so that the 32 bit "days" value lands in registers rather than on the stack as per CCS calling conventions SLAA140A section 1.3
    extern int enter_tslmode_asm( unsigned s ); /* declare external asm function */
}

#endif /* TSL_ASM_H_ */
