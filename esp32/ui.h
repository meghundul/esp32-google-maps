#ifndef UI_H
#define UI_H

#define LV_LVGL_H_INCLUDE_SIMPLE
#include "config.h"
#include "lcd.h"
#include "local_fonts.h"
#include "theme.h"

#include "FS.h"
#include "SPIFFS.h"
#include "ble.h"
#include <lvgl.h>

#define FS                      SPIFFS
#define FORMAT_SPIFFS_IF_FAILED true

// ICON CONFIG — 64x64 icons
#define ICON_HEIGHT             64
#define ICON_WIDTH              64

// 1-bit bitmap buffer & RGB565 render buffer
#define ICON_BITMAP_BUFFER_SIZE ((ICON_HEIGHT * ICON_WIDTH) / 8)
#define ICON_RENDER_BUFFER_SIZE (ICON_WIDTH * ICON_HEIGHT * (LV_COLOR_DEPTH / 8))


// SCREEN SIZE from config.h (HORIZONTAL or VERTICAL)
#ifdef HORIZONTAL
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 172
#else
#define SCREEN_WIDTH  172
#define SCREEN_HEIGHT 320
#endif


// ---------------------------
// PARTIAL BUFFER (1 ROW)
// ---------------------------
#define DRAW_BUF_HEIGHT 1
#define DRAW_BUF_SIZE   (SCREEN_WIDTH * DRAW_BUF_HEIGHT)

uint16_t draw_buf_0[DRAW_BUF_SIZE];


// LCD INSTANCE
SimpleSt7789 lcd(&SPI,
                 SPISettings(80000000, MSBFIRST, SPI_MODE0),
                 SCREEN_HEIGHT,
                 SCREEN_HEIGHT,
                 PIN_LCD_CS,
                 PIN_LCD_DC,
                 PIN_LCD_RST,
                 PIN_BACKLIGHT,
#ifdef HORIZONTAL
                 SimpleSt7789::ROTATION_270
#else
                 SimpleSt7789::ROTATION_180
#endif
);


// ---------------------------
// LVGL LOGGING
// ---------------------------
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char* buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

// ---------------------------
// LVGL FLUSH CALLBACK (LVGL 9)
// ---------------------------
void my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    lcd.flushWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)px_map);
    lv_display_flush_ready(disp);
}

static uint32_t my_tick(void) {
    return millis();
}


namespace Data {
    namespace details {
        int speed                 = -1;
        String nextRoad           = String();
        String nextRoadDesc       = String();
        String eta                = String();
        String ete                = String();
        String distanceToNextTurn = String();
        String totalDistance      = String();
        String displayIconHash    = String();
        String receivedIconHash   = String();
        bool iconDirty            = false;

        std::vector<String> availableIcons{};
        uint8_t receivedIconBitmapBuffer[ICON_BITMAP_BUFFER_SIZE];
        uint8_t iconBitmapBuffer[ICON_BITMAP_BUFFER_SIZE];
        uint8_t iconRenderBuffer[ICON_RENDER_BUFFER_SIZE];
    }
}


namespace UI {

    namespace details {
        lv_obj_t* lblSpeed;
        lv_obj_t* lblSpeedUnit;
        lv_obj_t* lblEta;
        lv_obj_t* lblNextRoad;
        lv_obj_t* lblNextRoadDesc;
        lv_obj_t* lblDistanceToNextRoad;
        lv_obj_t* imgTbtIcon;

        uint32_t lastUpdate = 0;
    }


    // ---------------------------
    // UI INIT
    // ---------------------------
    void init() {
        using namespace details;

        SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI);

#ifdef HORIZONTAL
        lcd.setOffset(0, 34);
#else
        lcd.setOffset(34, 0);
#endif

        lcd.init();

        lv_init();
        lv_tick_set_cb(my_tick);

        delay(100);

        // splash clear
        memset(draw_buf_0, 0xAA, sizeof(draw_buf_0));
        lcd.flushWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, draw_buf_0);

        delay(200);

