#include "ScanningLaserProjector.h"
#include "Fonts.h"

static const Colour kRed( 255, 0, 0 );
static const Colour kGreen( 0, 255, 0 );
static const Colour kBlue( 0, 0, 255 );
static const Colour kDim( 32, 32, 32 );

static ColourArray colourArray;
static InputState previousInputState;

static const uint8_t kWidth = 128;
static const uint8_t kHeight = 16;

static const uint8_t kWidthBytes = 128 >> 3;

GFXcanvas1 gfx( kWidth, kHeight );

void TurnLedOn()
{
	digitalWrite( LED_PIN, HIGH );
}
void TurnLedOff()
{
	digitalWrite( LED_PIN, LOW );
}

void TimerHandler()
{
	TurnLedOn();
	SetTimerInterrupt( 0, (MicroSeconds) 30000, TurnLedOff, 1 );
}

inline void writePixel( uint8_t byte, uint8_t bitIdx )
{
	digitalWrite( LASER_PIN, (byte >> bitIdx) & 1 );
}

inline void shortDelay( uint8_t count )
{
	for( uint8_t i = 0; i < count; ++i )
	{
		__asm__("nop\n\t" "nop\n\t");
	}
}

uint8_t interBitDelayCount = 20;
uint8_t interByteDelayCount = 15;

// Calculate inter-bit and inter-byte delay count values for a desired scan duration
// This is done using empirical data.
void calculateDelayCounts( MicroSeconds desiredHScanDuration )
{
	// TODO: This
}

inline void interBitDelay() { shortDelay( interBitDelayCount ); }
inline void interByteDelay() { shortDelay( interByteDelayCount ); }

void HScan( uint8_t idx )
{
	uint8_t* pByte = gfx.getBuffer() + (idx * kWidthBytes);
	for( uint8_t x = 0; x < kWidthBytes; ++x )
	{
		uint8_t byte = *pByte++;
		writePixel( byte, 7 );
		interBitDelay();
		writePixel( byte, 6 );
		interBitDelay();
		writePixel( byte, 5 );
		interBitDelay();
		writePixel( byte, 4 );
		interBitDelay();
		writePixel( byte, 3 );
		interBitDelay();
		writePixel( byte, 2 );
		interBitDelay();
		writePixel( byte, 1 );
		interBitDelay();
		writePixel( byte, 0 );
		interByteDelay();
	}
}


void Startup()
{
	memset( &previousInputState, 0, sizeof( previousInputState ) );

	gfx.setFont( &FreeMono9pt7b );
	gfx.setCursor( 3, kHeight - 3 );
	gfx.drawRect( 0, 0, kWidth, kHeight, 1 );
	gfx.print( "Hello World" );
}

void dumpDisplayToTTY()
{
	uint8_t* pByte = gfx.getBuffer();
	for( uint8_t y = 0; y < kHeight; ++y )
	{
		for( uint8_t x = 0; x < kWidthBytes; ++x )
		{
			for( int a=7; a >= 0; a-- )
			{ 
				Serial.print( bitRead (*pByte, a) );
			}
			++pByte;
		}
		Serial.println("");
	}
	unsigned long start = micros();
	for( uint8_t i = 0; i < kHeight; ++i )
	{
		HScan( i );
	}
	unsigned long duration = micros() - start;
	Serial.println( duration );
}

#define PUSH_BUTTON( field ) input.field = inputState.field && !previousInputState.field

const ColourArray& Update( const InputState& inputState )
{
	InputState input = inputState;
	// Modify the push-button inputs so they only appear as set the first frame
	// they're pressed
	PUSH_BUTTON( m_redButton );
	PUSH_BUTTON( m_blueButton );

	previousInputState = inputState;

	colourArray.Clear();
	// for( unsigned int i = 0; i < (unsigned int) inputState.m_measuredRevsPerSecond; ++i )
	// {
		// colourArray[i] = kDim;
	// }

	// Button tests
	if( input.m_redButton )
	{
		SetTimerInterrupt( 1, (MicroSeconds) 100000, TimerHandler, 10 );
	}
	if( input.m_blueButton )
	{
		dumpDisplayToTTY();
	}
	if( inputState.m_redButton )
	{
		colourArray[0] = kRed;
	}
	if( inputState.m_blueButton )
	{
		colourArray[1] = kBlue;
	}

	return colourArray;
}
