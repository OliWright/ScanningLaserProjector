#include "ScanningLaserProjector.h"

static const Colour kRed( 255, 60, 30 );
static const Colour kGreen( 50, 255, 50 );
static const Colour kBlue( 20, 40, 255 );
static const Colour kDim( 32, 32, 32 );

static ColourArray colourArray;
static InputState previousInputState;

void Startup()
{
	memset( &previousInputState, 0, sizeof( previousInputState ) );
}

#define LATCH( field ) input.field = inputState.field && !previousInputState.field

const ColourArray& Update( const InputState& inputState )
{
	// Latch the inputs
	InputState input;
	LATCH( m_fasterButton );
	LATCH( m_slowerButton );
	previousInputState = inputState;


	colourArray.Clear();
	for( unsigned int i = 0; i < (unsigned int) inputState.m_measuredRevsPerSecond; ++i )
	{
		colourArray[i] = kDim;
	}
	if( inputState.m_fasterButton )
	{
		colourArray[0] = kRed;
	}
	if( inputState.m_slowerButton )
	{
		colourArray[10] = kBlue;
	}

	return colourArray;
}
