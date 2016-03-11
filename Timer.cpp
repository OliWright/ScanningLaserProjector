#include "Timer.h"

struct Prescaler
{
	uint8_t m_bitShift;        // Prescaler value is 1 << m_bitShift
	uint8_t m_clockSelectBits; // Bits to set in the TCCRxB register to select this prescaler
};
typedef volatile uint8_t  Reg8;
typedef volatile uint16_t Reg16;
typedef volatile void     RegVoid;

ClockState clockStates[2];
uint8_t clockStateBufferIdx = 0;

// Contains const information about a hardware timer, like available prescalers.
class TimerInfo
{
public:
	TimerInfo( uint8_t idx, const Prescaler* pPrescalers, uint8_t numPrescalers, uint16_t maxCount )
	: m_pPrescalers( pPrescalers ), m_maxCount( maxCount ), m_idx( idx ), m_numPrescalers( numPrescalers )
	{}

	uint8_t          GetNumPrescalers()          const { return m_numPrescalers; }
	const Prescaler& GetPrescaler( uint8_t idx ) const { return m_pPrescalers[idx]; }
	uint16_t         GetMaxCount()               const { return m_maxCount; }

private:
	const Prescaler* m_pPrescalers;
	const uint16_t   m_maxCount;
	const uint8_t    m_idx;
	const uint8_t    m_numPrescalers;
};

// The prescalers that are available for each timer, and their corresponding clock-select register settings
static const Prescaler kPrescalersTimer0[] = { /* 8 */ {3, (1<<CS01)}, /* 64 */ { 6, (1<<CS00) | (1<<CS01)}, /* 256 */ { 8, (1<<CS02) }, /* 1024 */ { 10, (1<<CS00) | (1<<CS02) } };
static const Prescaler kPrescalersTimer1[] = { /* 8 */ {3, (1<<CS11)}, /* 64 */ { 6, (1<<CS10) | (1<<CS11)}, /* 256 */ { 8, (1<<CS12) }, /* 1024 */ { 10, (1<<CS10) | (1<<CS12) } };
static const Prescaler kPrescalersTimer2[] = { /* 8 */ {3, (1<<CS21)}, /* 32 */ { 5, (1<<CS20) | (1<<CS21)}, /* 64 */  { 6, (1<<CS22) } };

#define TIMER_INFO( idx, numCounterBits ) TimerInfo( idx, kPrescalersTimer##idx, sizeof( kPrescalersTimer##idx ) / sizeof( Prescaler ), (1 << numCounterBits) - 1 )
static const TimerInfo kTimerInfo[3] = 
{
	TIMER_INFO( 0, 8 ),
	TIMER_INFO( 1, 16 ),
	TIMER_INFO( 2, 8 ),
};

static const unsigned long kNumCyclesPerSecond = 16000000;
static const uint8_t kNumCyclesPerMicroSecond = kNumCyclesPerSecond / 1000000;

class TimerState
{
public:
	TimerState()
	: m_numInterrupts( 0 ), m_handler( nullptr ), m_interval( 0 )
	{}

	void SetInterruptHandler( InterruptHandler handler, MicroSeconds interval, uint16_t numInterrupts = 0 )
	{
		m_numInterrupts = numInterrupts;
		m_handler = handler;
		m_interval = interval;
		m_expectedInterruptTime = micros() + interval;
	}

	inline void Interrupt()
	{
		MicroSeconds now = micros();
		MicroSeconds diff = now - m_expectedInterruptTime;
		if( abs(diff) > 100 )
		{
			// The interrupt has happened much earlier than anticipated.
			// Serial.print( TCNT1 );
			// Serial.print( ", ");
			// Serial.println( diff );
			return;
		}
		m_expectedInterruptTime = now + m_interval;
		if( m_handler )
		{
			m_handler();
			if( m_numInterrupts )
			{
				if( --m_numInterrupts == 0 )
				{
					m_handler = nullptr;
				}
			}
		}
	}

private:
	uint16_t         m_numInterrupts;
	InterruptHandler m_handler;
	MicroSeconds     m_interval;
	MicroSeconds     m_expectedInterruptTime;
};

static TimerState timerStates[3];

//ISR(TIMER0_COMPA_vect) { timerStates[0].Interrupt(); }
ISR(TIMER1_COMPA_vect) { timerStates[1].Interrupt(); }
//ISR(TIMER2_COMPA_vect) { timerStates[2].Interrupt(); }

