#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "hal/bright/bright.h"
#include "hal/device.h"
#include <SPI.h>
#include <Wire.h>
#include <interface.h>

#define AW9523_ADDR      0x58
#define AW9523_REG_IN0   0x00
#define AW9523_REG_IN1   0x01
#define AW9523_REG_OUT0  0x02
#define AW9523_REG_CFG0  0x04
#define AW9523_REG_CFG1  0x05
#define AW9523_REG_RST   0x7F

// Adafruit MEMENTO AW9523 Expander Button Pins
#define BTN_SELECT       1    // Bottom-right "Select" -> Back / Escape
#define BTN_OK           11   // Bottom-left "OK"      -> Select / Enter
#define BTN_RIGHT        12   // D-Pad Right           -> Next '>'
#define BTN_UP           13   // D-Pad Up              -> Scroll Up
#define BTN_LEFT         14   // D-Pad Left            -> Prev '<'
#define BTN_DOWN         15   // D-Pad Down            -> Scroll Down

#define SHUTTER_BTN      0    // Top-right "Cheese!"   -> Direct GPIO Select / Enter
#define MEMENTO_SDA      34
#define MEMENTO_SCL      33

static bool aw9523_ready = false;
static uint32_t lastPollMs = 0;
static uint32_t lastRetryMs = 0;
static uint16_t prevBtnMask = 0xFFFF;
static bool prevShutterLow = false;

static bool aw9523WriteReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(AW9523_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

static bool aw9523ReadReg(uint8_t reg, uint8_t &val) {
    Wire.beginTransmission(AW9523_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)AW9523_ADDR, (uint8_t)1) != 1) return false;
    val = Wire.read();
    return true;
}

static bool initAW9523() {
    Wire.begin(MEMENTO_SDA, MEMENTO_SCL);
    Wire.setClock(100000);

    Wire.beginTransmission(AW9523_ADDR);
    if (Wire.endTransmission() != 0) {
        aw9523_ready = false;
        return false;
    }

    aw9523WriteReg(AW9523_REG_RST, 0x00);  // Soft-reset expander to clean state
    delay(5);
    aw9523WriteReg(AW9523_REG_CFG0, 0xFE); // P0.0 output (speaker), P0.1-P0.7 inputs
    aw9523WriteReg(AW9523_REG_OUT0, 0x01); // Unmute onboard speaker
    aw9523WriteReg(AW9523_REG_CFG1, 0xFF); // P1.0-P1.7 inputs (D-pad + OK)

    // Clear initial interrupt/input latch
    uint8_t dummy;
    aw9523ReadReg(AW9523_REG_IN0, dummy);
    aw9523ReadReg(AW9523_REG_IN1, dummy);

    aw9523_ready = true;
    return true;
}

void _setup_gpio() {
    // Initialize shared SPI bus early so TFT_eSPI never dereferences a null SPI host
    SPI.begin(36, 37, 35, 39);

    bruceConfigPins.buzzer     = 46;
    bruceConfigPins.i2c_bus    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.sys_i2c    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)36, (gpio_num_t)37, (gpio_num_t)35, (gpio_num_t)48};
    bruceConfigPins.irTx       = 17; // JST-PH A0
    bruceConfigPins.irRx       = 18; // JST-PH A1
    bruceConfigPins.rfTx       = 17;
    bruceConfigPins.rfRx       = 18;
    bruceConfigPins.rotation   = 3;  // Upright landscape (status bar at top)
    bruceConfigPins.badusb_bus = {(gpio_num_t)18, (gpio_num_t)17};

    pinMode(SHUTTER_BTN, INPUT_PULLUP);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    initAW9523();
}

void _post_setup_gpio() {
    hal_bright_attach(TFT_BL);
    hal_bright_set(TFT_BL, 100);
    initAW9523(); // Re-bind AW9523 after Bruce core finishes bus initialization
}

void _setBrightness(uint8_t brightval) {
    hal_bright_set(TFT_BL, brightval);
}

void InputHandler(void) {
    if (millis() - lastPollMs < 30) return;
    lastPollMs = millis();

    // Auto-recover I2C expander if Bruce core reconfigured Wire
    if (!aw9523_ready && (millis() - lastRetryMs > 1000)) {
        lastRetryMs = millis();
        initAW9523();
    }

    uint8_t p0 = 0xFF, p1 = 0xFF;
    if (aw9523_ready) {
        if (!aw9523ReadReg(AW9523_REG_IN0, p0) || !aw9523ReadReg(AW9523_REG_IN1, p1)) {
            aw9523_ready = false;
            p0 = 0xFF;
            p1 = 0xFF;
        }
    }

    uint16_t curMask = ((uint16_t)p1 << 8) | p0;
    uint16_t pressed = prevBtnMask & ~curMask; // Falling-edge detection (active LOW)
    prevBtnMask = curMask;

    bool shutterLow = (digitalRead(SHUTTER_BTN) == LOW);
    bool shutterPressed = shutterLow && !prevShutterLow;
    prevShutterLow = shutterLow;

    bool okPressed    = ((pressed & (1 << BTN_OK)) != 0) || shutterPressed;
    bool escPressed   = (pressed & (1 << BTN_SELECT)) != 0;
    bool upPressed    = (pressed & (1 << BTN_UP)) != 0;
    bool downPressed  = (pressed & (1 << BTN_DOWN)) != 0;
    bool leftPressed  = (pressed & (1 << BTN_LEFT)) != 0;
    bool rightPressed = (pressed & (1 << BTN_RIGHT)) != 0;

    if (!okPressed && !escPressed && !upPressed && !downPressed && !leftPressed && !rightPressed) {
        return;
    }

    if (wakeUpScreen()) return;

    if (okPressed) {
        SelPress = true;
        AnyKeyPress = true;
    }
    if (escPressed) {
        EscPress = true;
        AnyKeyPress = true;
    }
    if (upPressed) {
        UpPress = true;
        AnyKeyPress = true;
    }
    if (downPressed) {
        DownPress = true;
        AnyKeyPress = true;
    }
    if (leftPressed) {
        PrevPress = true;
        AnyKeyPress = true;
    }
    if (rightPressed) {
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
