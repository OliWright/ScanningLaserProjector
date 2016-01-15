#include "ColourArray.h"

const Colour Colour::kBlack(0,0,0);
const Colour Colour::kWhite(255,255,255);

void ColourArray::Clear()
{
	for( unsigned int i = 0; i < kNumPixels; ++i )
	{
		m_array[i].Clear();
	}
}

void BrightnessArray::Clear()
{
	for( unsigned int i = 0; i < kNumPixels; ++i )
	{
		m_array[i] = 0;
	}
}
