#include <Adafruit_TinyUSB.h>
#include <FastLED.h>

#define LED_RING_DATA 4
#define SWITCH 16
#define DT 17
#define CLK 18
#define NUM_LEDS 12

// For counter
float counter = 0;
uint8_t intCounter = 0;
uint8_t lastclk;
volatile int interruptDelta = 0;
unsigned long lastDecay = 0;

// For switch
int lastPress;
bool justPressed = false;
volatile bool switchTriggered = false;

// For LEDS
int activeLeds;
unsigned long prev;
CRGB leds[NUM_LEDS];
int ledMode = 0;
int lastCycle = 0;
int offset = 0;

// For PC Volume
enum {
  RID_CONSUMER_CONTROL = 1,
};

// HID report descriptor using TinyUSB's template
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER_CONTROL))
};

Adafruit_USBD_HID usb_hid;

void IRAM_ATTR encoderISR() {
  uint8_t clkState = digitalRead(CLK);
  uint8_t dtState = digitalRead(DT);

  if (dtState != clkState) {
    interruptDelta++;
  } else {
    interruptDelta--;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Volume Knob");
  usb_hid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(100);
    TinyUSBDevice.attach();
  }

  Serial.print("Waiting for USB to mount...");
  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
  Serial.println(" Mounted!");

  FastLED.addLeds<NEOPIXEL, LED_RING_DATA>(leds, NUM_LEDS);

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);

  lastclk = digitalRead(CLK);

  attachInterrupt(digitalPinToInterrupt(CLK), encoderISR, CHANGE);

  Serial.println("Ready");
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif

  Serial.println(ledMode);
  readEncoder();
  readSwitch();

  switch (ledMode) {
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
        ledMode = 1;
        break;
      }
  }
}

void dynamicSolidRainbow() {
  int ledCount = constrain(abs(counter) / 3.0, 0, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);


  if (counter > 0) {
    for (int i = 0; i < ledCount; i++) {
      leds[i] = CHSV(map(ledCount, 0, 12, 0, 310), 255, 255);
    }
  } else if (counter < 0) {
    for (int i = 0; i < ledCount; i++) {
      leds[NUM_LEDS - 1 - i] = CHSV(map(ledCount, 0, 12, 0, 310), 255, 255);
    }
  }

  FastLED.show();
}

void dynamicBlood() {
  int ledCount = constrain(abs(counter) / 3.0, 0, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  if (counter > 0) {
    for (int i = 0; i < ledCount; i++) {
      leds[i] = CHSV(0, map(i, 0, 11, 100, 255), 150);
    }
  } else if (counter < 0) {
    for (int i = 0; i < ledCount; i++) {
      leds[NUM_LEDS - 1 - i] = CHSV(0, map(i, 0, 11, 100, 255), 150);
    }
  }
  FastLED.show();
}

void rainbowCycle() {
  if (millis() - lastCycle > 20) {
    offset++;
    lastCycle = millis();
  }

  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV((i * 256 / NUM_LEDS) + offset, 255, 150);
  }

  FastLED.show();
}

void readSwitch() {
  if ((digitalRead(SWITCH) == LOW)) {
    delayMicroseconds(200);
    bool currentState = digitalRead(SWITCH);

    if (currentState == LOW) {
      if (!justPressed) {
        justPressed = true;
        lastPress = millis();
      }

      if (millis() - lastPress > 1000) {
        ledMode++;
        if (ledMode > 2) {
          ledMode = 0;
        }
        animateModeChange(ledMode);
        Serial.print("Selected mode ");
        Serial.println(ledMode);

        justPressed = false;
        return;
      }
    } else {
      justPressed = false;
      return;
    }
  } else {
    if (justPressed) {
      if (millis() - lastPress < 1000) {
        mute();
      }
      justPressed = false;
    }
  }
}

void readEncoder() {
  if (interruptDelta != 0) {
    noInterrupts();
    int movement = interruptDelta;
    interruptDelta = 0;
    interrupts();

    if (movement > 0) {
      counter++;
      intCounter++;
      if (intCounter % 2 == 0) {
        volumeUp();
      }
    } else {
      counter--;
      intCounter--;
      if (intCounter % 2 == 0) {
        volumeDown();
      }
    }

    prev = millis();
  }

  if (millis() - prev > 750 && millis() - lastDecay > 10) {
    lastDecay = millis();
    if (counter > 0.5) {
      counter -= 0.5;
    } else if (counter < -0.5) {
      counter += 0.5;
    } else {
      counter = 0;
    }
  }
}

void sendConsumerKey(uint16_t key) {
  if (!usb_hid.ready()) {
    return;
  }
  uint16_t report = key;
  usb_hid.sendReport(RID_CONSUMER_CONTROL, &report, sizeof(report));
  delay(10);
  report = 0;
  usb_hid.sendReport(RID_CONSUMER_CONTROL, &report, sizeof(report));
  delay(2);
}

void animateModeChange(int mode) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(100);

  CRGB color = CHSV(0, 0, 100);

    for (int i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
}

void volumeUp() {
  sendConsumerKey(HID_USAGE_CONSUMER_VOLUME_INCREMENT);
}

void volumeDown() {
  sendConsumerKey(HID_USAGE_CONSUMER_VOLUME_DECREMENT);
}

void mute() {
  sendConsumerKey(HID_USAGE_CONSUMER_MUTE);
}
