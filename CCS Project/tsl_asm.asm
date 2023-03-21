

        	.cdecls C,LIST,"msp430.h"  				; Include device header file
            .cdecls C,LIST,"lcd_display_exp.h"  	; Links to the info we need to update the LCD

            .global RTL_MODE_BEGIN
            .global TSL_MODE_BEGIN
            .global TSL_MODE_REFRESH

            ;---- Get these pointers from the C side

            ;.text                           ; Assemble to Flash memory
            ;.data							;; Put functions into RAM?

            .sect ".TI.ramfunc"				; Put these functions into RAM where they seem to use slightly less power
            .retain                         ; Ensure current section gets linked
            .retainrefs

			;Assumes the symbol `ram_vector_PORT1` is the address of the ISR vector we will take over.
			.ref		ram_vector_PORT1

			;tsl_next_day is called each 24 hours in TSL mode to update the day count and display
			; Note this is the mangled C++ name for `void tsl_next_dayv()`
			.ref		_Z12tsl_next_dayv

;Begin the TSL mode ISR
;Set the ISR vector to point here to start this mode.
;On the next tick, it will increment the seconds and display the result.

;Assumes the following symbols:
;	secs_lcd_words
; 	secs		   - elapsed seconds

			.ref 	secs_lcd_words			; - table of prerendered values to write to the seconds word in LCDMEM (one entry for each second 0-59)
			.ref	rtc_secs				; - elapsed seconds starting point, read from the RTC in the startup C code

			.ref 	mins_lcd_words			; - table of prerendered values to write to the minutes word in LCDMEM (one entry for each min 0-59)
			.ref	rtc_mins				; - elapsed minutes starting point, read from the RTC in the start up C code

			; Unforntunately hours digits can not be on consecutive LPINs and we are out of call-saved registers
			; anyway so we update the hours digits seporately using these procomuted segment byte arrays
			.ref 	hours_lcd_bytes
			.ref	rtc_hours

			; A complicated 2D table of words that we write to LCDMEM for the frames of the ready-to-launch animation
			.ref 	ready_to_launch_lcd_frame_words

; 17us when minutes do not change (!)

TSL_MODE_BEGIN:

	;This ISR initializes the TSL mode and executes the first pass. It, in turn, changes the ISR vector so that the next
	;time the interrupt happens, just the next pass executes without the initialization. Why run the init code off an ISR?
	;Why not just call the init in the forground thread and have it set everything up for the ISR?
	;We need to init everything into very specific registers for the ISR to be hyperefficient, and we can only
	;have that kind of control in ASM. If any C code runs after our init, it could clobber so of this careful set up.
	;By doing the init on the first ISR call, we are sure that no C code will run after we do our init.
	;We could potentially have the C code call the init code and then have the init code inititiate the sleep,
	;but then the ASM init code would have to be the last code to run before sleeping and that would make the flow in the C side ugly.

	;For MSP430 and MSP430X, the ABI designates R4-R10 as callee-saved registers. That is, a called function is
	;expected to preserve them so they have the same value on return from a function as they had at the point of the
	;call. We will use these since we call back to C when date count changes and we don't want them to get clobbered.

			MOV.W 		#secs_lcd_words,R4			; R4=Base of the secs table
			MOV.W		#(secs_lcd_words+2*60),R5	; R5=1 byte past end of the secs table

			; R6 = compute our location in the table based on current seconds
			MOV.B		&rtc_secs,R6				; Start with seconds. Note that secs are stored as a byte on the C side.
			ADD.W		R6,R6						; Double it so it is now a word pointer
			ADD.W		R4,R6						; R6=Pointer to location of next sec in secs table


			MOV.W 		#mins_lcd_words,R7			; R7=Base of the mins table
			MOV.W		#(mins_lcd_words+2*60),R8	; R8=1 byte past end of the mins table

			; R9= compute our location in the table based on current minutes
			MOV.B		&rtc_mins,R9				; Start with mins. Note that these are stored as a byte on the C side.
			ADD.W		R9,R9						; Double it so it is now a word pointer
			ADD.W		R7,R9						; R9=location of next min in mins table

			; We only have one call saved register left, so for hours we only keep
			; the straight value there and then compute everything else each update.
			; thats OK since hours update only 1/3600th of the time. If we really wanted we could
			; hand-code this all in ASM and then be able to use all regs, but not worth it.
			MOV.B		&rtc_hours,R10					;  R10=Hours

			; move the vector to point to our actual updater now that all the registers are set
			mov.w	#TSL_MODE_ISR, &ram_vector_PORT1

