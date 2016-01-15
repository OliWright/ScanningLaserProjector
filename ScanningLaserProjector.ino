#include "ScanningLaserProjector.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// Which pin on the Arduino is connected to the NeoPixels?
#define NEOPIXELS_PIN 3

#define FASTER_PIN 4
#define SLOWER_PIN 5

#define FAN_INPUT_PIN 2

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(kNumPixels, NEOPIXELS_PIN, NEO_GRB + NEO_KHZ800);

static MicroSeconds mostRecentSyncTime = 0;
static MicroSeconds syncInterval = 0;
static uint8_t revsPerSecond = 0;

void fanInterrupt()
{
	MicroSeconds now = micros();
	syncInterval = now - mostRecentSyncTime;
	mostRecentSyncTime = now;

	// Calculate the frequency
	revsPerSecond = (MicroSeconds) 1000000 / syncInterval;
}

void setup()
{
	pixels.begin(); // This initializes the NeoPixel library.

	// Initialize serial communications (if there is a PC attached) at 57600 bps:
	Serial.begin(57600);
	randomSeed(analogRead(0));

	pinMode( FASTER_PIN, INPUT_PULLUP );
	pinMode( SLOWER_PIN, INPUT_PULLUP );
	pinMode( FAN_INPUT_PIN, INPUT_PULLUP);

	attachInterrupt( digitalPinToInterrupt(FAN_INPUT_PIN), fanInterrupt, RISING);

	// Startup the app
	Startup();
}

void loop()
{
	// Read the buttons
	InputState inputState;
	inputState.m_fasterButton = (digitalRead( FASTER_PIN ) == LOW);
	inputState.m_slowerButton = (digitalRead( SLOWER_PIN ) == LOW);
	inputState.m_measuredRevsPerSecond = revsPerSecond;
	inputState.m_mostRecentSyncTime = mostRecentSyncTime;

	// Update and get the ColourArray from ScanningLaserProjector
	const ColourArray& colourArray = Update( inputState );

	// Pass the colour array to the NeoPixels
	for( int i = 0; i < kNumPixels; i++ )
	{
		const Colour& colour = colourArray[i];
		pixels.setPixelColor( i, pixels.Color( colour.R(), colour.G(), colour.B() ) );
	}
	pixels.show();


	//Serial.println(revsPerSecond, DEC);
}
