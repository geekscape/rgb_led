/*
 * File:    rgb_led.ino
 * Version: 0.0
 * Author:  Andy Gelme (@geekscape)
 * License: GPLv3
 */

#include <math.h>
#include <TimerOne.h>

#include "rgb_led.h"

const byte PIN_LED_CLOCK = 7;
const byte PIN_LED_DATA  = 8;

const byte LED_COUNT = 16;
rgb_t led[LED_COUNT];

const long BLEND_RATE = 100;  // milliseconds

byte  blendFlag = false;
long  blendDoneTime = 0;
long  blendNextTime = 0;
rgb_t ledDelta[LED_COUNT];
rgb_t ledTarget[LED_COUNT];

int ledOffset = 0;  // Adjust starting point for using led[] array for output

rgb_t colors[] = {
  RED, ORANGE, YELLOW, GREEN, BLUE, PINK, PURPLE, WHITE
};

const byte COLORS_COUNT = sizeof(colors) / sizeof(rgb_t);

void setup() {
  pinMode(PIN_LED_DATA,  OUTPUT);
  pinMode(PIN_LED_CLOCK, OUTPUT);

  ledSetAll(BLACK);

  Timer1.initialize(30000);
  Timer1.attachInterrupt(ledHandler);
}

void loop() {
  ledSetAll(BLACK);

// Pulse a sequence of colors
// --------------------------
  for (byte index = 0;  index < COLORS_COUNT;  index ++) {
    ledPulse(colors[index], 1000);
  }

// Create pattern of dark through to bright RED LEDs
// Shift (rotate) the pattern along the line of LEDs
// -------------------------------------------------
  ledFadeUp(RED);

  for (byte count = 0;  count < 128;  count ++) {
    ledOffset = (ledOffset + 1) % LED_COUNT;
    delay(25);
  }
  ledOffset = 0;

// Blend from one sequence of random LED colors to the next
// --------------------------------------------------------
  for (byte count = 0;  count < 16;  count ++) {
    for (byte index = 0;  index < LED_COUNT;  index ++) {
      ledTarget[index] =
        RGB(random(0, 2) * 255, random(0, 2) * 255, random(0, 2) * 255);
    }

    blend(1000);
    delay(1000); // Wait for blend to complete
  }
}

void ledSet(
  byte  index,  // LED number
  rgb_t rgb) {  // RGB color

  noInterrupts();
  led[index].color[RED_INDEX]   = rgb.color[RED_INDEX];
  led[index].color[GREEN_INDEX] = rgb.color[GREEN_INDEX];
  led[index].color[BLUE_INDEX]  = rgb.color[BLUE_INDEX];
  interrupts();
}

void ledSetAll(
  rgb_t rgb) {  // RGB color

  for (byte count = 0;  count < LED_COUNT;  count ++) ledSet(count, rgb);
}

const float FADE_FACTOR    = 10000000.0;
const float FADE_INCREMENT = (FADE_FACTOR - 1.0) / (LED_COUNT - 1);
const float FADE_SCALE     = log(FADE_FACTOR);

void ledFadeUp(
  rgb_t rgb) {  // RGB color

  float factor = FADE_FACTOR;
  float previous = log(factor);
  float multiplier = 0.0;

  for (int count = 0;  count < LED_COUNT;  count ++) {
    float current = log(factor);
    multiplier += (previous - current);
    previous = current;
    factor -= FADE_INCREMENT;

    float red   = rgb.color[RED_INDEX]   * multiplier / FADE_SCALE;
    float green = rgb.color[GREEN_INDEX] * multiplier / FADE_SCALE;
    float blue  = rgb.color[BLUE_INDEX]  * multiplier / FADE_SCALE;

    ledSet(count, RGB(red, green, blue));
  }
}

void ledPulse(
  rgb_t rgb,      // RGB color
  int   speed) {  // Pulse time in milliseconds

  int red   = 0;
  int green = 0;
  int blue  = 0;
  int redDelta   = (rgb.color[RED_INDEX]   << 7) / 256;
  int greenDelta = (rgb.color[GREEN_INDEX] << 7) / 256;
  int blueDelta  = (rgb.color[BLUE_INDEX]  << 7) / 256;

  for (int count = 0;  count < 512;  count ++) {
    ledSetAll(RGB(red >> 7, green >> 7, blue >> 7));
    red   += redDelta;
    green += greenDelta;
    blue  += blueDelta;

    if (count == 255) {
      redDelta   = -redDelta;
      greenDelta = -greenDelta;
      blueDelta  = -blueDelta;
    }

    delayMicroseconds(speed);
  }
}

void blend(
  long blendSpeed) {  // Blend time in milliseconds

  for (byte count = 0;  count < LED_COUNT;  count ++) {
    ledDelta[count].color[RED_INDEX] =
      ((ledTarget[count].color[RED_INDEX] - led[count].color[RED_INDEX]) << 7) /
      (blendSpeed / BLEND_RATE);

    ledDelta[count].color[GREEN_INDEX] =
      ((ledTarget[count].color[GREEN_INDEX] - led[count].color[GREEN_INDEX]) << 7) /
      (blendSpeed / BLEND_RATE);

    ledDelta[count].color[BLUE_INDEX] =
      ((ledTarget[count].color[BLUE_INDEX] - led[count].color[BLUE_INDEX]) << 7) /
      (blendSpeed / BLEND_RATE);
  }

  blendNextTime = millis();
  blendDoneTime = blendNextTime + blendSpeed;
  blendFlag = true;
}

int blender(
  int value,
  int delta) {

  return(((value << 7) + delta) >> 7);
}

// Output led[] array to the strip of WS2801 RGB LEDs
// Every BLEND_RATE milliseconds, blend LEDs toward target colors

void ledHandler(void) {
  for (byte count = 0;  count < LED_COUNT;  count ++) {
    byte index = (ledOffset + count) % LED_COUNT;

    unsigned long color = led[index].color[BLUE_INDEX];
    color |= (unsigned long) led[index].color[GREEN_INDEX] << 8;
    color |= (unsigned long) led[index].color[RED_INDEX]   << 16;

    for (byte bit = 0;  bit < 24;  bit ++) {
      digitalWrite(PIN_LED_CLOCK, LOW);
      digitalWrite(PIN_LED_DATA,  (color & (1L << 23)) ? HIGH : LOW);
      digitalWrite(PIN_LED_CLOCK, HIGH);
      color <<= 1;
    }
  }

  digitalWrite(PIN_LED_CLOCK, LOW);  // Must stay low for 500 microseconds

  if (blendFlag) {
    long timeNow = millis();

    if (timeNow > blendNextTime) {
      blendNextTime = timeNow + BLEND_RATE;

      for (byte index = 0;  index < LED_COUNT;  index ++) {
        led[index].color[RED_INDEX] = blender(
          led[index].color[RED_INDEX], ledDelta[index].color[RED_INDEX]
        );
        led[index].color[GREEN_INDEX] = blender(
          led[index].color[GREEN_INDEX], ledDelta[index].color[GREEN_INDEX]
        );
        led[index].color[BLUE_INDEX] = blender(
          led[index].color[BLUE_INDEX], ledDelta[index].color[BLUE_INDEX]
        );
      }
    }

    if (timeNow >= blendDoneTime) blendFlag = false;
  }
}