TSL_MODE_ISR

 	  		;OR.B      	#128,&PAOUT_L+0  			; Set DEBUGA for profiling purposes.

 	  		; These next 3 lines are where this product spends the *VAST* majority of its life, so we hyper-optimize.

 	  		MOV.W		@R6+,&(LCDM0W_L+16)			; Read word value from table, increment the pointer, then write the word to the LCDMEM for the Seconds digits

			CMP.W		R6,R5						;; Check if we have reached the end of the table (seconds incremented to 60)

			JNE			TSL_DONE

			; Next minute

			MOV.W		R4,R6						; Reset the seconds pointer back to the top of the table (which, remember is "01").


 	  		MOV.W		@R9+,&(LCDM0W_L+14)			; Read word value from table, increment the pointer, then write the word to the LCDMEM for the Mins digits

			CMP.W		R9,R8						;; Check if we have reached the end of the table (seconds incremented to 60)

			JNE			TSL_DONE

			; Next hour

			MOV.W		R7,R9						; Reset the mins pointer back to the top of the table (which, remember is "01").

			ADD.B		#1,R10						; Increment hours

			CMP			#10,R10
			JGE			HOURS_GE_10

			; If we get here then incremented hours is 1-9
			; Display the hours 1 digit
			MOV.B		hours_lcd_bytes(R10),&(LCDM0W_L+13)		; Display hours in the hours ones digit on the LCD

			JMP 		TSL_DONE					; This wastes 3 cycles, we could just repeat the ending motif here.

HOURS_GE_10
			CMP			#20,R10
			JGE			HOURS_GE_20

			; If we get here then incremented hours is 10-19

			; We could save a couple of cycles here by only displaying the tens digit if hours is equal to "10"
			MOV.B		&(hours_lcd_bytes+1),&(LCDM0W_L+10)			; Display "1" in hours 10's digit
			MOV.B		(hours_lcd_bytes-10)(R10),&(LCDM0W_L+13)	; Display hours 1's digit in the hours ones digit on the LCD (see what I did there? :) )

			JMP 		TSL_DONE					; This wastes 3 cycles, we could just repeat the ending motif here.

HOURS_GE_20

			CMP			#24,R10
			JGE			HOURS_EQ_24

			; If we get here then incremented hours is 20-23

			MOV.B		&(hours_lcd_bytes+2),&(LCDM0W_L+10)			; Display "2" in hours 10's digit
			MOV.B		(hours_lcd_bytes-20)(R10),&(LCDM0W_L+13)	; Display hours 1's digit in the hours ones digit on the LCD (see what I did there? :) )

			JMP 		TSL_DONE					; This wastes 3 cycles, we could just repeat the ending motif here.


HOURS_EQ_24

			; If we get here then incremented hours is 24 so we have to roll to next day

			MOV.W		#0, R10			; Reset hours to 0

			MOV.B		&(hours_lcd_bytes+0),&(LCDM0W_L+13)			; Display "0" in hours 10's digit
			MOV.B		&(hours_lcd_bytes+0),&(LCDM0W_L+10)			; Display "0" in hours 1's digit

			CALL		#_Z12tsl_next_dayv							; Call the C++ side to increment day (this is the mangled name for `tsl_next_day()`


TSL_DONE
;----------------------------------------------------------------------
; 900 | CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending
;     | RV3032 INT interrupt flag that got us into this ISR.
;----------------------------------------------------------------------

        	;This is the standard way to clear the interrupt flag
        	;BIC.B     #2,&PAIFG_L+0         ; Clear the interrupt flag that got us here

        	;Here we save a cycle with a constant-sourced move. We can do this becuase we know a priori that once we enter TSL mode that
        	;the only possible interrupt source is the one that got us here, so safe to clear ALL sources blindly.
        	;Note that we can NOT do this in the RTL mode since we could get an interrupt from the trigger while the ISR
        	;is active and if we overwrite it with a 0 then we would lose it forever.
        	MOV.B     #0,&PAIFG_L+0         ; Clear the interrupt flag that got us here

 	  		;AND.B     #127,&PAOUT_L+0       ; Clear DEBUGA for profiling purposes.

            reti							; pops previous sleep mode, so puts us back to sleep
            								; TODO: Replace this IRET with a sleep and save 4 cycles, and then
            								; just clear 60 PUSHes off the stack every minute wiht a single write to SP.

            nop

            ;Has effect of skipping to next second. Should only be called at the top of a day.
