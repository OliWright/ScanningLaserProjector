#include "ScanningLaserProjector.h"
#include "Fonts.h"
#include <EEPROM.h>

static InputState previousInputState;

// Matrix dimensions and derived values
static const uint8_t  kWidth = 128;
static const uint8_t  kNumLasers = 4; // Don't change without changing all the code.
static const uint8_t  kNumMirrors = 16; // The number of mirrors in the drum.
static const uint8_t  kHeight = kNumMirrors * kNumLasers;
static const uint8_t  kWidthBytes = 128 >> 3;
static const uint16_t kLaserByteOffset = kNumMirrors * kWidthBytes;
static const Ticks kDrift = 4;

// Mirror drum
static Ticks previousDrumSyncTime = 0;
volatile static Ticks actualSyncTime = 0;
volatile static uint8_t drumSyncTimePosted = 0;
static Ticks previousDrumRevolutionDurationTicks = 0;
static uint8_t revsPerSecond = 0;
static uint16_t firstMirrorOffset = 1936; // Fraction of drum revolution * 4096

static uint8_t mirrorToRaster[] = {0, 14, 7, 12, 2, 9, 5, 11, 1, 15, 6, 13, 3, 8, 4, 10 };

uint8_t currentMirrorIdx = 0;
uint8_t pwmCompare = 105;

#define CURRENT_HORIZONTAL_RASTER_VERSION 0x0102
static uint16_t rasterHorizontalOffsetVersion;
static uint16_t rasterHorizontalOffsets[kNumMirrors] = {0};

GFXcanvas1 gfx( kWidth, kHeight );
//GFXcanvas1 gfx2( kWidth, kHeight );

static void readEepromData( void* pDst, int eepromAddress, uint16_t numBytes )
{
	uint8_t* pByte = (uint8_t*) pDst;
	for( uint16_t i = 0; i < numBytes; ++i )
	{
		pByte[i] = EEPROM.read( eepromAddress + i );
	}
}

static void writeEepromData( const void* pSrc, int eepromAddress, uint16_t numBytes )
{
	const uint8_t* pByte = (uint8_t*) pSrc;
	for( uint16_t i = 0; i < numBytes; ++i )
	{
		EEPROM.update( eepromAddress + i, pByte[i] );
	}
}

void Startup()
{
	memset( &previousInputState, 0, sizeof( previousInputState ) );

	// Draw some initial data into the bitmap
	gfx.setFont( &FreeMono9pt7b );
	gfx.setCursor( 3, 12 );
	gfx.drawRect( 0, 0, kWidth, kNumMirrors, 1 );
	gfx.print( "Hello World" );
	//gfx.fillRect( 0, 0, 128, 16, 1 );
	//gfx.fillRect( 64-4, 0, 8, 16, 1 );
	// gfx.fillRect( 64, 0, 16, 16, 1 );
	// gfx.fillRect( 96, 0, 16, 16, 1 );
	//gfx.fillRect( 0, 0, 32, 16, 1 );
	//gfx.fillRect( 96, 0, 32, 1, 1 );

	DisableAllTimerInterrupts();
	ConfigureTimer1ForClock();
	ConfigureTimer2ForPWM(pwmCompare);

	// Read horizontal offsets from EEPROM
	readEepromData( &rasterHorizontalOffsetVersion, 0, 2 );
	if( rasterHorizontalOffsetVersion == CURRENT_HORIZONTAL_RASTER_VERSION )
	{
		Serial.println( "Reading rasterHorizontalOffsets" );
		readEepromData( &rasterHorizontalOffsets, 2, sizeof(rasterHorizontalOffsets) );
	}
	else
	{
		for( uint8_t i = 0; i < kNumMirrors; ++i )
		{
			rasterHorizontalOffsets[i] = 32;
		}
	}
	rasterHorizontalOffsetVersion = CURRENT_HORIZONTAL_RASTER_VERSION;
}

static const uint8_t kNumFramesToEstablishSync = 8;
static const uint8_t kNumFramesToEstablishLostSync = 4;
static uint8_t numFramesInSync = 0;
static uint8_t numFramesNotInSync = 0;

static void setIsSynchronised()
{
	if( numFramesInSync < kNumFramesToEstablishSync)
	{
		++numFramesInSync;
	}
	numFramesNotInSync = 0;
}

static void setIsNotSynchronised()
{
	if( numFramesInSync == kNumFramesToEstablishSync)
	{
		if( numFramesNotInSync < kNumFramesToEstablishLostSync)
		{
			++numFramesNotInSync;
		}
		if( numFramesNotInSync == kNumFramesToEstablishLostSync)
		{
			numFramesInSync = 0;
		}
	}
	else
	{
		numFramesInSync = 0;
	}
}

static bool getIsSynchronised()
{
	return numFramesInSync == kNumFramesToEstablishSync;
}