void SetTimerInterrupt( uint8_t timerIdx, MicroSeconds interval, InterruptHandler handler, uint16_t numInterrupts )
{
	const TimerInfo& timerInfo = kTimerInfo[ timerIdx ];

	// Convert interval in micro-seconds to a number of cycles
	unsigned long intervalCycles = interval * kNumCyclesPerMicroSecond;

	// Find the lowest prescaler that will give us a count <= maxCount
	uint8_t prescalerIdx;
	uint16_t countTarget = timerInfo.GetMaxCount();
	for( prescalerIdx = 0; prescalerIdx < timerInfo.GetNumPrescalers(); ++prescalerIdx )
	{
		const Prescaler& prescaler = timerInfo.GetPrescaler( prescalerIdx );
		unsigned long count = intervalCycles >> prescaler.m_bitShift;
		if( count <= timerInfo.GetMaxCount() )
		{
			// The counter target that we would need for this prescaler fits into
			// this timer's counter bit width.
			// So this is the best prescaler to use.
			countTarget = (uint16_t) count;
			break;
		}
	}
	if( prescalerIdx == timerInfo.GetNumPrescalers() )
	{
		Serial.println( "Timer interval out of range." );
		// Set the maximum interval that we can for this timerIdx
		prescalerIdx = timerInfo.GetNumPrescalers() - 1;
		countTarget = timerInfo.GetMaxCount();
	}
	const Prescaler& prescaler = timerInfo.GetPrescaler( prescalerIdx );
	uint8_t clockSelect = prescaler.m_clockSelectBits;

	//Serial.println( 1 << prescaler.m_bitShift );
	//Serial.println( prescaler.m_clockSelectBits );
	 //Serial.println( countTarget );
	//Serial.println( ((unsigned long) countTarget) * (1 << prescaler.m_bitShift) );
	//cli();
	timerStates[ timerIdx ].SetInterruptHandler( handler, interval, numInterrupts );
	// Poke the registers
	if( timerIdx == 0 )
	{
		Serial.println( "AAAAAGGHGGHHHH");
		TCCR0A = 0;
		TCCR0B = 0;
		TCNT0 = 0;
		OCR0A = countTarget;
		TCCR0B = prescaler.m_clockSelectBits;
		TCCR0A = (1 << WGM01);
		TIMSK0 |= (1 << OCIE0A);
	}
	else if( timerIdx == 1)
	{
		TIMSK1 &= ~(1 << OCIE1A);
		TCCR1A = 0;
		//TCCR1B = 0;
		TCNT1 = 0;
		OCR1A = countTarget;
		TCCR1B = prescaler.m_clockSelectBits | (1 << WGM12);
		TIMSK1 |= (1 << OCIE1A);
	}
	else
	{
		Serial.println( "AAAAAGGHGGHHHH");
		TCCR2A = 0;
		TCCR2B = 0;
		TCNT2 = 0;
		OCR2A = countTarget;
		TCCR2B = prescaler.m_clockSelectBits;
		TCCR2A = (1 << WGM21);
		TIMSK2 |= (1 << OCIE2A);
	}
	//sei();
}

void ConfigureTimer1ForClock()
{
	// 16-bit with prescaler of 8
	// Counter resolution is 0.5us
	TIMSK1 &= ~(1 << OCIE1A);
	TCCR1A = 0;
	//TCCR1B = 0;
	TCNT1 = 0;
	OCR1A = 0xffff;
	TCCR1B = (1 << CS11);// | (1 << WGM12);
	//TIMSK1 |= (1 << OCIE1A);
}

void ConfigureTimer2ForPWM( uint8_t dutyCycle )
{
	TIMSK2 &= ~(1 << OCIE2A);
	TCCR2A = 0;
	TCCR2B = 0;
	TCNT2 = 0;
	OCR2A = 80; // Wrap at 80.  25KHz
	OCR2B = dutyCycle; // Pin 3
	TCCR2B = (1<<CS20); // Prescaler of 8.  2MHz
	TCCR2A = 0x23;
}

void DisableAllTimerInterrupts()
{
	TIMSK0 &= ~(1 << OCIE0A);
	TIMSK1 &= ~(1 << OCIE1A);
	TIMSK2 &= ~(1 << OCIE2A);
}
