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
  - send a value 0-255 to LIVE_BRIGHTNESS_CHARACTERISTIC_UUID (sanity checks like maxBrightness apply)
  - this can easily get expanded to sound control feature
*/

#define PIN 4
// our power switch has an onboard LED we can power with our ESP alone
#define BTN_VCC_PIN 5
#define BTN_GND_PIN 6
#define SERVICE_UUID "68a3891c-3667-4937-b89b-f69479854ae7"
#define COMMAND_CHARACTERISTIC_UUID "284c702d-3609-4939-bd2a-7f88a7d383f8"
#define LIVE_BRIGHTNESS_CHARACTERISTIC_UUID                                    \
  "39cdb833-ad1b-442f-8845-715d69e27614"

#define DEFAULT_MAX_BRIGHTNESS 90
#define DEFAULT_PULSE_DURATION 40
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
int currentPulseDuration = DEFAULT_PULSE_DURATION;

BlockNot bleTimer = BlockNot(1500, STOPPED);
// default 50 can be changed using commands
BlockNot pulseTimer(currentPulseDuration, STOPPED); 

// from default examples, just sets new color for each led
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

void enablePulse(int duration) {
  pulseTimer.setDuration(duration);
  if (pulseTimer.isStopped()) {
    pulseTimer.START_RESET;
  }
}

class BLEServerCallback : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
    log_i("Connected %d", pServer->getConnId());
    // prepare for updating params
    esp_bd_addr_t clientAddress;
    memcpy(clientAddress, param->connect.remote_bda, ESP_BD_ADDR_LEN);
    esp_gatt_conn_params_t gattConnParams = param->connect.conn_params;
    log_d("gattConnParams timeout %d interval %d latency %d",
          gattConnParams.timeout, gattConnParams.interval,
          gattConnParams.latency);
    esp_gap_conn_params_t connParams;
    ////supervision_tout  Range: 0x000A to 0x0C80 Time = N * 10 msec Time Range:
    ///100 msec to 32 seconds
    esp_ble_get_current_conn_params(clientAddress, &connParams);
    log_d("connParams timeout %d interval %d latency %d", connParams.timeout,
          connParams.interval, connParams.latency);

    esp_ble_conn_update_params_t updatedConnParams;
    memcpy(updatedConnParams.bda, clientAddress, ESP_BD_ADDR_LEN);
    // updatedConnParams.bda = clientAddress;
    updatedConnParams.min_int = 0x06; // x 1.25ms
    updatedConnParams.max_int = 0x20; // x 1.25ms
    updatedConnParams.latency = 0x00; // number of skippable connection events
    updatedConnParams.timeout =
        0x00C8; // x 200ms x 10ms = 2000ms time before peripheral will assume
                // connection is dropped.

    esp_err_t updateRes = esp_ble_gap_update_conn_params(&updatedConnParams);
    log_d("connParams updateRes %d", updateRes);
    esp_ble_get_current_conn_params(
        clientAddress,
        &connParams); ////supervision_tout  Range: 0x000A to 0x0C80 Time = N *
                      ///10 msec Time Range: 100 msec to 32 seconds
    log_d("connParams timeout %d interval %d latency %d", connParams.timeout,
          connParams.interval, connParams.latency);

    deviceConnected = true;
    colorWipe(strip.Color(0, 255, 0), 20); //green
    pulseTimer.STOP;
    BLEDevice::startAdvertising();
  }

  void onDisconnect(BLEServer *pServer) {
    log_i("Disconnected %d", pServer->getConnId());
    deviceConnected = false;
    pulseTimer.STOP;
    bleTimer.START_RESET;
    strip.setBrightness(DEFAULT_MAX_BRIGHTNESS);
    colorWipe(strip.Color(255, 0, 0), 20); //red
    enablePulse(DEFAULT_PULSE_DURATION); // set pulse
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
      enablePulse(currentPulseDuration);
      break;
    case 4: // make pulse slower
      currentPulseDuration += 10;
      if (currentPulseDuration >= 100) {
        currentPulseDuration = 100;
      }
      enablePulse(currentPulseDuration);
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
    //switch off pulse timer in live mode
    if(pulseTimer.isRunning()){
      pulseTimer.STOP;
    }
    uint8_t *data = pCharacteristic->getData();
    int brightness = *data;
    // we allow a 0 brightness, effectively turning it off
    // as the client can control the color
    int adjustedBrightness = map(brightness, 0, 255, 0, maxBrightness);
    // handle small brightness edge case when we have e.g. a maxBrightness of
    // 100 and we want to display 1b then adjustedBrightness would still be 0
    // const long rise = out_max - out_min;  => 100 - 0 = 100
    // const long delta = x - in_min;        => 1 - 0   = 1
    // return (delta * rise) / run + out_min; => 1 * 100 / (255 - 0) + 0 =>
    // 100/255 = 0 (long)
    if (brightness > 0 && adjustedBrightness == 0) {
      adjustedBrightness = 1;
    }
    log_d("brightness %d -> adjustedBrightness %d", brightness, adjustedBrightness);
    strip.setBrightness(adjustedBrightness);
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
  log_i("BLE Power is set pwrAdv %d pwrScan %d pwrDef %d", pwrAdv, pwrScan,
        pwrDef); // BLE Power is set pwrAdv 7 pwrScan 7 pwrDef 7 is highest

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
  strip.setBrightness(DEFAULT_MAX_BRIGHTNESS); // 0 ..255
  colorWipe(strip.Color(255, 0, 0), 20); //red
  enablePulse(DEFAULT_PULSE_DURATION); // set pulse
  strip.show();

  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, LOW);

  // btn led light
  pinMode(BTN_VCC_PIN, OUTPUT);
  pinMode(BTN_GND_PIN, OUTPUT);
  digitalWrite(BTN_VCC_PIN, HIGH); // btn led is on when esp gets energy, so its default high
  digitalWrite(BTN_GND_PIN, LOW);
}

void handlePulsing() {
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

void handleClient() { 
  //add code for further client handling
}

void loop() {
  handlePulsing(); 
  if (deviceConnected) {
    handleClient();
  }

  if (!deviceConnected && oldDeviceConnected) {
    if (bleTimer.TRIGGERED) {
      bleTimer.STOP;
      bleTimer.RESET;
      pServer->startAdvertising(); // restart advertising
      log_i("start advertising");
      oldDeviceConnected = deviceConnected;
    }
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    log_i("connecting");
  }
}
