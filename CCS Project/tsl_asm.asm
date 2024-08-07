

        	.cdecls C,LIST,"msp430.h"  				; Include device header file
            .cdecls C,LIST,"lcd_display_exp.h"  	; Links to the info we need to update the LCD
            .cdecls C,LIST,"tsl_asm.h"  			; References to calls and variables shared with the C side
            .cdecls C,LIST,"ram_isrs.h"  			; We need the addresses for the RAM vector table so we can update from RTL_BEGIN to RTL mode.
            .cdecls C,LIST,"pins.h"					; We need the specific RAM vector for the CLKOUT pin

            .retain                         ; Ensure current section gets linked
            .retainrefs

			.ref 		tsl_new_day			; C function called when day rolls over. Sadly I can not figure out how to define this in the tsl_asm.h file. :(

			.text

; Begin the TSL mode ISR
; Set the RAM ISR vector to point here to start this mode.
; It will switch the MCU back to to the FRAM interrupt vector.

; 17us when minutes do not change (!)

			.global 	TSL_MODE_BEGIN				; Publish function address so C can call it.
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

			MOV.W 		#secs_lcd_words,R4			; R4=Base of the secs table (so we can reset back to the begining when we get to the end)
			MOV.W		#(secs_lcd_words+2*60),R5	; R5=1 byte past end of the secs table (so we can test if we got to the end)

			; R6 = compute our location in the table based on current seconds
			MOV.B		&tsl_secs,R6				; Start with seconds. Note that secs are stored as a byte on the C side.
			ADD.W		R6,R6						; Double it so it is now a word pointer
			ADD.W		R4,R6						; R6=Pointer to location of next sec in secs table


			MOV.W 		#mins_lcd_words,R7			; R7=Base of the mins table
			MOV.W		#(mins_lcd_words+2*60),R8	; R8=1 byte past end of the mins table

			; R9= compute our location in the table based on current minutes
			MOV.B		&tsl_mins,R9				; Start with mins. Note that these are stored as a byte on the C side.
			ADD.W		R9,R9						; Double it so it is now a word pointer
			ADD.W		R7,R9						; R9=location of next min in mins table

			; We only have one call saved register left, so for hours we only keep
			; the straight value there and then compute everything else each update.
			; thats OK since hours update only 1/3600th of the time. If we really wanted we could
			; hand-code this all in ASM and then be able to use all regs, but not worth it.
			MOV.B		&tsl_hours,R10				;  R10=Hours

			; Next we need a register to hold the address of the persistent mins counter in FRAM. We do this once a minute, so should be efficient.
			MOV.W		&persistant_mins_ptr,R11    ; R11=Address of persitent_data.mins counter in FRAM.
													; Must be saved before calling C!

			MOV.W 		#(PFWP|DFWP),R12			; R12=The constant to write to SYSCFG0 to relock the info FRAM memory. We keep it in a register becuase it is faster than using a constant for this value (0x03)
													; Must be saved before calling C!

			; Switch to FRAM based interrupt vector table
			; Since the TSL_MODE_ISR is the default ISR for the CLKOUT pin in the FRAM vector table,
			; this will swithc us to directly call TSL_MODE_ISR on the next CLKOUT interrupt.
			; In c: `SYSCTL &= ~SYSRIVECT`

			BIC.W	 	#SYSRIVECT,&SYSCTL

			; Fall through to the actual handler, which will run once now and then will get called direct on each subsequent interrupt.


TSL_MODE_ISR

 	  		;OR.B      	#128,&PAOUT_L+0  			; Set DEBUGA for profiling purposes.

 	  		; These next 3 lines are where this product spends the *VAST* majority of its life, so we hyper-optimize.

 	  		MOV.W		@R6+,&(LCDM0W_L+16)			; Read word value from table, increment the pointer, then write the word to the LCDMEM for the Seconds digits

			CMP.W		R6,R5						; Check if we have reached the end of the seconds table (seconds incremented to 60)
			JNE			TSL_DONE					; This takes 2 cycles, branch taken or not

			; Next minute

			; Increment the persisant minutes counter in FRAM. Note that if this is the end of the day, this will increment that counter to 1440 (24 hours)
			; To account for this, (1) the next_day() C code always sets the mins directly back to 0, and (2) the startup code specifically looks for the case where the
			; mins is 1440 and increments the days if so becuase that means we failed between *here* and when the next_day would have incremented the days.


			; TODO: Make this more efficient with registers.
			; TODO: Is the extra power for lock/unlock worth it?

			; Note that we use MOV to update the SYSCFG0 rather than BIC and BIS because MOV takes 1 fewer cycles.
			; The only downside to this is that we overwrite whatever is in the other bits, but the program memory should
			; always be locked in our application.

			; Note that keeping SYSCFG0 in a register would not speed things up since indirect addressing is always implemented as address+register, they just used R0 if you do not specify one.
			; Is locking/unlock for each update worth it? Well, it only costs about 5 minutes per century: https://www.google.com/search?q=%28100+years%29+*++%286+microsecond%2Fminute%29

			MOV.W		#PFWP,&SYSCFG0			; 3 cycles. Unlock the info section of FRAM, leave program section locked. IN this case, #PFWP is autoaliased to the constant generator register.
			INC.W		0(R11)					; 4 cycles. Increment the mins counter. Note we do not need to do any overflow checking becuase once a day `tsl_next_day` will run and reset this.
			MOV.W		R12,&SYSCFG0	        ; 3 cycles. Lock both info section and program section of FRAM. Using a register for #((PFWP|DFWP) saves one cycle becuase it is not a value in the constant generator.

			; Now update the mins on the display

			MOV.W		R4,R6						; Reset the seconds pointer back to the top of the table (which, remember is "01") for next pass. We are currently displaying "00" which is in positon 59 in the table.

 	  		MOV.W		@R9+,&(LCDM0W_L+14)			; Read word value from table, increment the pointer, then write the word to the LCDMEM for the Mins digits

			CMP.W		R9,R8						;; Check if we have reached the end of the table (seconds incremented to 60)

			JNE			TSL_DONE

			; Next hour

			MOV.W		R7,R9						; Reset the mins pointer back to the top of the table (which, remember is "01").

			ADD.B		#1,R10						; Increment hours. TODO: We could use an ADD.B here and then use overlfow flag to avoid the CMP

			CMP			#10,R10
			JGE			HOURS_GE_10

			; If we get here then incremented hours is 1-9
			; Display the hours 1 digit
			MOV.B		hours_lcd_bytes(R10),&(LCDM0W_L+13)		; Display hours in the hours ones digit on the LCD. Remember that the hours table is *not* offset like secs and mins.

			JMP 		TSL_DONE					; This wastes 3 cycles every hour, we could just repeat the ending motif here.

HOURS_GE_10
			CMP			#20,R10
			JGE			HOURS_GE_20

			; If we get here then incremented hours is 10-19

			; We could save a couple of cycles here by only displaying the tens digit if hours is equal to "10"
			MOV.B		&(hours_lcd_bytes+1),&(LCDM0W_L+10)			; Display "1" in hours 10's digit
			MOV.B		(hours_lcd_bytes-10)(R10),&(LCDM0W_L+13)	; Display hours 1's digit in the hours ones digit on the LCD (see what I did there? :) )

			JMP 		TSL_DONE					; This wastes 3 cycles every hour, we could just repeat the ending motif here.

HOURS_GE_20

			CMP			#24,R10
			JGE			HOURS_EQ_24

			; If we get here then incremented hours is 20-23

			MOV.B		&(hours_lcd_bytes+2),&(LCDM0W_L+10)			; Display "2" in hours 10's digit
			MOV.B		(hours_lcd_bytes-20)(R10),&(LCDM0W_L+13)	; Display hours 1's digit in the hours ones digit on the LCD (see what I did there? :) )

			JMP 		TSL_DONE					; This wastes 3 cycles every hour, we could just repeat the ending motif here.


HOURS_EQ_24

			; If we get here then incremented hours is 24 so we have to roll to next day

			MOV.W		#0, R10			; Reset hours to 0

			MOV.B		&(hours_lcd_bytes+0),&(LCDM0W_L+13)			; Display "0" in hours 10's digit
			MOV.B		&(hours_lcd_bytes+0),&(LCDM0W_L+10)			; Display "0" in hours 1's digit


			PUSH.W		R11											; R11-R14 are not callee saved, so we have to save them before calling C.
			PUSH.W		R12
			CALL		#tsl_new_day								; Call the C++ side for a new day. Updates the days digits on the LCD and atomically
																	; (increments the persistant days and clears the minutes).
			POP.W		R12											; TODO: We could just reload the orginal values here and save a PUSH/POP.
			POP.W		R11

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
            								; just clear 60 PUSHes off the stack every minute with a single write to SP.

            .sect   PORT1_VECTOR             ; Vector
            .short  TSL_MODE_ISR             ;


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

		.text								; We need to switch back to text section becuase the above ISR definition chnages to the ISR section. :/

		.global  	RTL_MODE_BEGIN			; Publish function address so C can call it.

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

            .end

