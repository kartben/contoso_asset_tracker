#ifndef _CONTOSO_DISPLAY_H
#define _CONTOSO_DISPLAY_H

#include "./display_config.h"
#include "./delivery-truck-icon.h"
#include "./temperature-icon.h"
#include "./humidity-icon.h"
#include "./alarm-icon.h"
#include <qrcode.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#include "./opensans7pt7b.h"

#include <time.h>

#define __REFRESH_DISPLAY__   { refreshDisplay(); }

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS,  TOUCH_IRQ);
TS_Point p;
bool displayOn = true;
long displayOnTime = 0;
bool touchPressed = false;

long lastSensorRefreshMillis = 0;


enum connection_status;

bool Touch_Event() {
  if (touch.tirqTouched()) {
    if (touch.touched()) {   
        p = touch.getPoint(); 
        delay(1);
        p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
        p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);

        if (p.z > MINPRESSURE) {
            return true;
        }
    }
  } 
  return false;  
}

void initScreen() {
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);    // turn TFT backlight off;

    // initialize the TFT
    tft.begin(); 
    touch.begin();

    tft.setRotation(3);   // landscape mode
    touch.setRotation(3);  
    tft.fillScreen(BLACK);// clear screen 

    tft.setFont(&OpenSans_Regular7pt7b);
    tft.setCursor(0,0);
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
}

void drawTime() {
    char buff[64];
    time_t adjustedTime = rtc.getEpoch() + timeZoneOffset ;
    tm *adjustedTimeTm = gmtime(&adjustedTime);

    memset(buff, 0, sizeof(buff));
    strftime(buff, sizeof(buff), "%T", adjustedTimeTm);
    //sprintf(buff, "%02d:%02d:%02d",rtc.getHours(),rtc.getMinutes(),rtc.getSeconds());
    
    int16_t  x1, y1;
    uint16_t w, h;
    // clear previous time
    tft.setFont(&OpenSans_Regular7pt7b);
    tft.setTextSize(1);
    tft.setCursor(TIME_X, TIME_Y);    tft.getTextBounds("00:00:00", TIME_X, TIME_Y, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w+4, h, BLACK);
    tft.print(buff);
}

void drawMessage(const char* buffer) {
    tft.fillRect(0, SCREEN_HEIGHT, SCREEN_WIDTH, -20, BLACK);
    tft.setFont(&OpenSans_Regular7pt7b);
    tft.setTextSize(1);
    int16_t  x1, y1;
    uint16_t w, h;
    tft.getTextBounds(buffer, 0, 100, &x1, &y1, &w, &h);

    tft.setCursor(SCREEN_WIDTH / 2 - w / 2,SCREEN_HEIGHT - 6);
    tft.print(buffer);

    // use the opportunity to refresh the clock
    drawTime();
}

void drawMessage(const __FlashStringHelper *ifsh) {
  drawMessage(reinterpret_cast<const char *>(ifsh));
}

void drawMessageF(char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 128, fmt, args);
    va_end (args);
    drawMessage(buf);
}


void drawSignalQuality(int signalQuality) {
    for (int i = 0; i < 5; i++) {
        tft.fillRect(SIGNAL_QUALITY_X + (3*i), SIGNAL_QUALITY_Y, 3, -(4 + (2*i)), (i <= signalQuality) ? GREEN : GREY);
    }
}

void drawServicesStatus(connection_status statuses[]) {
    for (int i = 0 ; i < 3; i++) {
        uint16_t color;
        switch(statuses[i]) {
            case 0: color = RED;    break;
            case 1: color = YELLOW; break;
            case 2: color = GREEN;  break;
            case 3: color = BLUE;  break;
            default: color = RED;
        }
        tft.fillCircle(SERVICES_STATUS_X + SERVICES_STATUS_RADIUS + (i * SERVICES_STATUS_RADIUS * 2 + 6), SERVICES_STATUS_Y - SERVICES_STATUS_RADIUS, SERVICES_STATUS_RADIUS, color); 
    }
}

void drawSensorAndButtons() {
    tft.drawRGBBitmap(QR_CODE_X + QR_CODE_WIDTH + 20, QR_CODE_Y + 10, temperature_icon, 32, 32);
    tft.drawRGBBitmap(QR_CODE_X + QR_CODE_WIDTH + 20, QR_CODE_Y + 10 + (32 + 10), humidity_icon, 32, 32);
    tft.drawRGBBitmap(QR_CODE_X + QR_CODE_WIDTH + 30, QR_CODE_Y + 10 + (32 + 10) * 2, alarm_icon, 64, 64);
}

void drawSensorValues() {
    char buff[10];

    tft.setFont(&OpenSans_Regular7pt7b);

    tft.fillRect(QR_CODE_X + QR_CODE_WIDTH + 20 + 32, QR_CODE_Y + 10, 80, 32 + 10 + 32, BLACK);

    tft.setCursor(QR_CODE_X + QR_CODE_WIDTH + 20 + 32 + 5, QR_CODE_Y + 10 + 20);
    sprintf(buff, "%.2fC", tempValue);
    tft.print(buff);

    tft.setCursor(QR_CODE_X + QR_CODE_WIDTH + 20 + 32 + 5, QR_CODE_Y + 10 + (32 + 10) + 20);
    sprintf(buff, "%.2f%%", humidityValue);
    tft.print(buff);

}

void drawQrCode(char* codeToDisplay) {
    // display QR Code 
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(1)];
    qrcode_initText(&qrcode, qrcodeData, 1, ECC_LOW, codeToDisplay);

    // Top quiet zone
    uint8_t thickness = QR_CODE_WIDTH / qrcode.size;
    uint16_t lineLength = qrcode.size * thickness;
    uint8_t xOffset = QR_CODE_X + (QR_CODE_WIDTH-lineLength)/2;
    uint8_t yOffset = QR_CODE_Y + (QR_CODE_WIDTH-lineLength)/2;
    tft.fillRect(QR_CODE_X, QR_CODE_Y, QR_CODE_WIDTH, QR_CODE_WIDTH, WHITE);

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
        uint8_t q = qrcode_getModule(&qrcode, x, y);
        if (q) tft.fillRect(x * thickness + xOffset, y * thickness + yOffset, thickness, thickness, BLACK);
        }
    }
}

void refreshDisplay() {
    drawSignalQuality(gsmInfo.signalQuality);
    drawServicesStatus(servicesStatus);
    drawTime();
    if (millis() - lastSensorRefreshMillis  > SENSOR_READ_INTERVAL) {
        drawSensorValues();   
        lastSensorRefreshMillis = millis();
    }
}

void prepareScreenDisplay() {
    tft.drawRGBBitmap(10, BASELINE_NOTIFICATION_BAR - 16 + 4, delivery_truck_icon, 16, 16);

    tft.setFont(&OpenSans_Regular7pt7b);

    tft.setCursor(30, BASELINE_NOTIFICATION_BAR);
    tft.print("Contoso");

    drawSensorAndButtons();   
    drawQrCode("http://azureiot.com");
    refreshDisplay();
}

#endif