#include <Arduino.h>
#include "USB.h"
#include "USBHIDConsumerControl.h"
#include <FastLED.h>
#include <ArduinoJson.h>
#include <RotaryEncoder.h>

#define LED_RING_DATA 16
#define SWITCH 15
#define DT 17
#define CLK 18
#define NUM_LEDS 12

enum ButtonState
{
  IDLE,
  PRESSED,
  WAITING_FOR_DOUBLE_CLICK,
  WAITING_FOR_TRIPLE_CLICK,
  WAITING_FOR_TRIPLE_TIMEOUT,
  LONG_PRESSED,
  WAITING_FOR_RELEASE
};

// Encoder
RotaryEncoder encoder(CLK, DT, RotaryEncoder::LatchMode::TWO03);

// For counter
float counter = 0;
float counterLimit = 30;
int lastPos = 0;
int lastDecay = 0;

// For switch
long pressStartTime = 0;
long doublePressStartTime = 0;
long lastLongPress = 0;
ButtonState buttonState = IDLE;

// For LEDS
int activeLeds;
unsigned long prev;
CRGB leds[NUM_LEDS];
int ledMode = 0;
int lastCycle = 0;
int offset = 0;

USBHIDConsumerControl ConsumerControl;

void animateModeChange(int mode)
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(100);

  CRGB color = CHSV(0, 0, 100);

  for (int i = 0; i < 3; i++)
  {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
}

void volumeUp()
{
  ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
  delay(2);
  ConsumerControl.release();
}

void volumeDown()
{
  ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
  delay(2);
  ConsumerControl.release();
}

void mute()
{
  ConsumerControl.press(CONSUMER_CONTROL_MUTE);
  delay(2);
  ConsumerControl.release();
}

void nextSong()
{
  ConsumerControl.press(CONSUMER_CONTROL_SCAN_NEXT);
  delay(2);
  ConsumerControl.release();
}

void previousSong()
{
  ConsumerControl.press(CONSUMER_CONTROL_SCAN_PREVIOUS);
  delay(2);
  ConsumerControl.release();
}

void dynamicSolidRainbow()
{
  int ledCount = constrain(abs(counter) / 2.0, 0, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  if (counter > 0)
  {
    for (int i = 0; i < ledCount; i++)
    {
      leds[i] = CHSV(map(ledCount, 0, 12, 0, 310), 255, 255);
    }
  }
  else if (counter < 0)
  {
    for (int i = 0; i < ledCount; i++)
    {
      leds[NUM_LEDS - 1 - i] = CHSV(map(ledCount, 0, 12, 0, 310), 255, 255);
    }
  }

  FastLED.show();
}

void dynamicBlood()
{
  int ledCount = constrain(abs(counter) / 2.0, 0, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  if (counter > 0)
  {
    for (int i = 0; i < ledCount; i++)
    {
      leds[i] = CHSV(0, map(i, 0, 11, 100, 255), 255);
    }
  }
  else if (counter < 0)
  {
    for (int i = 0; i < ledCount; i++)
    {
      leds[NUM_LEDS - 1 - i] = CHSV(0, map(i, 0, 11, 100, 255), 255);
    }
  }
  FastLED.show();
}

void rainbowCycle()
{
  if (millis() - lastCycle > 20)
  {
    offset++;
    lastCycle = millis();
  }

  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV((i * 256 / NUM_LEDS) + offset, 255, 255);
  }

  FastLED.show();
}

void randomColors()
{
  if (millis() - lastCycle > 100)
  {
    lastCycle = millis();

    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CHSV(random(0, 255), 255, 255);
    }
  }

  FastLED.show();
}

void fireAnimation()
{
  if (millis() - lastCycle > 100)
  {
    lastCycle = millis();

    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CHSV(random(0, 45), 255, random(100, 255));
    }
  }

  FastLED.show();
}

void readSwitch()
{
  bool pressed = digitalRead(SWITCH) == LOW;

  switch (buttonState)
  {
  case IDLE:
    if (pressed)
    {
      pressStartTime = millis();
      buttonState = PRESSED;
    }
    break;

  case PRESSED:
    if (!pressed)
    {
      buttonState = WAITING_FOR_DOUBLE_CLICK;
    }
    else if (pressed && millis() - pressStartTime > 1000)
    {
      buttonState = LONG_PRESSED;
    }
    break;
  case LONG_PRESSED:
    ledMode++;
    if (ledMode > 4)
    {
      ledMode = 0;
    }
    animateModeChange(ledMode);
    lastLongPress = millis();
    buttonState = IDLE;
    break;
  case WAITING_FOR_DOUBLE_CLICK:
    if (pressed && millis() - pressStartTime < 250)
    {
      pressStartTime = millis();
      buttonState = WAITING_FOR_TRIPLE_CLICK;
    }
    else if (!pressed && millis() - lastLongPress > 1000 && millis() - pressStartTime > 250)
    {
      mute();
      buttonState = WAITING_FOR_RELEASE;
    }
    break;
  case WAITING_FOR_TRIPLE_CLICK:
    if (!pressed && millis() - pressStartTime < 250)
    {
      buttonState = WAITING_FOR_TRIPLE_TIMEOUT;
    }
    break;
  case WAITING_FOR_TRIPLE_TIMEOUT:
    if (pressed && millis() - pressStartTime < 250)
    {
      previousSong();
      buttonState = WAITING_FOR_RELEASE;
    }
    else if (millis() - pressStartTime > 250)
    {
      nextSong();
      buttonState = WAITING_FOR_RELEASE;
    }
    break;
  case WAITING_FOR_RELEASE:
    if (!pressed)
    {
      buttonState = IDLE;
    }
    break;
  }
}

void readEncoder()
{
  encoder.tick();
  int pos = encoder.getPosition();

  if (pos != lastPos)
  {
    if (encoder.getDirection() == RotaryEncoder::Direction::COUNTERCLOCKWISE)
    { 
      volumeUp();
      counter += 1;
    }
    else
    {
      volumeDown();
      counter -= 1;
    }
    prev = millis();
  }

  lastPos = pos;

  if (millis() - prev > 750 && millis() - lastDecay > 10)
  {
    lastDecay = millis();
    if (counter > 0.5)
    {
      counter -= 0.5;
    }
    else if (counter > counterLimit)
    {
      counter = counterLimit;
    }
    else if (counter < -counterLimit)
    {
      counter = -counterLimit;
    }
    else if (counter < -0.5)
    {
      counter += 0.5;
    }
    else
    {
      counter = 0;
    }
  }
}

void setup()
{
  USB.begin();
  ConsumerControl.begin();

  Serial.begin(9600);

  unsigned long start = millis();
  while (!Serial && millis() - start < 3000)
  {
    delay(10);
  }

  Serial.println("USB Serial Ready");

  FastLED.addLeds<WS2812B, LED_RING_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  FastLED.show();

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);

  Serial.println("Ready");
}

void loop()
{
  readEncoder();
  readSwitch();

  switch (ledMode)
  {
  case 0:
  {
    rainbowCycle();
    break;
  }
  case 1:
  {
    dynamicBlood();
    break;
  }
  case 2:
  {
    dynamicSolidRainbow();
    break;
  }
  case 3:
  {
    fireAnimation();
    break;
  }
  case 4:
  {
    randomColors();
    break;
  }
  default:
  {
    ledMode = 0;
    break;
  }
  }
}