#ifndef SCANNING_LASER_PROJECTOR_H
#define SCANNING_LASER_PROJECTOR_H

#include "Timer.h"
#include <Adafruit_GFX.h>

#define LED_PIN LED_BUILTIN

// 4 Lasers attached to the pins controlled by the lower nybble of PORTB
// Pins 8 - 11
#define LASER_PIN 8

#define RED_BUTTON_PIN 4
#define BLUE_BUTTON_PIN 5
#define WHITE_BUTTON_PIN 6

#define DRUM_PWM_PIN 3

void Startup();

extern GFXcanvas1 gfx;

struct InputState
{
	bool         m_redButton;
	bool         m_blueButton;
	bool         m_whiteButton;
};

void Update();

void MirrorDrumInterrupt();

#endif