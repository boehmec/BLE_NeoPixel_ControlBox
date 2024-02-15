#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BlockNot.h>

/**
  General Commands: 0-9
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

  Live brightness:
  - send a value 1-255 to LIVE_BRIGHTNESS_CHARACTERISTIC_UUID (sanity checks
  like maxBrightness apply)
  - this can easily get expanded to sound control feature
*/

#define PIN 4
// our power switch has an onboard LED we can power with our ESP alone
#define BTN_VCC_PIN 5
#define BTN_GND_PIN 6
#define SERVICE_UUID "68a3891c-3667-4937-b89b-f69479854ae7"
#define COMMAND_CHARACTERISTIC_UUID "284c702d-3609-4939-bd2a-7f88a7d383f8"
#define LIVE_BRIGHTNESS_CHARACTERISTIC_UUID  "39cdb833-ad1b-442f-8845-715d69e27614"

#define DEFAULT_MAX_BRIGHTNESS 100
const int MAX_MTU = 350;

Adafruit_NeoPixel strip(30, PIN, NEO_GRB + NEO_KHZ800);

BLEServer *pServer = NULL;
BLECharacteristic *commandsCharacteristic = NULL;
BLECharacteristic *liveBrightnessCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// will save energy, our 5V buck boost can handle only 0.8Amps and if 255 would
// be max = 1.8Amps then 100 ~ 0.7A increase at own risk
int maxBrightness = DEFAULT_MAX_BRIGHTNESS;
int brightness = DEFAULT_MAX_BRIGHTNESS;
bool turnBrightnessUp = true;
int currentPulseDuration = 50;

BlockNot bleTimer = BlockNot(1500, STOPPED);
BlockNot pulseTimer(currentPulseDuration, STOPPED); // default 50 can be changed using commands

// from default examples, just sets new color for each led
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

class BLEServerCallback : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    log_i("Connected %d", pServer->getConnId());
    deviceConnected = true;
    colorWipe(strip.Color(0, 255, 0), 20);
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer *pServer) {
    log_i("Disconnected %d", pServer->getConnId());
    deviceConnected = false;
    pulseTimer.STOP;
    bleTimer.START_RESET;
    strip.setBrightness(DEFAULT_MAX_BRIGHTNESS);
    colorWipe(strip.Color(255, 0, 0), 20);
  }
};

class CommandsCallback : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t *data = pCharacteristic->getData();
    int command = *data;
    if (command < 0 || command > 9) {
      log_w("Unknown command!");
      return;
    }
    log_d("Command %d", command);

    switch (command) {
    case 0:
      colorWipe(strip.Color(255, 0, 0), 0); // Red
      break;
    case 1:
      colorWipe(strip.Color(0, 255, 0), 0); // Green
      break;
    case 2:
      colorWipe(strip.Color(0, 0, 255), 0); // Blue
      break;
    case 3: // make pulse faster
      currentPulseDuration -= 10;
      if (currentPulseDuration <= 10) {
        currentPulseDuration = 10;
      }
      pulseTimer.setDuration(currentPulseDuration);
      if (pulseTimer.isStopped()) {
        pulseTimer.START_RESET;
      }
      break;
    case 4: // make pulse slower
      currentPulseDuration += 10;
      if (currentPulseDuration >= 100) {
        currentPulseDuration = 100;
      }
      pulseTimer.setDuration(currentPulseDuration);
      if (pulseTimer.isStopped()) {
        pulseTimer.START_RESET;
      }
      break;
    case 5: // current brightness darker
      brightness -= 20;
      if (brightness <= 0) {
        brightness = 0;
      }
      strip.setBrightness(brightness); // 0 ..255
      strip.show();                    // Update strip with new contents
      break;
    case 6: // current brightness brighter
      brightness += 20;
      if (brightness >= maxBrightness) {
        brightness = maxBrightness;
      }
      strip.setBrightness(brightness); // 0 ..255
      strip.show();                    // Update strip with new contents
      break;
    case 7: // max brightness darker
      maxBrightness -= 20;
      if (maxBrightness <= 1) {
        maxBrightness = 1;
      }
      break;
    case 8: // max brightness brighter
      maxBrightness += 20;
      if (maxBrightness >= 255) {
        maxBrightness = 255;
      }
      break;
    case 9: // reset to default
      // stop all pulse
      pulseTimer.STOP;
      maxBrightness = 100;
      brightness = 100;
      currentPulseDuration = 50;
      strip.show();
      break;
    }
  }
};

class LiveBrightnessCallback : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t *data = pCharacteristic->getData();
    int brightness = *data;
    if (brightness >= maxBrightness) {
      brightness = maxBrightness;
    }
    // prevent turning off led
    if (brightness <= 1) {
      brightness = 1;
    }

    strip.setBrightness(brightness);
    strip.show();
  }
};

void setup() {
  log_i("BLE NeoPixel LED Box Controller");
  BLEDevice::init("BLE NeoPixel Board Control");
  BLEDevice::setMTU(MAX_MTU);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

  int pwrAdv = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  int pwrScan = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_SCAN);
  int pwrDef = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
  log_i("BLE Power is set pwrAdv %d pwrScan %d pwrDef %d" , pwrAdv, pwrScan, pwrDef); //BLE Power is set pwrAdv 7 pwrScan 7 pwrDef 7 is highest

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BLEServerCallback());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  commandsCharacteristic = pService->createCharacteristic(
      COMMAND_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  commandsCharacteristic->setCallbacks(new CommandsCallback());

  liveBrightnessCharacteristic = pService->createCharacteristic(
      LIVE_BRIGHTNESS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  liveBrightnessCharacteristic->setCallbacks(new LiveBrightnessCallback());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(
      true); // TRUE otherwise our remote cannot find it
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  strip.begin();
  strip.setBrightness(brightness); // 0 ..255
  colorWipe(strip.Color(255, 0, 0), 20);
  strip.show();

  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, LOW);

  // btn led light
  pinMode(BTN_VCC_PIN, OUTPUT);
  pinMode(BTN_GND_PIN, OUTPUT);
  digitalWrite(BTN_VCC_PIN, HIGH); // btn led is on when esp gets energy, so its default high
  digitalWrite(BTN_GND_PIN, LOW);
}

void handlePulsing(){
  if (pulseTimer.TRIGGERED) {
      if (turnBrightnessUp) {
        brightness++;
        if (brightness >= maxBrightness) {
          brightness = maxBrightness;
          turnBrightnessUp = false;
        }
      } else {
        brightness--;
        if (brightness <=
            1) { // 0 would be off and color info is lost, so set min to 1
          brightness = 1;
          turnBrightnessUp = true;
        }
      }
      strip.setBrightness(brightness); // 0 ..255
      strip.show();                    // Update strip with new contents
    }
}

void handleClient(){
  handlePulsing();
}

void loop() {
  if (deviceConnected) {
    handleClient();
  }

  if (!deviceConnected && oldDeviceConnected) {
    if (bleTimer.TRIGGERED) {
      bleTimer.STOP;
      bleTimer.RESET;
      pServer->startAdvertising();  // restart advertising
      log_i("start advertising" );
      oldDeviceConnected = deviceConnected;
    }
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    log_i("connecting" );
  }
}
