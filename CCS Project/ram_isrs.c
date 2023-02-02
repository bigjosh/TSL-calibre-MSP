#include "ram_isrs.h"

// All the RAM-based interrupt vectors. Ideally, these would be pointers to `__interrupt` functions, but the compiler can't handle that.
// Note these are void pointers rather than function pointers because if you make them
// function pointers then the compiler will try to make trampolines to them and fail.
// There does not seem to be a way to specify that a pointer is "small" with the TI compiler.
// Note these need to be `volatile` so the compiler always writes the values into the actual memory locations rather than potentially caching them.

__attribute__((section(".ram_int45"))) void * volatile ram_vector_LCD_E;
__attribute__((section(".ram_int46"))) void * volatile ram_vector_PORT2;
__attribute__((section(".ram_int47"))) void * volatile ram_vector_PORT1;
__attribute__((section(".ram_int48"))) void * volatile ram_vector_ADC;
__attribute__((section(".ram_int49"))) void * volatile ram_vector_USCI_B0;
__attribute__((section(".ram_int50"))) void * volatile ram_vector_USCI_A0;
__attribute__((section(".ram_int51"))) void * volatile ram_vector_WDT;
__attribute__((section(".ram_int52"))) void * volatile ram_vector_RTC;
__attribute__((section(".ram_int53"))) void * volatile ram_vector_TIMER1_A1;
__attribute__((section(".ram_int54"))) void * volatile ram_vector_TIMER1_A0;
__attribute__((section(".ram_int55"))) void * volatile ram_vector_TIMER0_A1;
__attribute__((section(".ram_int56"))) void * volatile ram_vector_TIMER0_A0;
__attribute__((section(".ram_int57"))) void * volatile ram_vector_UNMI;
__attribute__((section(".ram_int58"))) void * volatile ram_vector_SYSNMI;
//__attribute__((section(".ram_int59"))) void *ram_vector_RESET;          // This one is dumb because the SYSRIVECT bit gets cleared on reset so this can never happen.
