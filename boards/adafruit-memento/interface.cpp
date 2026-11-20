#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "hal/bright/bright.h"
#include "hal/device.h"
#include <Wire.h>
#include <interface.h>

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
    bruceConfigPins.buzzer     = 46;
    bruceConfigPins.i2c_bus    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.sys_i2c    = {(gpio_num_t)MEMENTO_SDA, (gpio_num_t)MEMENTO_SCL};
    bruceConfigPins.irTx       = 17; // JST-PH A0
    bruceConfigPins.irRx       = 18; // JST-PH A1
    bruceConfigPins.rfTx       = 17;
    bruceConfigPins.rfRx       = 18;
    bruceConfigPins.rotation   = 0;
    bruceConfigPins.badusb_bus = {(gpio_num_t)18, (gpio_num_t)17};

    // microSD shares SPI bus with ST7789 TFT (SCK=36, MISO=37, MOSI=35, CS=48)
    bruceConfigPins.SDCARD_bus = {
        (gpio_num_t)36, (gpio_num_t)37, (gpio_num_t)35, (gpio_num_t)48
    };

    pinMode(SHUTTER_BTN, INPUT_PULLUP);

    // Initialize AW9523 I2C GPIO expander for buttons & speaker unmute
    Wire.begin(MEMENTO_SDA, MEMENTO_SCL);
    Wire.beginTransmission(AW9523_ADDR);
    if (Wire.endTransmission() == 0) {
        aw9523_ready = true;
        // Port 0: pin 0 = OUTPUT (speaker unmute), pins 1..7 = INPUT (pin 1 = SELECT)
        aw9523WriteReg(AW9523_REG_CFG0, 0xFE);
        aw9523WriteReg(AW9523_REG_OUT0, 0x01); // Set pin 0 HIGH to unmute speaker
        // Port 1: pins 8..15 = INPUT (OK=11, RIGHT=12, UP=13, LEFT=14, DOWN=15)
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
    if (millis() - lastPollMs < 35) return; // 35ms debounce
    lastPollMs = millis();

    uint8_t p0 = aw9523_ready ? aw9523ReadReg(AW9523_REG_IN0) : 0xFF;
    uint8_t p1 = aw9523_ready ? aw9523ReadReg(AW9523_REG_IN1) : 0xFF;
    uint16_t curMask = ((uint16_t)p1 << 8) | p0;

    // Trigger on falling edge (HIGH -> LOW)
    uint16_t pressed = prevBtnMask & ~curMask;
    prevBtnMask = curMask;

    bool okPressed     = (pressed & (1 << 11)) != 0;
    bool selPressed    = (pressed & (1 << 1))  != 0;
    bool upPressed     = (pressed & (1 << 13)) != 0;
    bool downPressed   = (pressed & (1 << 15)) != 0;
    bool leftPressed   = (pressed & (1 << 14)) != 0;
    bool rightPressed  = (pressed & (1 << 12)) != 0;
    bool shutterLow    = (digitalRead(SHUTTER_BTN) == LOW);

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
    // Handled via physical RST button and ON/OFF slide switch on MEMENTO
}

