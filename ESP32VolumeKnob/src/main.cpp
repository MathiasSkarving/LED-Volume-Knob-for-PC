#include <Arduino.h>
#include "USB.h"
#include "USBHIDConsumerControl.h"
#include <FastLED.h>

#define LED_RING_DATA 16
#define SWITCH 15
#define DT 17
#define CLK 18
#define NUM_LEDS 12

// For counter
float counter = 0;
uint8_t intCounter = 0;
uint8_t lastCLK = 0;
volatile int encoderDelta = 0;
unsigned long lastDecay = 0;

// For switch
int lastSwitchModeTime = 0;

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
}

void volumeDown()
{
  ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
}

void mute()
{
  ConsumerControl.press(CONSUMER_CONTROL_MUTE);
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
      leds[i] = CHSV(0, map(i, 0, 11, 100, 255), 150);
    }
  }
  else if (counter < 0)
  {
    for (int i = 0; i < ledCount; i++)
    {
      leds[NUM_LEDS - 1 - i] = CHSV(0, map(i, 0, 11, 100, 255), 150);
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
    leds[i] = CHSV((i * 256 / NUM_LEDS) + offset, 255, 150);
  }

  FastLED.show();
}

void readSwitch()
{
  if ((digitalRead(SWITCH) == LOW))
  {
    // Debounce
    delayMicroseconds(200);
    bool currentState = digitalRead(SWITCH);
    int start = millis();

    while (currentState == LOW)
    {
      if (digitalRead(SWITCH) == HIGH && (millis() - lastSwitchModeTime > 1000))
      {
        mute();
        return;
      }

      if (millis() - start > 1000)
      {
        ledMode++;
        if (ledMode > 2)
        {
          ledMode = 0;
        }
        animateModeChange(ledMode);
        Serial.print("Selected mode ");
        Serial.println(ledMode);
        lastSwitchModeTime = millis();
        return;
      }
    }
  }
}

void readEncoder()
{
  int clk = digitalRead(CLK);

  if (clk != lastCLK)
  {
    if (digitalRead(DT) != clk)
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

  lastCLK = clk;

  if (millis() - prev > 750 && millis() - lastDecay > 10)
  {
    lastDecay = millis();
    if (counter > 0.5)
    {
      counter -= 0.5;
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

  lastCLK = digitalRead(CLK);

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
  default:
  {
    ledMode = 0;
    break;
  }
  }
}