#if LV_USE_LOG != 0
        lv_log_register_print_cb(my_print);
#endif

        // ---------------------------
        // LVGL DISPLAY SETUP
        // ---------------------------
        lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

        lv_display_set_flush_cb(disp, my_disp_flush);

        // PARTIAL RENDER MODE
        lv_display_set_buffers(
            disp,
            draw_buf_0,
            nullptr,
            sizeof(draw_buf_0),
            LV_DISPLAY_RENDER_MODE_PARTIAL
        );

        // White background
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);


        // ---------------------------
        // UI OBJECTS
        // ---------------------------
        imgTbtIcon = lv_img_create(lv_scr_act());
        lv_obj_set_style_bg_color(imgTbtIcon, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);

        lblSpeed = lv_label_create(lv_scr_act());
        lv_label_set_text(lblSpeed, "0");
        lv_obj_set_style_text_color(lblSpeed, lv_color_make(0xFF, 0x00, 0x00), LV_PART_MAIN);

        lblSpeedUnit = lv_label_create(lv_scr_act());
        lv_label_set_text(lblSpeedUnit, "km/h");

        lblDistanceToNextRoad = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_color(lblDistanceToNextRoad, lv_color_make(0x00, 0x00, 0xFF), LV_PART_MAIN);

        lblNextRoad     = lv_label_create(lv_scr_act());
        lblNextRoadDesc = lv_label_create(lv_scr_act());
        lblEta          = lv_label_create(lv_scr_act());

#ifdef HORIZONTAL
// ===== LANDSCAPE UI =====