TSL_MODE_REFRESH
			ADD.W		#2,R6						; Skip to next second

			; move the vector to point back to our actual updater now that seconds are corrected
			mov.w	#TSL_MODE_ISR, &ram_vector_PORT1

			JMP		TSL_MODE_ISR

;---- RTL ISR RAMFUNC
; 48us
; 26us startup after CLKOUT from FRAM
; 1.33uA

;---- RTL SQUIGLLE ISR FRAM
; 51us
; 28us startup after CLKOUT from FRAM
; 1.35uA

;Begin the RTL mode ISR
;Set the ISR vector to point here to start this mode.
;Assumes the symbol `ready_to_launch_lcd_frames` points to a table of LCD frames for the squiggle animation
			.ref		ready_to_launch_lcd_frames

RTL_MODE_BEGIN:
	;This entry point sets up all the registers and changes the PORT_1 vector to point to the optimized ISR, which will get called directly on subsequent interrupts
	;First we update the vector to point to the recurring ISR vector
	mov.w	#RTL_MODE_ISR, &ram_vector_PORT1

	;For MSP430 and MSP430X, the ABI designates R4-R10 as callee-saved registers. That is, a called function is
	;expected to preserve them so they have the same value on return from a function as they had at the point of the
	;call.

	mov.w	#ready_to_launch_lcd_frame_words,R12 		; Save the address of the base of the table. We will use this as a live pointer.
	mov.w	#(128-1),R13 								; We will use this for a 1 cycle, no branch way to normalize our pointer which by luck is into an 8*(8*2) table
	mov.w	R12,R14										; Keep a copy of the base address to OR in later


	;; Disable the FRAM controller
	; With controller on uses 1.36uA
	; With controller off uses 1.37uA
	; Why?
	; MOV.W	#FRCTLPW,&FRCTL0			; Write password to FRAM controller
	; BIC.W	#FRPWR,&GCCTL0				; Clear the bit that turns on the controller on wake

	; We are now ready, so drop into the real ISR. The real ISR will be called directly for now on since we updated the vector to point here.

; This is the entry point for the ISR
; We leave this mode when the trigger is pulled by the trigger ISR
RTL_MODE_ISR:

 	; OR.B      #128,&PAOUT_L+0  ;			// DebugA ON - For profiling

	; Copy the data for this frame into the LCDMEM registers.
	; note only need to do this for the LPINs that are actually connected,
	; which are the only ones we put into the table.
	; note we can do this 4 nibbles at a time for free using .W instructions.

      MOV.W @R12+,(LCDM0W_L+0)		; L0,L1,L2,L3 - 4 cycles
      MOV.W @R12+,(LCDM0W_L+2)		; L4,L5,L6,L7
	;					   +4		; L8,L9,L10,L11  (These are the COM pins, do not want to mess with them)
      MOV.W @R12+,(LCDM0W_L+6)		; L12,L13,L14,L15
      MOV.W @R12+,(LCDM0W_L+8)		; L16,L17,L18,L19
      MOV.W @R12+,(LCDM0W_L+10)		; L20,L21,L22,L23
      MOV.W @R12+,(LCDM0W_L+12)		; L24,L25,L26,L27
      MOV.W @R12+,(LCDM0W_L+14)		; L28,L29,L30,L31
      MOV.W @R12+,(LCDM0W_L+16)		; L32,L33,L34,L35

      AND.W R13,R12					; Look ma, no compare/branch/load! 1 cycle each AND and OR.
      OR.W  R14,R12					; OR back in the base address (remember it is 128 byte aligned)


      BIC.B     #2,&PAIFG_L+0 ;  	; Clear interrupt flag
      								; Note that we can not use the constant-MOV 0x00 trick that worked above becuase
      								; in RTL mode the person could pull the trigger, which would set the flag for that pin and if
      								; we then hd bad timing and cleared that bit here then the pull would be lost forever.

 	  ; AND.B     #127,&PAOUT_L+0       ; DEBUGA OFF - For profiling

      RETI

;------------------------------------------------------------------------------
;           Interrupt Vectors
;------------------------------------------------------------------------------

            ;.sect   PORT1_VECTOR              ; Vector
            ;.short  RTL_MODE_BEGIN              ;
            .end

