# Why Assembly?

We use pure assembly in the `tsl_asm.asm` file for running in Time-Since-Launch mode because it saves power.

The fastest I could do using pure C was to use a RAM table of words that gets written to the LCD memory on each tick. The code looks like this...

```
__interrupt void rtc_isr(void) {
	if (mode=TSL_MODE) {
		*secs_lcdmem_word = secs_lcd_words[  secs ];
		secs+;
		if (secs==60) {
			/* Handle next minute */
		}
	}
 	CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.
}
```

The compiler didn't handle the memory to memory assignment so Well, so I optimized it to...

```
__interrupt void rtc_isr(void) {
	if (mode=TSL_MODE) {
        asm("        MOV.B     &secs+0,r15           ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        RLAM.W    #1,r15                ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        MOV.W     secs_lcd_words+0(r15),(LCDM0W_L+16) ; [] |../tsl-calibre-msp.cpp:1390|");
		secs+;
		if (secs==60) {
			/* Handle next minute */
		}
	}
 	CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.
}
```

....which executes in 43.6us per tick. 

This is very inefficient because the compiler generated ISR saves and reloads the registers on each call, even though *we* know that nothing else can run since the CPU is sleeping between ticks. 

So it seems the only solution is to write directly in asm where we can load the registers up once on starting the mode and then even load them again. 