static void turnLedOn()
{
	digitalWrite( LED_PIN, HIGH );
}
static void turnLedOff()
{
	digitalWrite( LED_PIN, LOW );
}

static void turnLaserOn()
{
	digitalWrite( LASER_PIN, HIGH );
}
static void turnLaserOff()
{
	digitalWrite( LASER_PIN, LOW );
}

inline void writePixel( uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t bitIdx )
{
	uint8_t pins;
	pins  = (byte0 >> bitIdx) & 1;
	pins |= ((byte1 >> bitIdx) & 1) << 1;
	pins |= ((byte2 >> bitIdx) & 1) << 2;
	pins |= ((byte3 >> bitIdx) & 1) << 3;
	PORTB = (PORTB & 0xf0) | pins;
	//digitalWrite( LASER_PIN, (byte >> bitIdx) & 1 );
}

inline void shortDelay( uint16_t count )
{
	for( uint16_t i = 0; i < count; ++i )
	{
		__asm__("nop\n\t" "nop\n\t");
	}
}

uint16_t interByteDelayCount = 15;

// Empirically determined difference between inter-byte and inter-bit delay counts
// to make the time between the last bit of one byte and the first bit of the next
// byte equal.
static const uint16_t kInterByteDelayCountDifference = 5;

static const uint16_t kMinInterByteDelayCount = 0;
static const uint16_t kMaxInterByteDelayCountLog2 = 8;
static const uint16_t kMaxInterByteDelayCount = 1 << kMaxInterByteDelayCountLog2;

// Empirically measured in microseconds using measureDelayCounts()
static const uint16_t kMinDelayCountHScanDuration = 1690;
static const uint16_t kMaxDelayCountHScanDuration = 20257;

MicroSeconds hScanInterval = 3000;
MicroSeconds hScanDuration = 2000;

static bool enableHScanTiming = true;
static Ticks nextScanTime = 0;
static Ticks nextScanTimeAdjusted = 0;
static Ticks nextRevolutionStartTime = 0;

void calcHorizontalScanDelays()
{
	// Calculate inter-byte delay count values for a desired scan duration
	if( hScanDuration <= kMinDelayCountHScanDuration )
	{
		interByteDelayCount = kMinInterByteDelayCount;
	}
	else if( hScanDuration >= kMaxDelayCountHScanDuration )
	{
		interByteDelayCount = kMaxInterByteDelayCount;
	}
	else
	{
		interByteDelayCount = (uint16_t) ((unsigned long) ((hScanDuration - kMinDelayCountHScanDuration) << kMaxInterByteDelayCountLog2) / (kMaxDelayCountHScanDuration - kMinDelayCountHScanDuration));
	}
}

inline void interBitDelay() { shortDelay( interByteDelayCount + kInterByteDelayCountDifference ); }
inline void interByteDelay() { shortDelay( interByteDelayCount ); }

// Do a single horizontal scan.
static void horizontalScan( uint8_t scanLineIdx )
{
	//MicroSeconds startTime = micros();
	uint8_t* pByte = gfx.getBuffer() + ((scanLineIdx+1) * kWidthBytes);
	for( int8_t x = kWidthBytes-1; x >= 0; --x )
	{
		--pByte;
		uint8_t byte0 = *pByte;
		uint8_t byte1 = *(pByte + kLaserByteOffset);
		uint8_t byte2 = *(pByte + (kLaserByteOffset*2));
		uint8_t byte3 = *(pByte + (kLaserByteOffset*3));
		writePixel( byte0, byte1, byte2, byte3, 0 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 1 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 2 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 3 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 4 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 5 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 6 );
		interBitDelay();
		writePixel( byte0, byte1, byte2, byte3, 7 );
		interByteDelay();
	}
	writePixel( 0,0,0,0, 0 );
#if 0
	MicroSeconds duration = micros() - startTime;
	MicroSeconds durationError = duration - hScanDuration;
	if( enableHScanTiming && (abs(durationError) > 200) )
	{
		Serial.print( "H: " );
		Serial.print( duration );
		Serial.print( ", " );
		Serial.println( hScanDuration );
	}
#endif
}

// Use this to establish 
void measureDelayCounts()
{
	// Measure minumum delay
	enableHScanTiming = false;
	interByteDelayCount = kMinInterByteDelayCount;
	uint16_t start = micros();
	for( uint8_t i = 0; i < kNumMirrors; ++i )
	{
		horizontalScan( i );
	}
	unsigned long duration = micros() - start;
	Serial.print( "kMinDelayCountHScanDuration = ");
	Serial.println( duration / kNumMirrors );
	// Measure maximum delay
	interByteDelayCount = kMaxInterByteDelayCount;
	start = micros();
	for( uint8_t i = 0; i < kNumMirrors; ++i )
	{
		horizontalScan( i );
	}
	duration = micros() - start;
	Serial.print( "kMaxDelayCountHScanDuration = ");
	Serial.println( duration / kNumMirrors );
	// Try for a scan 3000us
	hScanDuration = 3000;
	calcHorizontalScanDelays();
	start = micros();
	for( uint8_t i = 0; i < kNumMirrors; ++i )
	{
		horizontalScan( i );
	}
	duration = micros() - start;
	enableHScanTiming = true;
	Serial.print( "calcHorizontalScanDelays(3000) = ");
	Serial.println( duration / kNumMirrors );
	Serial.print( "interByteDelayCount = ");
	Serial.println( interByteDelayCount );
}

