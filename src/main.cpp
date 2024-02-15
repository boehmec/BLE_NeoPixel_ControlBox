#include <Adafruit_NeoPixel.h>
#include <BlockNot.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

/**
  Commands: 0-9
  0: turn color to red
  1: turn color to green
  2: turn color to blue
  3: start pulsing current color, and make pulsing also faster
  4: start pulsing current color, and make pulsing also slower
  5: current brightness darker
  6: current brightness brighter
  7: max brightness darker
  8: max brightness brighter
  9: reset to default
*/

#define PIN 4
//our power switch has an onboard LED we can power with our ESP alone
#define BTN_VCC_PIN 5
#define BTN_GND_PIN 6
#define SERVICE_UUID "68a3891c-3667-4937-b89b-f69479854ae7"
#define CHARACTERISTIC_UUID "284c702d-3609-4939-bd2a-7f88a7d383f8"

Adafruit_NeoPixel strip(30, PIN, NEO_GRB + NEO_KHZ800);

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

//will save energy, our 5V buck boost can handle only 0.8Amps and if 255 would be max = 1.8Amps then 100 ~ 0.7A increase at own risk
int maxBrightness = 100; 
int brightness = 100;
bool turnBrightnessUp = true;
int currentPulseDuration = 50;
BlockNot pulseTimer(currentPulseDuration, STOPPED);  //default 50 can be changed using commands

//from default examples, just sets new color for each led
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

class BLEServerCallback : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    log_i("Connected %d", pServer->getConnId());
    deviceConnected = true;
    colorWipe(strip.Color(0, 255, 0), 20);
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer* pServer) {
    log_i("Disconnected %d", pServer->getConnId());
    colorWipe(strip.Color(255, 0, 0), 20);
    deviceConnected = false;
    pulseTimer.STOP;
  }
};

class CommandsCallback : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic* pCharacteristic) {
    uint8_t* data = pCharacteristic->getData();
    int command = *data;
    if(command < 0 || command > 9){
      log_w("Unknown command!");
      return;
    }
    log_d("Command %d", command);

    switch (command) {
      case 0:
        colorWipe(strip.Color(255, 0, 0), 20);  // Red
        break;
      case 1:
        colorWipe(strip.Color(0, 255, 0), 20);  // Green
        break;
      case 2:
        colorWipe(strip.Color(0, 0, 255), 20);  // Blue
        break;
      case 3:  //make pulse faster
        currentPulseDuration -= 10;
        if (currentPulseDuration <= 10) {
          currentPulseDuration = 10;
        }
        pulseTimer.setDuration(currentPulseDuration);
        if (pulseTimer.isStopped()) {
          pulseTimer.START_RESET;
        }
        break;
      case 4:  //make pulse slower
        currentPulseDuration += 10;
        if (currentPulseDuration >= 100) {
          currentPulseDuration = 100;
        }
        pulseTimer.setDuration(currentPulseDuration);
        if (pulseTimer.isStopped()) {
          pulseTimer.START_RESET;
        }
        break;
      case 5:  //current brightness darker
        brightness -= 20;
        if (brightness <= 0) {
          brightness = 0;
        }
        strip.setBrightness(brightness);  // 0 ..255
        strip.show();                     // Update strip with new contents
        break;
      case 6:  //current brightness brighter
        brightness += 20;
        if (brightness >= maxBrightness) {
          brightness = maxBrightness;
        }
        strip.setBrightness(brightness);  // 0 ..255
        strip.show();                     // Update strip with new contents
        break;
      case 7:  //max brightness darker
        maxBrightness -= 20;
        if (maxBrightness <= 1) {
          maxBrightness = 1;
        }
        break;
      case 8:  //max brightness brighter
        maxBrightness += 20;
        if (maxBrightness >= 255) {
          maxBrightness = 255;
        }
        break;
      case 9: //reset to default 
        //stop all pulse
        pulseTimer.STOP;
        maxBrightness = 100;
        brightness = 100;
        currentPulseDuration = 50;
        strip.show();
        break;
    }
  }
};

void setup() {
  log_i("BLE NeoPixel LED Box Controller");
  BLEDevice::init("BLE NeoPixel Board Control");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BLEServerCallback());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new CommandsCallback());
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true); //TRUE otherwise our remote cannot find it
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  strip.begin();
  strip.setBrightness(brightness);  // 0 ..255
  colorWipe(strip.Color(255, 0, 0), 20);
  strip.show();

  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, LOW);

  //btn led light
  pinMode(BTN_VCC_PIN, OUTPUT);
  pinMode(BTN_GND_PIN, OUTPUT);
  digitalWrite(BTN_VCC_PIN, HIGH); //btn led is on when esp gets energy, so its default high
  digitalWrite(BTN_GND_PIN, LOW);
}

void loop() {
  if (deviceConnected) {
    if (pulseTimer.TRIGGERED) {
      if (turnBrightnessUp) {
        brightness++;
        if (brightness >= maxBrightness) {
          brightness = maxBrightness;
          turnBrightnessUp = false;
        }
      } else {
        brightness--;
        if (brightness <= 1) {  //0 would be off and color info is lost, so set min to 1
          brightness = 1;
          turnBrightnessUp = true;
        }
      }
      strip.setBrightness(brightness);  // 0 ..255
      strip.show();                     // Update strip with new contents
    }
  }
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    log_i("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}
