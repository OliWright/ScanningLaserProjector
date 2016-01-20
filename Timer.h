#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

typedef unsigned long MicroSeconds;
typedef void (*InterruptHandler)();

// Set up a timer to call an interrupt handler at a set interval.
// Optionally set a number of times to call the interrupt handler
// (if numInterrupts is set to 0, the handler will be called indefinitely).
// There are 3 timers. Timers 0 and 2 have 8 bit counters, the
// implication of which is that their precision will be compromised
// with larger intervals because higher and higher prescalers
// will have to be used.
// Timer 1 has a 16 bit counter, so can generally use a lower
// prescaler which effectively increases its precision.
void SetTimerInterrupt( uint8_t timerIdx, MicroSeconds interval, InterruptHandler handler, uint16_t numInterrupts = 0 );

#endif