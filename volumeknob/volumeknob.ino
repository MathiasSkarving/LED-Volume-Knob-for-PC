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

// For switch
int lastPress;

// For LEDS
int activeLeds;
unsigned long prev;
CRGB leds[NUM_LEDS];
int ledMode = 0;

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

  readEncoder();

  readSwitch();

  switch (ledMode) {
    case 0:
      {
        rainbowLed();
        break;
      }
    case 1:
      {
        bloodCycle();
        break;
      }
    default:
      {
        ledMode = 0;
        break;
      }
  }
}

void rainbowLed() {
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

void bloodCycle() {
  fill_solid(leds, NUM_LEDS, CHSV(0, 255, 20));

  int pos;
  if (counter > 0) {
    pos = constrain(abs(counter) / 3.0, 0, NUM_LEDS - 1);
  } else if (counter < 0) {
    pos = (NUM_LEDS - 1) - constrain(abs(counter) / 3.0, 0, NUM_LEDS - 1);
  } else {
    FastLED.show(); 
    return;
  }

  leds[pos] = CHSV(0, 255, 255);

  if(pos > 0) {
    leds[pos-1] = CHSV(0, 255, 130);
    leds[pos+1] = CHSV(0, 255, 130);
  } 
  else if(pos < NUM_LEDS-1) {
    leds[pos-1] = CHSV(0, 255, 130);
    leds[pos+1] = CHSV(0, 255, 130);
  }

  FastLED.show();
}

void readSwitch() {
  if (digitalRead(SWITCH) == LOW) {
    delay(50); 
    if (digitalRead(SWITCH) == HIGH) return; 

    lastPress = millis();
    while (digitalRead(SWITCH) == LOW) {
      if (millis() - lastPress > 1000) {
        ledMode++;
        if (ledMode > 1) {
          ledMode = 0;
        }
        animateModeChange(ledMode);
        Serial.print("Selected mode ");
        Serial.println(ledMode);
        
        while(digitalRead(SWITCH) == LOW) { delay(10); } 
        return;
      }
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

  if (millis() - prev > 1000) {
    counter *= 0.995;
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

  CRGB color;
  switch (mode) {
    case 0: color = CRGB::Blue; break;
    case 1: color = CRGB::Green; break;
    case 2: color = CRGB::Purple; break;
    case 3: color = CRGB::Red; break;
    default: color = CRGB::White; break;
  }

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
