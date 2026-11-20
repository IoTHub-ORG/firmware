#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "hal/bright/bright.h"
#include "hal/device.h"
#include <SPI.h>
#include <Wire.h>
#include <interface.h>
#include <Wire.h>
#include <Adafruit_AW9523.h>

Adafruit_AW9523 aw;

// AW9523 Expander Button Pins on Adafruit MEMENTO
#define BTN_OK      11
#define BTN_SELECT  1
#define BTN_UP      13
#define BTN_DOWN    15
#define BTN_LEFT    14
#define BTN_RIGHT   12
#define BTN_SHUTTER 0   // Direct ESP32-S3 "Cheese!" button


#define AW9523_ADDR      0x58
#define AW9523_REG_IN0   0x00
#define AW9523_REG_IN1   0x01
#define AW9523_REG_OUT0  0x02
#define AW9523_REG_CFG0  0x04
#define AW9523_REG_CFG1  0x05

#define SHUTTER_BTN      0
#define MEMENTO_SDA      34
#define MEMENTO_SCL      33

static bool aw9523_ready = false;
static uint32_t lastPollMs = 0;
static uint16_t prevBtnMask = 0xFFFF;

static void aw9523WriteReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(AW9523_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t aw9523ReadReg(uint8_t reg) {
    Wire.beginTransmission(AW9523_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    if (Wire.requestFrom((uint8_t)AW9523_ADDR, (uint8_t)1) != 1) return 0xFF;
    return Wire.read();
}

void _setup_gpio() {
    // Initialize shared SPI bus early so TFT_eSPI / DisplayDrivers never dereference a null SPI host
    SPI.begin(36, 37, 35, 39);

    bruceConfigPins.buzzer     = 46;
    bruceConfigPins.i2c_bus    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.sys_i2c    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)36, (gpio_num_t)37, (gpio_num_t)35, (gpio_num_t)48};
    bruceConfigPins.irTx       = 17; // JST-PH A0
    bruceConfigPins.irRx       = 18; // JST-PH A1
    bruceConfigPins.rfTx       = 17;
    bruceConfigPins.rfRx       = 18;
    bruceConfigPins.rotation   = 0;
    bruceConfigPins.badusb_bus = {(gpio_num_t)18, (gpio_num_t)17};

    pinMode(SHUTTER_BTN, INPUT_PULLUP);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Initialize AW9523 I2C GPIO expander for buttons & speaker unmute
    Wire.begin(MEMENTO_SDA, MEMENTO_SCL);
    Wire.beginTransmission(AW9523_ADDR);
    if (Wire.endTransmission() == 0) {
        aw9523_ready = true;
        aw9523WriteReg(AW9523_REG_CFG0, 0xFE);
        aw9523WriteReg(AW9523_REG_OUT0, 0x01); // Unmute speaker
        aw9523WriteReg(AW9523_REG_CFG1, 0xFF);
    }
}

void _post_setup_gpio() {
    hal_bright_attach(TFT_BL);
    hal_bright_set(TFT_BL, 100);
}

void _setBrightness(uint8_t brightval) {
    hal_bright_set(TFT_BL, brightval);
}

void InputHandler(void) {
    if (millis() - lastPollMs < 35) return;
    lastPollMs = millis();

    uint8_t p0 = aw9523_ready ? aw9523ReadReg(AW9523_REG_IN0) : 0xFF;
    uint8_t p1 = aw9523_ready ? aw9523ReadReg(AW9523_REG_IN1) : 0xFF;
    uint16_t curMask = ((uint16_t)p1 << 8) | p0;

    uint16_t pressed = prevBtnMask & ~curMask;
    prevBtnMask = curMask;

    bool okPressed    = (pressed & (1 << 11)) != 0;
    bool selPressed   = (pressed & (1 << 1))  != 0;
    bool upPressed    = (pressed & (1 << 13)) != 0;
    bool downPressed  = (pressed & (1 << 15)) != 0;
    bool leftPressed  = (pressed & (1 << 14)) != 0;
    bool rightPressed = (pressed & (1 << 12)) != 0;
    bool shutterLow   = (digitalRead(SHUTTER_BTN) == LOW);

    if (okPressed) {
        SelPress = true;
        AnyKeyPress = true;
    }
    if (selPressed || shutterLow) {
        EscPress = true;
        AnyKeyPress = true;
    }
    if (upPressed || leftPressed) {
        PrevPress = true;
        AnyKeyPress = true;
    }
    if (downPressed || rightPressed) {
        NextPress = true;
        AnyKeyPress = true;
    }
}

void powerOff() {
    hal_bright_set(TFT_BL, 0);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SHUTTER_BTN, LOW);
    esp_deep_sleep_start();
}

void checkReboot() {
}
