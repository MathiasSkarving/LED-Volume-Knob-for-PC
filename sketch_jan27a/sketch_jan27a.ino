#include <FastLED.h>

#define LED_RING_DATA 4
#define SWITCH 16
#define DT 17
#define CLK 18
#define NUM_LEDS 12

// For counter
volatile int counter = 0;
volatile int lastCounter = 0;
volatile uint8_t lastclk = 0;

// For speed
unsigned long now;
unsigned long prev;
float deltaCount;
float deltaTime;
float speed;
int lastDir;

// For LEDS
int activeLeds;

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    delay(100);
    Serial.println("Waiting for serial");
  }

  lastclk = digitalRead(CLK);

  FastLED.addLeds<NEOPIXEL, LED_RING_DATA>(leds, NUM_LEDS);  // GRB ordering is assumed

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);

  Serial.println("Ready");
}

void loop() {
  uint8_t clk = digitalRead(CLK);

  if (clk != lastclk) {
    uint8_t dt = digitalRead(DT);

    if (dt == clk) {
      counter++;
      lastDir = 1;
    } else {
      counter--;
      lastDir = -1;
    }

    if (counter % 2 == 0) {
      now = millis();
      deltaTime = now - prev;
      if (deltaTime > 0) {
        speed = (1000.0 / deltaTime) * lastDir;
      }
      prev = now;
    }

    Serial.println(speed);
    lastclk = clk;
  }

  activeLeds = constrain((speed + 200.0) * 12.0 / 400.0, 0, 12);

  for (int i = 0; i < activeLeds; ++i) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();

}
