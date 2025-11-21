#include "lcd.h"
#include "registers.h"
#include <SPI.h>

/*
  Unified, optimized LCD driver for ST7789 + LVGL 9

  Key points:
   - setAddrWindow: X -> CASET, Y -> RASET (always)
   - sendData: blocking, chunked SPI transfers to prevent tearing
   - flushWindow: fully synchronous; returns only after transfer done
   - Backlight: uses ledcAttach / ledcWrite (Arduino-compatible LEDC)
*/

SimpleSt7789::SimpleSt7789(SPIClass* spi,
                           const SPISettings& spiSettings,
                           uint16_t width,
                           uint16_t height,
                           uint8_t cs,
                           uint8_t dc,
                           uint8_t rst,
                           uint8_t backlight,
                           Rotation rotation)
: _spi(spi),
  _spiSettings(spiSettings),
  _width(width),
  _height(height),
  _pinCs(cs),
  _pinDc(dc),
  _pinRst(rst),
  _pinBacklight(backlight),
  _rotation(rotation),
  _xOffset(0),
  _yOffset(0) {
}

void SimpleSt7789::init() {
    pinMode(_pinCs, OUTPUT);
    pinMode(_pinDc, OUTPUT);

    if (_pinRst != (uint8_t)-1) {
        pinMode(_pinRst, OUTPUT);
    }

    if (_pinBacklight != (uint8_t)-1) {
        // Use Arduino-compatible LEDC attach (older cores): ledcAttach(pin, freq, resolution)
        // and ledcWrite(pin, value). This matches the existing codebase expectations.
        // If your core supports ledcAttachPin/ledcSetup, you can switch to those.
        ledcAttach(_pinBacklight, 1000, 10); // freq=1kHz, 10-bit resolution
        ledcWrite(_pinBacklight, 512);       // ~50% brightness default (0..1023)
    }

    reset();

    // Standard ST7789 init sequence (kept from original)
    sendCommand(REG_SLPOUT);
    delay(120);
    setRotation(_rotation);

    sendCommandFixed(REG_COLMOD, {0x05}); // 16-bit color mode (0x05)
    sendCommandFixed(REG_RAMCTRL, {0x00, 0xE8});
    sendCommandFixed(REG_PORCTRL, {0x0C, 0x0C, 0x00, 0x33, 0x33});
    sendCommandFixed(REG_GCTRL, {0x35});
    sendCommandFixed(REG_VCOMS, {0x35});
    sendCommandFixed(REG_LCMCTRL, {0x2C});
    sendCommandFixed(REG_VDVVRHEN, {0x01});
    sendCommandFixed(REG_VRHS, {0x13});
    sendCommandFixed(REG_VDVS, {0x20});
    sendCommandFixed(REG_FRCTR2, {0x0F});
    sendCommandFixed(REG_PWCTRL1, {0xA4, 0xA1});
    sendCommandFixed(0xD6, {0xA1});
    sendCommandFixed(REG_PVGAMCTRL, {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30});
    sendCommandFixed(REG_NVGAMCTRL, {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32});
    sendCommand(REG_INVON);
    sendCommand(REG_SLPOUT);
    delay(120);
    sendCommand(REG_DISPON);

    setBrightness(100);
}

void SimpleSt7789::reset() {
    if (_pinRst == (uint8_t)-1) return;

    digitalWrite(_pinCs, LOW);
    delay(50);
    digitalWrite(_pinRst, LOW);
    delay(50);
    digitalWrite(_pinRst, HIGH);
    delay(50);
}

void SimpleSt7789::setRotation(Rotation rotation) {
    _rotation = rotation;
    // Use MADCTL flags from registers.h (MADCTL_MX, MADCTL_MY, MADCTL_MV, MADCTL_RGB)
    uint8_t madctl = 0;
    switch (rotation) {
        case ROTATION_0:   madctl = MADCTL_MX | MADCTL_MY | MADCTL_RGB; break;
        case ROTATION_90:  madctl = MADCTL_MY | MADCTL_MV | MADCTL_RGB; break;
        case ROTATION_180: madctl = MADCTL_RGB; break;
        case ROTATION_270: madctl = MADCTL_MX | MADCTL_MV | MADCTL_RGB; break;
        default:           madctl = MADCTL_RGB; break;
    }
    sendCommand(REG_MADCTL, &madctl, 1);
}

