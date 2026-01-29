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

// For PC Volume
enum {
  RID_KEYBOARD = 1,
  RID_MOUSE,
  RID_CONSUMER_CONTROL,
};

// HID report descriptor using TinyUSB's template
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE   (HID_REPORT_ID(RID_MOUSE)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER_CONTROL))
};

Adafruit_USBD_HID usb_hid;

void setup() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  
  Serial.begin(115200);
  delay(1000);

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Volume Knob");
  usb_hid.begin();
  delay(1000);

  lastclk = digitalRead(CLK);

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  delay(1000);
  FastLED.addLeds<NEOPIXEL, LED_RING_DATA>(leds, NUM_LEDS);  // GRB ordering is assumed

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);

  Serial.println("Ready");
}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
  #endif
  if (!TinyUSBDevice.mounted()) {
    return;
  }

  uint8_t clk = digitalRead(CLK);

  if (clk != lastclk) {
    uint8_t dt = digitalRead(DT);

    if (dt == clk) {
      counter++;
      intCounter++;
      if (intCounter % 2 == 0) {
        volumeDown();
      }
    } else {
      counter--;
      intCounter--;
      if (intCounter % 2 == 0) {
        volumeUp();
      }
    }

    /*
    if (counter % 2 == 0) {
      now = millis();
      deltaTime = now - prev;
      if (deltaTime > 0) {
        speed = (1000.0 / deltaTime) * lastDir;
      }
      prev = now;
    }
    */

    Serial.println(counter);
    prev = millis();
    lastclk = clk;
  }
  /*
  if (millis() - prev > 500) {
    if(speed > 0) {
      speed -= 5;
    } else if(speed < 0) {
      speed += 5;
    }
  }
  */
  if (millis() - prev > 500) {
    counter *= 0.995;
  }

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

void sendConsumerKey(uint16_t key) {
  if (!usb_hid.ready()) return;

  usb_hid.sendReport16(RID_KEYBOARD, key);
  delay(2);
  usb_hid.sendReport16(RID_KEYBOARD, 0);
  delay(2);
}

void volumeUp() {
  sendConsumerKey(HID_KEY_VOLUME_UP);
}

void volumeDown() {
  sendConsumerKey(HID_KEY_VOLUME_DOWN);
}

void mute() {
  sendConsumerKey(HID_USAGE_CONSUMER_MUTE);
}
