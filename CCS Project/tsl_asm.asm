

            .cdecls C,LIST,"msp430.h"  ; Include device header file

            .global tslmode_asm

            .text                           ; Assemble to Flash memory
            .retain                         ; Ensure current section gets linked
            .retainrefs

tslmode_asm
StopWDT     mov.w   #WDTPW+WDTHOLD,&WDTCTL  ; Stop WDT
SetupP1     bic.b   #BIT0,&P1OUT            ; Clear P1.0 output
            bis.b   #BIT0,&P1DIR            ; P1.0 output
            bic.w   #LOCKLPM5,PM5CTL0       ; Unlock I/O pins

Mainloop    xor.b   #BIT0,&P1OUT            ; Toggle P1.0 every 0.1s
Wait        mov.w   #50000,R15              ; Delay to R15
L1          dec.w   R15                     ; Decrement R15
            jnz     L1                      ; Delay over?
            dadd    r12,r12
            jmp     Mainloop                ; Again
            reti