void delayMicroSeconds( MicroSeconds duration )
{
	static const MicroSeconds kLargeCountTime = 2426;
	static const MicroSeconds kSmallCountTime = 38;
	static const uint16_t kSmallCount = 64;
	static const uint16_t kLargeCount = 4096 + kSmallCount;
	long count = 64 + ((duration - kSmallCountTime) << 12) / (kLargeCountTime - kSmallCountTime);
	shortDelay( count );
}

void measureShortDelay()
{
	Ticks start = GetClockMain();
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	shortDelay(64);
	Ticks duration = (GetClockMain() - start) >> (3 + 1);  // +1 to convert to us
	Serial.print( "shortDelay(64) = " );
	Serial.println( duration );
	start = micros();
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	shortDelay(4096 + 64);
	duration = (GetClockMain() - start) >> (3 + 1);  // +1 to convert to us
	Serial.print( "shortDelay(4096) = " );
	Serial.println( duration );
	
	// Test
	start = micros();
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	delayMicroSeconds( 50 );
	duration = (GetClockMain() - start) >> (3 + 1);  // +1 to convert to us
	Serial.print( "delayMicroSeconds(50) = " );
	Serial.println( duration );
}

void dumpDisplayToTTY()
{
	cli();
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
	sei();
}

static uint8_t fillScanIdx = kNumMirrors-1;
void fillNextScan()
{
	gfx.fillRect( 0, fillScanIdx, kWidth, 1, 0 );
	if( ++fillScanIdx == kNumMirrors )
	{
		fillScanIdx = 0;
	}
	gfx.fillRect( 0, fillScanIdx, kWidth, 1, 1 );
	Serial.println( fillScanIdx );
}

static uint8_t calibrationScanIdx = 0;

#define PUSH_BUTTON( field ) input.field = inputState.field && !previousInputState.field
void checkButtons()
{
	// Read the buttons
	InputState inputState;
	inputState.m_redButton = (digitalRead( RED_BUTTON_PIN ) == LOW);
	inputState.m_blueButton = (digitalRead( BLUE_BUTTON_PIN ) == LOW);
	inputState.m_whiteButton = (digitalRead( WHITE_BUTTON_PIN ) == LOW);

	// Modify the push-button inputs so they only appear as set the first frame
	// they're pressed
	InputState input = inputState;
	PUSH_BUTTON( m_redButton );
	PUSH_BUTTON( m_blueButton );
	PUSH_BUTTON( m_whiteButton );
	previousInputState = inputState;

#if 1
	if( input.m_redButton )
	{
		rasterHorizontalOffsets[calibrationScanIdx]+=4;
	}
	if( input.m_blueButton )
	{
		rasterHorizontalOffsets[calibrationScanIdx]-=4;
	}
	if( input.m_whiteButton )
	{
		if( ++calibrationScanIdx == kNumMirrors )
		{
			calibrationScanIdx = 0;
			writeEepromData( &rasterHorizontalOffsetVersion, 0, 2 );
			writeEepromData( &rasterHorizontalOffsets, 2, sizeof(rasterHorizontalOffsets) );
		}
		Serial.print( "Scan: " );
		Serial.println( calibrationScanIdx );
	}
#else
	// Button tests
	if( input.m_redButton )
	{
		//turnLaserOn();
		// pwmCompare += 1;
		// Serial.println( pwmCompare );
		// OCR2B = pwmCompare;
		
		firstMirrorOffset += 5;
		Serial.println( firstMirrorOffset );
	}
	if( input.m_blueButton )
	{
		// pwmCompare -= 1;
		// Serial.println( pwmCompare );
		// OCR2B = pwmCompare;
		firstMirrorOffset -= 5;
		Serial.println( firstMirrorOffset );
	}
	if( input.m_whiteButton )
	{
		//measureShortDelay();
		turnLaserOn();
		fillNextScan();
		//dumpDisplayToTTY();
		//measureDelayCounts();
	}
#endif
}

void doSomeWork()
{
	// Do some reading of the serial port, and updating of the bitmap etc.
	//fillNextScan();
}

