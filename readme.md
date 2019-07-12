# Contoso Art Shipping Asset Tracker

## About

Arduino MKRGSM1400 + MKRENV shield application that implements an asset tracking solution for a fictional art shipping company, Contoso Art Shipping. This application sends telemetry information such as temperature, humidity, etc. to Azure IoT Central using MKR1400's cellular modem. 

When combined with an LCD touchscreen (see e.g [Arduitouch enclosure](https://www.hwhardsoft.de/english/projects/arduitouch-mkr/)), the application provides a nice GUI for monitoring the status of the connection to Azure IoT Central, the GSM signal quality, as well as sensor information.

## Purchasing an Arduino MKRGSM1400

If you dont have an Arduino MKRGSM1400 you can purchase one from [Arduino Store](https://store.arduino.cc/usa/mkr-gsm-1400) or [Mouser](https://www2.mouser.com/ProductDetail/Arduino/ABX00018?qs=%2Fha2pyFaduhmYvnoJVWU32W04%2F60eHEJ%252B94cFnN%252BkXI=).

MKR ENV shield can be purchased from [Arduino Store](https://store.arduino.cc/usa/mkr-env-shield) or [Mouser](https://www2.mouser.com/ProductDetail/Arduino/ASX00011?qs=%2Fha2pyFaduihW07lKAnjJGztgJ19Y1I%2Fl9R2%2FAgQnsRhOqKdbbnOcw%3D%3D).

Arduitouch Touchscreen enclosure can be purchased from [Zihatec](https://www.hwhardsoft.de/english/webshop/arduitouch/#cc-m-product-11543776297).

## Installation

TBD

## Prerequisite

Install the Arduino IDE and the necessary drivers for the Arduino MKR1000 series of boards and ensure that a simple LED blink sketch compiles and runs on the board.  Follow the guide here https://www.arduino.cc/en/Guide/MKR1000

This code requires a couple of libraries to be installed for it to compile. To install an Arduino library open the Arduino IDE and click the "Sketch" menu and then "Include Library" -> "Manage Libraries".  In the dialog filter by the library name below and install the latest version.  For more information on installing libraries with Arduino see https://www.arduino.cc/en/guide/libraries. 

Libraries you need to install:

* MKRGSM
* Arduino_MKRENV
* RTCZero
* PubSubClient
* ArduinoHttpClient

If you are planning to use an LCD touchscreen display, you also need:

* Adafruit GFX Library
* Adafruit ILI9341
* XPT2046_Touchscreen
* QRCode

**Note** - We need to increase the payload size limit in PubSubClient to allow for the larger size of MQTT messages from the Azure IoT Hub.  Open the file at %HomePath%\Documents\Arduino\libraries\PubSubClient\src\PubSubClient.h in your favorite code editor.  Change the line (line 26 in current version):

``` C
#define MQTT_MAX_PACKET_SIZE 128
```

to:

``` C
#define MQTT_MAX_PACKET_SIZE 2048
```

Save the file and you have made the necessary fix.

To connect the device to Azure IoT Central you will need to provision an IoT Central application.  This is free for **seven days** but if you already have signed up for an Azure subscription and want to use pay as you go IoT Central is free as long as you have no more than **five devices** and do not exceed **1MB per month** of data.  

Go to https://apps.azureiotcentral.com/ to create an application (you will need to sign in with a Microsoft account identity you may already have one if you use Xbox, office365, Windows 10, or other Microsoft services).  

* Choose Trial or Pay-As-You-Go.
* Select the Sample DevKits template (middle box)
* Provide an application name and URL domain name 
* If you select Pay-As-You-Go you will need to select your Azure subscription and select a region to install the application into.  This information is not needed for Trial.
* Click "Create"

You should now have an IoT Central application provisioned so lets add a real device.  Click Device Explorer on the left.  You will now see three templates in the left hand panel (MXChip, Raspberry Pi, Windows 10 IoT Core).  We are going to use the MXChip template for this exercise to prevent having to create a new template.  Click "MXChip" and click the "+V" icon on the toolbar, this will present a drop down where we click "Real" to add a new physical device.  Give a name to your device and click "Create".  

You now have a device in IoT Central that can be connected to from the Arduino MKR1000/1010 device.  Proceed to wiring and configuration.

## Configuration

The `configure.h` file allows you to configure various settings. At a minimum, you will need to set the following constants:

### Cellular connection

``` CPP
// GSM information
static char PROGMEM PINNUMBER[]     = "<replace with SIM card's PIN number, or empty string if no PIN is needed>";
// APN data
static char PROGMEM GPRS_APN[]      = "<replace with your APN>";
static char PROGMEM GPRS_LOGIN[]    = "<replace with your APN login or empty string if no login is needed";
static char PROGMEM GPRS_PASSWORD[] = "<replace with your APN password or empty string if no password is needed";
```

### IoT Central

``` CPP
// Azure IoT Central device information
static char PROGMEM iotc_enrollmentKey[] = "<replace with your IoT Central group key>";
static char PROGMEM iotc_scopeId[] = "<replace with your IoT Central Scope ID>";
static char PROGMEM iotc_modelId[] = "<replace with the model ID of your asset tracker>";
```

## Compiling and running

Load the contoso_asset_tracker\contoso_asset_tracker.ino file into the Arduino IDE and click the Upload button on the toolbar.  The code should compile and be uploaded to the device.  In the output window you should see:


```
Sketch uses 71816 bytes (27%) of program storage space. Maximum is 262144 bytes.
Atmel SMART device 0x10010005 found
Device       : ATSAMD21G18A
Chip ID      : 10010005
Version      : v2.0 [Arduino:XYZ] Dec 20 2016 15:36:43
Address      : 8192
Pages        : 3968
Page Size    : 64 bytes
Total Size   : 248KB
Planes       : 1
Lock Regions : 16
Locked       : none
Security     : false
Boot Flash   : true
BOD          : true
BOR          : true
Arduino      : FAST_CHIP_ERASE
Arduino      : FAST_MULTI_PAGE_WRITE
Arduino      : CAN_CHECKSUM_MEMORY_BUFFER
Erase flash
done in 0.846 seconds

Write 72472 bytes to flash (1133 pages)

[=                             ] 5% (64/1133 pages)
[===                           ] 11% (128/1133 pages)
[=====                         ] 16% (192/1133 pages)
[======                        ] 22% (256/1133 pages)
[========                      ] 28% (320/1133 pages)
[==========                    ] 33% (384/1133 pages)
[===========                   ] 39% (448/1133 pages)
[=============                 ] 45% (512/1133 pages)
[===============               ] 50% (576/1133 pages)
[================              ] 56% (640/1133 pages)
[==================            ] 62% (704/1133 pages)
[====================          ] 67% (768/1133 pages)
[======================        ] 73% (832/1133 pages)
[=======================       ] 79% (896/1133 pages)
[=========================     ] 84% (960/1133 pages)
[===========================   ] 90% (1024/1133 pages)
[============================  ] 96% (1088/1133 pages)
[==============================] 100% (1133/1133 pages)
done in 0.482 seconds

Verify 72472 bytes of flash with checksum.
Verify successful
done in 0.060 seconds
CPU reset.
```
The code is now running on the device and should be sending data to IoT Central.  We can look at the serial port monitor by clicking the Tool menu -> Serial Monitor (you may need to change the baud rate to 115200).  You should start to see output displayed in the window.  If you are using a MKR1000 and see the following messages output then there is an issue connecting to the IoT Hub via MQTT due to invalid certificates:

```
---> mqtt failed, rc=-2
```

To fix this we need to update the Wi-Fi firmware on the device to the latest version (19.5.4 for the MKR1000).  Follow the instructions here https://www.arduino.cc/en/Tutorial/FirmwareUpdater to update the firmware to the latest version (currently 19.5.4).  Then start this section from the beginning.

### Telemetry:
If the device is working correctly you should see output like this in the serial monitor that indicates data is successfully being transmitted to Azure IoT Central:

```
===> mqtt connected
Current state of device twin:
	{
  "desired": {
    "fanSpeed": {
      "value": 200
    },
    "$version": 16
  },
  "reported": {
    "dieNumber": 1,
    "fanSpeed": {
      "value": 200,
      "statusCode": 200,
      "status": "completed",
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.70}
Sending telemetry ...
	{"temp":   25.10, "humidity":   27.10}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.10, "humidity":   29.60}
--> IoT Hub acknowledges successful receipt of twin property: 1
Sending telemetry ...
	{"temp":   25.60, "humidity":   29.20}
Sending telemetry ...
	{"temp":   25.10, "humidity":   28.20}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.20, "humidity":   28.50}
--> IoT Hub acknowledges successful receipt of twin property: 2
Sending telemetry ...
	{"temp":   25.30, "humidity":   28.40}
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.70}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.30}
--> IoT Hub acknowledges successful receipt of twin property: 3
Sending telemetry ...
	{"temp":   25.20, "humidity":   28.00}
```

Now that we have data being sent lets look at the data in our IoT Central application.  Click the device you created and then select the temperature and humidity telemetry values in the Telemetry column.  You can turn on and off telemetry values by clicking on the eyeballs.  We are only sending temperature and humidity so no other telemetry items will be active.  You should see a screen similar to this:

![telemetry screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/telemetry.png)

### Properties:
The device is also updating the property "Die Number", click on the "Properties" link at the top and you should see the value in the Die Number change about ever 15 seconds.

![properties screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/properties.png)

### Settings:
The device will accept settings and acknowledge the receipt of the setting back to IoT Central.  Go to the "Settings" link at the top and change the value for Fan Speed (RPM), then click the "Update" button the text below the input box will briefly turn red then go green when the device acknowledges receipt of the setting.  In the serial monitor the following should be observed:

```
Fan Speed setting change received with value: 200
{"fanSpeed":{"value":200,"statusCode":200,"status":"completed","desiredVersion":9}}
--> IoT Hub acknowledges successful receipt of twin property: 1
```

The settings screen should look something like this:

![settings screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/settings.png)

### Commands:
We can send a message to the device from IoT Central.  Go to the "Commands" link at the top and enter a message into the Echo - Value to display text box.  The message should consist of only alpha characters (a - z) and spaces, all other characters will be ignored.  Click the "Run" button and watch your device.  You should see the LED blink morse code.  If you enter SOS the led should blink back ...---... where dots are short blinks and dashes slightly longer :-)

![commands screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/commands.png)

The morse code blinking LED is here on the MKR1000

![morse code blinking LED location](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/blink_led.png)
