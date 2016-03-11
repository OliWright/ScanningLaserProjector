#include "ScanningLaserProjector.h"

#include <Arduino.h>

// This must be on a pin that has interrupt capability
#define DRUM_ROTATION_SIGNAL_PIN 2

void setup()
{
	// Initialize serial communications (if there is a PC attached) at 57600 bps:
	Serial.begin(115200);
	randomSeed(analogRead(0));

	pinMode( RED_BUTTON_PIN, INPUT_PULLUP );
	pinMode( BLUE_BUTTON_PIN, INPUT_PULLUP );
	pinMode( WHITE_BUTTON_PIN, INPUT_PULLUP );
	pinMode( DRUM_ROTATION_SIGNAL_PIN, INPUT_PULLUP);

	pinMode( LED_PIN, OUTPUT );
	pinMode( LASER_PIN, OUTPUT );
	pinMode( LASER_PIN, OUTPUT );

	pinMode( DRUM_PWM_PIN, OUTPUT );

	attachInterrupt( digitalPinToInterrupt(DRUM_ROTATION_SIGNAL_PIN), MirrorDrumInterrupt, RISING);

	// Startup the app
	Startup();
}

void loop()
{
	// Update the ScanningLaserProjector
	Update();
}
