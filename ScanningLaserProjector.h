#ifndef SCANNING_LASER_PROJECTOR_H
#define SCANNING_LASER_PROJECTOR_H

#include "ColourArray.h"

void Startup();

typedef unsigned long MicroSeconds;

struct InputState
{
	bool         m_fasterButton;
	bool         m_slowerButton;
	MicroSeconds m_mostRecentSyncTime;
	unsigned int m_measuredRevsPerSecond;
};

const ColourArray& Update( const InputState& inputs );

#endif