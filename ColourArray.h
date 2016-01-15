#ifndef COLOUR_ARRAY_H
#define COLOUR_ARRAY_H

#include <Arduino.h>

// How many NeoPixels are attached to the Arduino?
static const unsigned int kNumPixels = 150;

typedef uint8_t ColourComponent;
typedef uint8_t Brightness;
typedef uint16_t PixelIdx;

struct Colour
{
	Colour()
	{
		m_components[0] = 0;
		m_components[1] = 0;
		m_components[2] = 0;
	}

	Colour( ColourComponent r, ColourComponent g, ColourComponent b )
	{
		m_components[0] = r;
		m_components[1] = g;
		m_components[2] = b;
	}

	ColourComponent R() const { return m_components[0]; }
	ColourComponent G() const { return m_components[1]; }
	ColourComponent B() const { return m_components[2]; }

	void Clear() { *this = kBlack; }

	const Colour& operator = ( const Colour& rhs )
	{
		m_components[0] = rhs.m_components[0];
		m_components[1] = rhs.m_components[1];
		m_components[2] = rhs.m_components[2];
	}

	static const Colour kBlack;
	static const Colour kWhite;

	ColourComponent m_components[3];
};

struct BrightnessArray
{
	BrightnessArray()
	{
		Clear();
	}

	void Clear();

	Brightness operator [] (unsigned int idx ) const { return m_array[ idx ]; }
	Brightness& operator [] (unsigned int idx ) { return m_array[ idx ]; }

	Brightness m_array[ kNumPixels ];
};

struct ColourArray
{
	ColourArray()
	{}

	const Colour& operator [] (unsigned int idx ) const { return m_array[ idx ]; }
	Colour& operator [] (unsigned int idx ) { return m_array[ idx ]; }

	void Clear();

	Colour m_array[ kNumPixels ];
};

#endif