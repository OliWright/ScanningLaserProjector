#ifndef SCANNING_LASER_PROJECTOR_H
#define SCANNING_LASER_PROJECTOR_H

#include "ColourArray.h"
#include "Timer.h"
#include <Adafruit_GFX.h>

#define LED_PIN   13
#define LASER_PIN 13

void Startup();

extern GFXcanvas1 gfx;

struct InputState
{
	bool         m_redButton;
	bool         m_blueButton;
	MicroSeconds m_mostRecentSyncTime;
	unsigned int m_measuredRevsPerSecond;
};

const ColourArray& Update( const InputState& inputs );

#endif