void calcNextRevolutionSettings( bool expectData )
{
	if( drumSyncTimePosted )
	{
		cli();
		if( drumSyncTimePosted > 1 )
		{
			//Serial.println( drumSyncTimePosted );
			// Correct for syncing up to a multiple of the actual frequency
			previousDrumSyncTime -= (actualSyncTime - previousDrumSyncTime) * (drumSyncTimePosted - 1);
		}
		drumSyncTimePosted = 0;
		Ticks drumRevolutionDurationTicks = actualSyncTime - previousDrumSyncTime;
		previousDrumSyncTime = actualSyncTime;
		if( abs( drumRevolutionDurationTicks - previousDrumRevolutionDurationTicks ) < 1000 )
		{
			setIsSynchronised();
		}
		else
		{
			setIsNotSynchronised();
		}
		previousDrumRevolutionDurationTicks = drumRevolutionDurationTicks;

		Ticks delayToFirstMirror = (drumRevolutionDurationTicks * (unsigned long) firstMirrorOffset) >> 12;
		nextRevolutionStartTime = actualSyncTime + delayToFirstMirror;
		if( nextRevolutionStartTime < GetClockMain() )
		{
			Serial.println("V");
		}

		// Calculate desired horizontal scan time, and from that
		// the inter-bit and inter-byte delays
		hScanInterval = TicksToMicroSeconds(drumRevolutionDurationTicks) / kNumMirrors;
		hScanDuration = (hScanInterval * 2) >> 2; // We'll draw for 1/2 of the hScanDuration
		calcHorizontalScanDelays();

		//turnLedOn();
		sei();

#if 0
		if( !expectData )
		{
			// We're not currently doing any scanning, we're just trying to get back in sync
			// so we should have plenty of time for some spam....
			Ticks now = GetClockMain();
			Serial.print( "Now: " );
			Serial.println( now );
			Serial.print( "M0: " );
			Serial.println( nextRevolutionStartTime - now );
			Serial.print( "T: " );
			Serial.println( actualSyncTime );
			Serial.print( "D: " );
			Serial.println( drumRevolutionDurationTicks );
		}
#endif
	}
	else if( expectData )
	{
		Serial.println( "X" );
	}
}

#if 0
void Update()
{
	Serial.println( digitalRead( 2 ) );
}
#else
void Update()
{
	sei();
	if( getIsSynchronised() )
	{
		Ticks timeToNextScan = nextScanTimeAdjusted - GetClockMain();
		//if( timeToNextScan > 300 )
		{
			checkButtons();
		}
#if 0
		if( timeToNextScan > 1400 )
		{
			doSomeWork();
		}
		else if( timeToNextScan > 300 )
		{
			//checkButtons();
			Ticks timeToNextScan = nextScanTimeAdjusted - GetClockMain();
			if( timeToNextScan < 50 )
			{
				Serial.println("Y");
			}
		}
		else if( timeToNextScan > 0 )
		{

			if( timeToNextScan < 50 )
			{
				Serial.println("Y");
			}
#endif
		if( timeToNextScan > 0 )
		{
			// Spin until the time is right to draw the next scan-line
			while( (nextScanTimeAdjusted - GetClockMain()) > 0 ){}
			// Spit out a single scan-line
			if( currentMirrorIdx < kNumMirrors )
			{
				horizontalScan( mirrorToRaster[currentMirrorIdx] );
			}
			// Establish the start time for the next scan
			nextScanTime += MicroSecondsToTicks(hScanInterval);
			if( ++currentMirrorIdx == kNumMirrors )
			{
				// We've finished all the scanlines.
				// Update all our timings and set things up so we start
				// the first scanline of the next rotation at the right time.
				calcNextRevolutionSettings( true );
				currentMirrorIdx = 0;
				nextScanTime = nextRevolutionStartTime;
			}
			// Adjust the scan-line horizontally according to the calibration data.
			nextScanTimeAdjusted = nextScanTime + rasterHorizontalOffsets[ mirrorToRaster[currentMirrorIdx] ];
		}
		else
		{
			Serial.println("Z");
			Serial.print( "Now: " );
			Serial.println( GetClockMain() );
			Serial.print( "TTNS: " );
			Serial.println( timeToNextScan );
			setIsNotSynchronised();
		}
	}
	else
	{
		// Not synchronised
		//Serial.println("Z");
		GetClockMain();
		checkButtons();
		calcNextRevolutionSettings( getIsSynchronised() );
		nextScanTime = nextRevolutionStartTime;
		nextScanTimeAdjusted = nextScanTime;
		currentMirrorIdx = 0;
		turnLedOn();
		turnLaserOn();
	}
}
#endif

void MirrorDrumInterrupt()
{
	cli();
	Ticks now = GetClockInterrupt();
	if( (now - actualSyncTime) > 10000 )
	{
		actualSyncTime = now;
		++drumSyncTimePosted;
	}
	sei();
}