#define LEFT_PART_WIDTH  (SCREEN_HEIGHT / 2 - 12)
#define RIGHT_PART_WIDTH (SCREEN_WIDTH - LEFT_PART_WIDTH - 10)

        lv_obj_set_style_width(imgTbtIcon, ICON_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_height(imgTbtIcon, ICON_HEIGHT, LV_PART_MAIN);
        lv_obj_align(imgTbtIcon, LV_ALIGN_TOP_LEFT, 10, 10);

        lv_obj_set_style_width(lblSpeed, LEFT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblSpeed, get_montserrat_number_bold_48(), LV_STATE_DEFAULT);
        lv_obj_align(lblSpeed, LV_ALIGN_BOTTOM_LEFT, 12, -10);

        lv_obj_set_style_width(lblSpeedUnit, LEFT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblSpeedUnit, get_montserrat_24(), LV_STATE_DEFAULT);
        lv_obj_align_to(lblSpeedUnit, lblSpeed, LV_ALIGN_TOP_LEFT, 0, -28);

        lv_obj_set_style_width(lblEta, RIGHT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblEta, get_montserrat_24(), LV_STATE_DEFAULT);
        lv_obj_align(lblEta, LV_ALIGN_TOP_RIGHT, 0, 10);

        lv_obj_set_style_width(lblDistanceToNextRoad, RIGHT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblDistanceToNextRoad, get_montserrat_bold_32(), LV_STATE_DEFAULT);
        lv_obj_align_to(lblDistanceToNextRoad, lblEta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

        lv_obj_set_style_width(lblNextRoadDesc, RIGHT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblNextRoadDesc, get_montserrat_semibold_24(), LV_STATE_DEFAULT);
        lv_obj_align(lblNextRoadDesc, LV_ALIGN_BOTTOM_RIGHT, 0, -10);

        lv_obj_set_style_width(lblNextRoad, RIGHT_PART_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblNextRoad, get_montserrat_semibold_28(), LV_STATE_DEFAULT);
        lv_obj_align_to(lblNextRoad, lblNextRoadDesc, LV_ALIGN_TOP_LEFT, 0, -40);

#else
// ===== PORTRAIT UI =====

        lv_obj_set_style_width(imgTbtIcon, ICON_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_height(imgTbtIcon, ICON_HEIGHT, LV_PART_MAIN);
        lv_obj_align(imgTbtIcon, LV_ALIGN_TOP_LEFT, 10, 10);

        lv_obj_set_style_width(lblSpeed, SCREEN_WIDTH / 2 - 12, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblSpeed, get_montserrat_number_bold_48(), LV_STATE_DEFAULT);
        lv_obj_align(lblSpeed, LV_ALIGN_TOP_RIGHT, -12, 15);

        lv_obj_set_style_width(lblSpeedUnit, SCREEN_WIDTH / 2 - 12, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblSpeedUnit, get_montserrat_24(), LV_STATE_DEFAULT);
        lv_obj_align(lblSpeedUnit, LV_ALIGN_TOP_RIGHT, -12, 50);

        lv_obj_set_style_width(lblDistanceToNextRoad, SCREEN_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblDistanceToNextRoad, get_montserrat_semibold_28(), LV_STATE_DEFAULT);
        lv_obj_align(lblDistanceToNextRoad, LV_ALIGN_TOP_MID, 0, 85);

        lv_obj_set_style_width(lblNextRoad, SCREEN_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblNextRoad, get_montserrat_semibold_28(), LV_STATE_DEFAULT);
        lv_obj_align_to(lblNextRoad, lblDistanceToNextRoad, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

        lv_obj_set_style_width(lblNextRoadDesc, SCREEN_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_text_font(lblNextRoadDesc, get_montserrat_semibold_24(), LV_STATE_DEFAULT);
        lv_obj_align_to(lblNextRoadDesc, lblNextRoad, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

        lv_obj_set_style_text_font(lblEta, get_montserrat_24(), LV_STATE_DEFAULT);
        lv_obj_align(lblEta, LV_ALIGN_BOTTOM_MID, 0, -5);

#endif
    }


    // ---------------------------
    // UI UPDATE LOOP
    // ---------------------------
    void update() {
        using namespace details;

        if (millis() - details::lastUpdate < 5)
            return;

        details::lastUpdate = millis();

        // LVGL internal updates
        lv_timer_handler();

        // Update icon
        if (Data::details::iconDirty) {
            Data::details::iconDirty = false;

            static lv_img_dsc_t icon;
            icon.header.cf     = LV_COLOR_FORMAT_RGB565;
            icon.header.w      = ICON_WIDTH;
            icon.header.h      = ICON_HEIGHT;
            icon.header.stride = ICON_WIDTH * (LV_COLOR_DEPTH / 8);
            icon.data_size     = ICON_RENDER_BUFFER_SIZE;
            icon.data          = (const uint8_t*)Data::details::iconRenderBuffer;

            lv_img_set_src(imgTbtIcon, &icon);
        }
    }

} // namespace UI


// --------------------------------------
// Utility: Convert 1-bit icon -> RGB565
// --------------------------------------
void convert1BitBitmapToRgb565(void* dst, const void* src,
                               uint16_t width, uint16_t height,
                               uint16_t color, uint16_t bgColor,
                               bool invert = false)
{
    uint16_t* d      = (uint16_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    auto activeColor   = invert ? bgColor : color;
    auto inactiveColor = invert ? color : bgColor;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            bool pixelOn = (s[(y * width + x) >> 3] & (1 << (7 - (x & 7))));
            d[y * width + x] = pixelOn ? activeColor : inactiveColor;
        }
    }
}

namespace Data {

    // ---------- FORWARD DECLARATIONS ----------
    bool hasNavigationData();
    bool hasSpeedData();
    void clearNavigationData();
    void clearSpeedData();
    int speed();
    void setSpeed(const int& value);
    String nextRoad();
    void setNextRoad(const String& value);
    String nextRoadDesc();
    void setNextRoadDesc(const String& value);
    String eta();
    void setEta(const String& value);
    String ete();
    void setEte(const String& value);
    String totalDistance();
    void setTotalDistance(const String& value);
    String distanceToNextTurn();
    void setDistanceToNextTurn(const String& value);
    String displayIconHash();
    void setIconHash(const String& value);
    uint8_t* iconRenderBuffer();
    void setIconBuffer(const uint8_t* value, const size_t& length);
    String fullEta();
    void saveIcon(const String& iconHash, const uint8_t* buffer);
    bool isIconExisted(const String& iconHash);
    void loadIcon(const String& iconHash);
    void receiveNewIcon(const String& iconHash, const uint8_t* buffer);
    void removeAllFiles();
    void listFiles();
    size_t readFile(const String& filename, uint8_t* buffer, const size_t bufferSize);
    void writeFile(const String& filename, const uint8_t* buffer, const size_t& length);


    // ----------------------
    // DATA STORAGE CONTROL
    // ----------------------
    void init() {
        if (!FS.begin(FORMAT_SPIFFS_IF_FAILED)) {
            Serial.println("Error mounting SPIFFS");
            return;
        }
        listFiles();
    }

    bool hasNavigationData() {
        return !(details::nextRoad.isEmpty() &&
                 details::nextRoadDesc.isEmpty() &&
                 details::eta.isEmpty() &&
                 details::distanceToNextTurn.isEmpty());
    }

    bool hasSpeedData() {
        return details::speed >= 0;
    }

    void clearNavigationData() {
        details::nextRoad     = "";
        details::nextRoadDesc = "";
        details::eta          = "";
        details::ete          = "";
        details::distanceToNextTurn = "";
        details::totalDistance      = "";
        details::displayIconHash    = "";
        details::receivedIconHash   = "";
    }

    void clearSpeedData() {
        setSpeed(-1);
    }

    int speed() {
        return std::max(details::speed, 0);
    }

    void setSpeed(const int& value) {
        if (value == details::speed) return;

        details::speed = value;

        if (value == -1) {
            lv_label_set_text(UI::details::lblSpeed, "");
        } else {
            lv_label_set_text(UI::details::lblSpeed, String(value).c_str());
        }
    }

    String nextRoad() {
        return hasNavigationData() ? details::nextRoad : "---";
    }

    void setNextRoad(const String& value) {
        if (value == details::nextRoad) return;

        if (!value.isEmpty()) {
            ThemeControl::flashScreen();
        }

        details::nextRoad = value;
        lv_label_set_text(UI::details::lblNextRoad, value.c_str());
    }

    String nextRoadDesc() {
        return hasNavigationData() ? details::nextRoadDesc : "---";
    }

    void setNextRoadDesc(const String& value) {
        if (value == details::nextRoadDesc) return;

        details::nextRoadDesc = value;
        lv_label_set_text(UI::details::lblNextRoadDesc, value.c_str());
    }

    String eta() {
        return hasNavigationData() ? details::eta : "---";
    }

    void setEta(const String& value) {
        if (value == details::eta) return;
        details::eta = value;
        lv_label_set_text(UI::details::lblEta, fullEta().c_str());
    }

    String ete() {
        return hasNavigationData() ? details::ete : "---";
    }

    void setEte(const String& value) {
        if (value == details::ete) return;
        details::ete = value;
        lv_label_set_text(UI::details::lblEta, fullEta().c_str());
    }

    String totalDistance() {
        return hasNavigationData() ? details::totalDistance : "---";
    }

    void setTotalDistance(const String& value) {
        if (value == details::totalDistance) return;
        details::totalDistance = value;
        lv_label_set_text(UI::details::lblEta, fullEta().c_str());
    }

    String distanceToNextTurn() {
        return hasNavigationData() ? details::distanceToNextTurn : "---";
    }

    void setDistanceToNextTurn(const String& value) {
        if (value == details::distanceToNextTurn) return;
        details::distanceToNextTurn = value;
        lv_label_set_text(UI::details::lblDistanceToNextRoad, value.c_str());
    }

    // ETA format
    String fullEta() {
        return ete() + " - " + totalDistance() + " - " + eta();
    }

    String displayIconHash() {
        return details::displayIconHash;
    }

    void setIconHash(const String& value) {
        if (value == details::displayIconHash) return;

        details::displayIconHash = value;

        if (value.isEmpty()) {
            setIconBuffer(nullptr, 0);
            return;
        }

        if (isIconExisted(value)) {
            loadIcon(value);
            return;
        }

        // icon will arrive via BLE
    }

    uint8_t* iconRenderBuffer() {
        return details::iconRenderBuffer;
    }

    void setIconBuffer(const uint8_t* value, const size_t& length) {
        if (!value || length == 0) {
            memset(details::iconRenderBuffer, 0xFF, sizeof(details::iconRenderBuffer));
            details::iconDirty = true;
            return;
        }

        if (length > ICON_BITMAP_BUFFER_SIZE) {
            Serial.println("Icon buffer overflow");
            return;
        }

        convert1BitBitmapToRgb565(details::iconRenderBuffer,
                                  value,
                                  ICON_WIDTH,
                                  ICON_HEIGHT,
                                  lv_color_to_u16(lv_color_make(0, 0, 255)),
                                  lv_color_to_u16(lv_color_make(255, 255, 255)));

        details::iconDirty = true;
    }

    // FILE FUNCTIONS
    void removeAllFiles() {
        File root = FS.open("/");
        File file = root.openNextFile();
        while (file) {
            FS.remove(file.path());
            file = root.openNextFile();
        }
    }

    void listFiles() {
        File root = FS.open("/");
        File file = root.openNextFile();

        details::availableIcons.clear();

        while (file) {
            String name = file.name();
            if (name.endsWith(".bin")) {
                String hash = name.substring(0, name.length() - 4);
                details::availableIcons.push_back(hash);
            }
            file = root.openNextFile();
        }
    }

    size_t readFile(const String& filename, uint8_t* buffer, const size_t bufferSize) {
        File file = FS.open(filename, FILE_READ);
        if (!file || file.isDirectory()) return 0;

        if (file.size() > bufferSize) return 0;

        size_t length = file.read(buffer, bufferSize);
        file.close();
        return length;
    }

    void writeFile(const String& filename, const uint8_t* buffer, const size_t& length) {
        File file = FS.open(filename, FILE_WRITE);
        if (!file) return;
        file.write(buffer, length);
        file.close();
    }

    bool isIconExisted(const String& iconHash) {
        return std::find(details::availableIcons.begin(),
                         details::availableIcons.end(),
                         iconHash)
               != details::availableIcons.end();
    }

    void saveIcon(const String& iconHash, const uint8_t* buffer) {
        if (isIconExisted(iconHash)) return;

        writeFile(String("/") + iconHash + ".bin", buffer, ICON_BITMAP_BUFFER_SIZE);
        details::availableIcons.push_back(iconHash);
    }

    void loadIcon(const String& iconHash) {
        if (!isIconExisted(iconHash)) return;

        readFile(String("/") + iconHash + ".bin",
                 details::iconBitmapBuffer,
                 ICON_BITMAP_BUFFER_SIZE);

        setIconBuffer(details::iconBitmapBuffer, ICON_BITMAP_BUFFER_SIZE);
    }

    void receiveNewIcon(const String& iconHash, const uint8_t* buffer) {
        if (iconHash == details::receivedIconHash) return;

        details::receivedIconHash = iconHash;
        memcpy(details::receivedIconBitmapBuffer, buffer, ICON_BITMAP_BUFFER_SIZE);
    }

    // Apply icon once per update cycle
    void update() {
        if (details::receivedIconHash.isEmpty()) return;

        if (!isIconExisted(details::receivedIconHash)) {
            saveIcon(details::receivedIconHash, details::receivedIconBitmapBuffer);
        }

        if (details::receivedIconHash == details::displayIconHash) {
            setIconBuffer(details::receivedIconBitmapBuffer, ICON_BITMAP_BUFFER_SIZE);
        }

        details::receivedIconHash = "";
    }

} // namespace Data


#endif // UI_H
