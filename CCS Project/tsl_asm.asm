

        	.cdecls C,LIST,"msp430.h"  				; Include device header file
            .cdecls C,LIST,"lcd_display_exp.h"  	; Links to the info we need to update the LCD

            .global enter_tslmode_asm

            ;---- Get these pointers from the C side

            .text                           ; Assemble to Flash memory
            .retain                         ; Ensure current section gets linked
            .retainrefs

			.ref	 	secs_lcdmem_word
			.ref 		secs_lcd_words

			; R12:R13 = Days as BCD value

			; R12 = Seconds

R_S_LCD_MEM	.set		R5						;; Address of the word the holds the seconds digits in LCD memory
R_S_TBL		.set		R6						;; base address of the table of data words to write to the seconds word in LCD memory
R_S_PTR		.set		R7						;; ptr into next word to use in the table of data words to write to the seconds word in LCD memory
R_S_CNT		.set		R8						;; Seconds counter, starts at 60 down to 0.

enter_tslmode_asm

			; We get secs in R12 for free by the C passing conventions

			mov 		&secs_lcdmem_word,R_S_LCD_MEM		;; Address in LCD memory to write seconds digits

			mov 		#secs_lcd_words,R_S_TBL

			mov 		R_S_TBL , R_S_PTR

			mov 		#SECS_PER_MIN,R_S_CNT

			nop
            bis.w   #LPM3+GIE,SR            ; Enter LPM3
			nop
			; never get here

TSL_MODE_ISR
			OR.B     	#4,&PDOUT_H+0			;; Set DEBUGA for profiling purposes.

			;mov.w		&secs_lcd_words,r13
			;mov.w		#secs_lcd_words,r12
			;add.w		#2,r12
			;mov.w		@r5,r6


			mov.w 		@R_S_PTR+,&(LCDM0W_L+16)	;; Copy the segments for the seconds digits
			;mov.w 		&secs_lcd_words+4,&(LCDM0W_L+16)	;; Copy the segments for the seconds digits


;			;mov.w 		@R_S_PTR+,0(R_S_LCD_MEM)
			;mov.w 		#0xffff,(LCDM0W_L+16)

			dec			R_S_CNT

			JNE			ISR_DONE

			mov 		R_S_TBL,R_S_PTR
			mov 		#SECS_PER_MIN,R_S_CNT

ISR_DONE
;----------------------------------------------------------------------
; 900 | CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending
;     | RV3032 INT interrupt flag that got us into this ISR.
;----------------------------------------------------------------------
        	BIC.B     #2,&PAIFG_L+0         ; Clear the interrupt flag that got us here

			BIC.B     #4,&PDOUT_H+0			;; Clear DEBUGA for profiling purposes.
            reti							; pops previous sleep mode, so puts us back to sleep


            nop
;------------------------------------------------------------------------------
;           Interrupt Vectors
;------------------------------------------------------------------------------

            .sect   PORT1_VECTOR              ; Vector
            .short  TSL_MODE_ISR              ;
            .end

