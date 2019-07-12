/*______Define pins for ArduiTouch _______*/
#define TFT_CS   A3
#define TFT_DC   0
#define TFT_MOSI 8
#define TFT_RST  22
#define TFT_CLK  9
#define TFT_MISO 10
#define TFT_LED  A2  


#define HAVE_TOUCHPAD
#define TOUCH_CS A4
#define TOUCH_IRQ 1
/*_______End of pin definitions______*/

/*______Define colors _______*/
// Color definitions
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0 
#define WHITE    0xFFFF
#define GREY     0xC618
/*_______End of color definitions______*/

/*______Assign pressure_______*/
#define ILI9341_ULTRA_DARKGREY    0x632C      
#define MINPRESSURE 8
#define MAXPRESSURE 2000
/*_______Assigned______*/

/*____Calibrate TFT LCD_____*/
#define TS_MINX 511
#define TS_MINY 308
#define TS_MAXX 3700
#define TS_MAXY 3600
/*______End of Calibration______*/


#define SCREEN_WIDTH  ILI9341_TFTHEIGHT 
#define SCREEN_HEIGHT ILI9341_TFTWIDTH  

#define DISPLAY_TIMEOUT 90 * 60 * 1000

#define BASELINE_NOTIFICATION_BAR   20

// Position of QR code
#define QR_CODE_WIDTH   160
#define QR_CODE_X       10
#define QR_CODE_Y       40

// Position of signal "bars"
#define SIGNAL_QUALITY_X SCREEN_WIDTH - 30
#define SIGNAL_QUALITY_Y BASELINE_NOTIFICATION_BAR

#define DISPLAY_REFRESH_INTERVAL 500

// Position of date/time
#define TIME_X SCREEN_WIDTH - 95
#define TIME_Y BASELINE_NOTIFICATION_BAR

// Position of services status lights (Network, DPS, Hub)
#define SERVICES_STATUS_X SCREEN_WIDTH / 2 - 20
#define SERVICES_STATUS_Y BASELINE_NOTIFICATION_BAR
#define SERVICES_STATUS_RADIUS 5


#define SND_PIN 2 // REMOVEME
