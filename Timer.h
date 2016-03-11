#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

typedef long MicroSeconds;
typedef long Ticks;
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
//
// Please be aware that timer0 is used by the standard functions 'delay',
// 'millis' and 'micros'. So if you want to use those, then leave timer0 alone.
// Also, timer2 is used by the standard function 'tone'.
void SetTimerInterrupt( uint8_t timerIdx, MicroSeconds interval, InterruptHandler handler, uint16_t numInterrupts = 0 );

// Configures timer1 to operate as a 16-bit clock with resolution 0.5us
void ConfigureTimer1ForClock();

void ConfigureTimer2ForPWM( uint8_t dutyCycle );

// Get the current 0.5us clock value.
struct ClockState
{
	uint16_t prevClock;
	uint16_t clockHigh;
	ClockState() : prevClock(0), clockHigh(0) {}
};
extern ClockState clockStates[2];
extern uint8_t clockStateBufferIdx;
inline Ticks GetClockMain()
{
	// Copy the previous ClockState
	ClockState& clockState = clockStates[ 1 - clockStateBufferIdx ];
	clockState = clockStates[ clockStateBufferIdx ];

	uint16_t clock = TCNT1;
	if( clockState.prevClock > clock )
	{
		// Wrap
		++clockState.clockHigh;
	}
	clockState.prevClock = clock;

	// Flip the buffers so the interrupt handler sees what's
	// just happened as an atomic operation.
	clockStateBufferIdx =1 - clockStateBufferIdx;

	return (((Ticks) clockState.clockHigh) << 16) | clock;
}

inline Ticks GetClockInterrupt()
{
	const ClockState& clockState = clockStates[ clockStateBufferIdx ];
	uint16_t clock = TCNT1;
	uint16_t high = clockState.clockHigh;
	if( clockState.prevClock > clock )
	{
		// Wrap
		++high;
	}
	return (((Ticks) high) << 16) | clock;
}

inline MicroSeconds TicksToMicroSeconds( Ticks ticks ) { return ticks >> 1; }
inline Ticks MicroSecondsToTicks( MicroSeconds us ) { return us << 1; }

void DisableAllTimerInterrupts();

#endif