void SimpleSt7789::setOffset(uint16_t xOffset, uint16_t yOffset) {
    _xOffset = xOffset;
    _yOffset = yOffset;
}

void SimpleSt7789::setBrightness(uint8_t percent) {
    if (_pinBacklight == (uint8_t)-1) return;
    percent = constrain(percent, 0, 100);
    // Convert to 10-bit range (0..1023)
    uint16_t pwm = map(percent, 0, 100, 0, 1023);
    ledcWrite(_pinBacklight, pwm);
}

void SimpleSt7789::flushWindow(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2,
                               uint16_t* color)
{
    // Compute and perform a blocking write of the area
    setAddrWindow(x1, y1, x2, y2);

    // Use 32-bit arithmetic to avoid overflow on bigger areas
    uint32_t width  = (uint32_t)x2 - (uint32_t)x1 + 1U;
    uint32_t height = (uint32_t)y2 - (uint32_t)y1 + 1U;
    uint32_t numBytes = width * height * 2U; // 2 bytes per pixel (RGB565)

    // sendData will chunk the transfer to avoid SPI driver issues
    sendData((const uint8_t*)color, (size_t)numBytes);
}

void SimpleSt7789::invertDisplay(bool invert) {
    sendCommand(invert ? REG_INVON : REG_INVOFF);
}

void SimpleSt7789::setAddrWindow(uint16_t x1, uint16_t y1,
                                uint16_t x2, uint16_t y2)
{
    // Apply offsets
    uint16_t ox1 = x1 + _xOffset;
    uint16_t ox2 = x2 + _xOffset;
    uint16_t oy1 = y1 + _yOffset;
    uint16_t oy2 = y2 + _yOffset;

    // ST7789 expects CASET = [XSTART, XEND], RASET = [YSTART, YEND] (big-endian)
    uint8_t caset[4] = {
        (uint8_t)(ox1 >> 8), (uint8_t)(ox1 & 0xFF),
        (uint8_t)(ox2 >> 8), (uint8_t)(ox2 & 0xFF)
    };
    uint8_t raset[4] = {
        (uint8_t)(oy1 >> 8), (uint8_t)(oy1 & 0xFF),
        (uint8_t)(oy2 >> 8), (uint8_t)(oy2 & 0xFF)
    };

    sendCommand(REG_CASET, caset, sizeof(caset));
    sendCommand(REG_RASET, raset, sizeof(raset));
    sendCommand(REG_RAMWR);
}

void SimpleSt7789::sendCommand(uint8_t command, const uint8_t* data, size_t size) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_pinCs, LOW);
    digitalWrite(_pinDc, LOW);

    // Send command byte
    _spi->transfer(command);

    if (data && size) {
        digitalWrite(_pinDc, HIGH);
        // sendData-like chunking for command payloads to be safe
        const size_t CHUNK = 4096;
        size_t pos = 0;
        while (pos < size) {
            size_t n = (size - pos > CHUNK) ? CHUNK : (size - pos);
            _spi->transferBytes(data + pos, nullptr, n);
            pos += n;
        }
    }

    digitalWrite(_pinCs, HIGH);
    _spi->endTransaction();
}

void SimpleSt7789::sendData(const uint8_t* data, size_t size) {
    // Blocking transfer in chunks to avoid large single transfer issues
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_pinCs, LOW);
    digitalWrite(_pinDc, HIGH);

    const size_t CHUNK = 4096; // safe chunk size; reduce if you get OOM or crashes
    size_t pos = 0;
    while (pos < size) {
        size_t n = (size - pos > CHUNK) ? CHUNK : (size - pos);
        _spi->transferBytes(data + pos, nullptr, n);
        pos += n;
    }

    digitalWrite(_pinCs, HIGH);
    _spi->endTransaction();
}
