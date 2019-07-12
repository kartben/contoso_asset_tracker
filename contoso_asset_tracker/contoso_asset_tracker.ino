/*
// Read temperature and humidity data from an Arduino MKRENV shield.
// The data is then sent to Azure IoT Central for visualizing via MQTT
//
// See the readme.md for details on connecting the sensor and setting up Azure IoT Central to receive the data.
*/

#include "./configure.h"

#include <time.h>
#include <SPI.h>

#include <MKRGSM.h>
#define DEVICE_NAME "Arduino MKR1400 GSM"

GPRS gprs;
//GSM gsmAccess(DEBUG == 1);
GSM gsmAccess(false);
GSMModem modem;
GSMLocation location;
GSMClient gsmClient; 


#include <RTCZero.h>
#include <Arduino_MKRENV.h>

/*  You need to go into this file and change this line from:
      #define MQTT_MAX_PACKET_SIZE 128
    to:
      #define MQTT_MAX_PACKET_SIZE 2048
*/
#include <PubSubClient.h>

#include <Reset.h>

// this is an easy to use NTP Arduino library by Stefan Staub - updates can be found here https://github.com/sstaub/NTP
#include "./ntp.h"
#include "./sha256.h"
#include "./base64.h"
#include "./parson.h"
#include "./utils.h"

#include "./InternalStorage.h" // copied from Wifi101OTA
#include "./UrlParser.h"
#include <ArduinoHttpClient.h>

int getHubHostName(char *scopeId, char* deviceId, char* key, char *hostName);

String iothubHost;
String deviceId;
String mqttUrl;
String userName;
String password;

GSMSSLClient mqttSslClient;
PubSubClient *mqtt_client = NULL;

bool timeSet = false;
bool mqttConnected = false;

#define TELEMETRY_SEND_INTERVAL     15000     // telemetry data sent every 5 seconds
#define PROPERTY_SEND_INTERVAL      60000     // property data sent every 60 seconds
#define SENSOR_READ_INTERVAL        2000      // read sensors every 2 seconds

long lastTelemetryMillis = 0;
long lastPropertyMillis = 0;
long lastSensorReadMillis = 0;
#if USE_DISPLAY
long lastTimeRefreshMillis = 0;
#endif

int telemetrySendInterval = TELEMETRY_SEND_INTERVAL;

float tempValue = 0.0;
float humidityValue = 0.0;
float batteryValue = 0.0;
bool panicButtonPressed = false;

typedef struct  {
    int signalQuality;
    bool isSimDetected;
    bool isRoaming;
    char imei[20];
    char iccid[20];
    long up_bytes;
    long dn_bytes;
} GsmInfo;

GsmInfo gsmInfo { 0, false, false, "", "", 0, 0};

static const char PROGMEM SIGNAL_QUALITY_0[] = "NO_SIGNAL";
static const char PROGMEM SIGNAL_QUALITY_1[] = "POOR";
static const char PROGMEM SIGNAL_QUALITY_2[] = "FAIR";
static const char PROGMEM SIGNAL_QUALITY_3[] = "GOOD";
static const char PROGMEM SIGNAL_QUALITY_4[] = "EXCELLENT";
static const char PROGMEM SIGNAL_QUALITY_5[] = "EXCELLENT";
static const char* const SIGNAL_QUALITY_MAP[] PROGMEM = {SIGNAL_QUALITY_0, SIGNAL_QUALITY_1, SIGNAL_QUALITY_2, SIGNAL_QUALITY_3, SIGNAL_QUALITY_4, SIGNAL_QUALITY_5};

enum connection_status { DISCONNECTED, PENDING, CONNECTED, TRANSMITTING};

#define GPRS_STATUS 0
#define DPS_STATUS 1
#define MQTT_STATUS 2
connection_status servicesStatus[3] = { DISCONNECTED, DISCONNECTED, DISCONNECTED};


// MQTT publish topics
static const char PROGMEM IOT_EVENT_TOPIC[] = "devices/{device_id}/messages/events/";
static const char PROGMEM IOT_TWIN_REPORTED_PROPERTY[] = "$iothub/twin/PATCH/properties/reported/?$rid={request_id}";
static const char PROGMEM IOT_TWIN_REQUEST_TWIN_TOPIC[] = "$iothub/twin/GET/?$rid={request_id}";
static const char PROGMEM IOT_DIRECT_METHOD_RESPONSE_TOPIC[] = "$iothub/methods/res/{status}/?$rid={request_id}";

// MQTT subscribe topics
static const char PROGMEM IOT_TWIN_RESULT_TOPIC[] = "$iothub/twin/res/#";
static const char PROGMEM IOT_TWIN_DESIRED_PATCH_TOPIC[] = "$iothub/twin/PATCH/properties/desired/#";
static const char PROGMEM IOT_C2D_TOPIC[] = "devices/{device_id}/messages/devicebound/#";
static const char PROGMEM IOT_DIRECT_MESSAGE_TOPIC[] = "$iothub/methods/POST/#";

int requestId = 0;
int twinRequestId = -1;

// create a UDP object for NTP to use
UDP *udp = new GSMUDP();
// create an NTP object
NTP ntp(*udp);
// Create an rtc object
RTCZero rtc;
long timeZoneOffset = 0;

#include "trace.h"
#include "iotc_dps.h"


#if X509 && ARM_PELION
#include "pelion_certs.h"
#endif

void initModem()
{
    MODEM.begin(true);
    bool connected = false;

    __TRACEF__((char*)F("ICCID: %s\n"), modem.getICCID().c_str());
    delay(1000);

    servicesStatus[GPRS_STATUS] = PENDING;
    servicesStatus[DPS_STATUS] = DISCONNECTED;
    servicesStatus[MQTT_STATUS] = DISCONNECTED;
    __REFRESH_DISPLAY__ 
    while (!connected) {
        __TRACE__((char*)F("Registering on GSM network"));
        if ((gsmAccess.begin(PINNUMBER) == GSM_READY)) {
            __TRACEF__((char*)F("Attempting to connect to APN: %s \n"), GPRS_APN);
            if ((gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY)) {
                connected = true;
                // get current UTC time
                getTime();
                servicesStatus[GPRS_STATUS] = CONNECTED;
                __REFRESH_DISPLAY__
                location.begin();
                // trigger a first query of current location
                location.available();
            }
        }
        else {
            __TRACE__("Not connected to cellular network.");
            delay(250);
        }
    }

    readGSMInfo();

    #if USE_DISPLAY
    drawQrCode(gsmInfo.iccid);
    #endif
}

// get the time from NTP and set the real-time clock on the MKR10x0
void getTime() {
    #if USE_NTP
    __TRACE__(F("Getting time using NTP server"));

    ntp.begin();
    ntp.update();

    __TRACE__(ntp.formattedTime("NTP time: %d. %B %Y - %A %T"));

    rtc.setEpoch(ntp.epoch());
    timeZoneOffset = 0;
    #else
    __TRACE__(F("Getting time from GSM Network"));

    unsigned long epoch = gsmAccess.getTime();
    unsigned long localTime = gsmAccess.getLocalTime();
    rtc.setEpoch(epoch);

    #endif

    __TRACEF__((char*)F("RTC Time: %02d/%02d/%02d "
                        "%02d:%02d:%02d, offset=%+d\n"),
                            rtc.getMonth(),rtc.getDay(),rtc.getYear(),
                            rtc.getHours(),rtc.getMinutes(),rtc.getSeconds(), 
                            timeZoneOffset);

    timeSet = true;
}

void acknowledgeSetting(const char* propertyKey, const char* propertyValue, int version) {
        // for IoT Central need to return acknowledgement
        const static char* PROGMEM responseTemplate = "{\"%s\":{\"value\":%s,\"statusCode\":%d,\"status\":\"%s\",\"desiredVersion\":%d}}";
        char payload[1024];
        sprintf(payload, responseTemplate, propertyKey, propertyValue, 200, F("completed"), version);
        __TRACEF__((char*)F("Sending acknowledgement: %s\n\n"), payload);
        String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
        char buff[20];
        topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
        servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
        mqtt_client->publish(topic.c_str(), payload);
        servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
        requestId++;
}

void handleDirectMethod(String topicStr, String payloadStr) {
    String msgId = topicStr.substring(topicStr.indexOf("$RID=") + 5);
    String methodName = topicStr.substring(topicStr.indexOf(F("$IOTHUB/METHODS/POST/")) + 21, topicStr.indexOf("/?$"));
    __TRACEF__((char*)F("Direct method call:\n\tMethod Name: %s\n\tParameters: %s\n"), methodName.c_str(), payloadStr.c_str());
    String status = String(F("OK"));
    if (strcmp(methodName.c_str(), "DOWNLOAD_FIRMWARE") == 0) {
        JSON_Value *root_value = json_parse_string(payloadStr.c_str());
        JSON_Object *root_obj = json_value_get_object(root_value);
        const char *package_uri = json_object_get_string(root_obj, "package_uri");
        Serial.println(package_uri);
        UrlParser parser;
        UrlParserResult parsed_url;
        if (parser.parse(package_uri, parsed_url)) {
            // download package
            if (parsed_url.port == 0) {
                parsed_url.port = 80; // TODO URL might be https protocol so default port would be different...
            } 
            HttpClient httpClient = HttpClient(gsmClient, parsed_url.host.c_str(), parsed_url.port);
            Serial.print("Connecting to http://");
            Serial.print(parsed_url.host);
            Serial.print(":");
            Serial.print(parsed_url.port);
            Serial.println(parsed_url.path);
            

            int err = httpClient.get(parsed_url.path);
            int fileLength = 0 ;
            if (err == HTTP_SUCCESS) {
                err = httpClient.responseStatusCode();
                if (err == 200) { // HTTP 200 
                    unsigned long timeoutStart = millis();
                    if(httpClient.skipResponseHeaders() == HTTP_SUCCESS) {
                        if(InternalStorage.open()) {
                            char c;
                            // Whilst we haven't timed out & haven't reached the end of the body
                            while ((httpClient.connected() || httpClient.available()) &&
                                (!httpClient.endOfBodyReached()) &&
                                ((millis() - timeoutStart) < 30 * 1000)) { // timeout 30s
                                if (httpClient.available()) {
                                    c = (char)httpClient.read();
                                    InternalStorage.write(c);
                                    fileLength++ ;
                                    // We read something, reset the timeout counter
                                    timeoutStart = millis();
                                }
                            }
                        } else {
                            __TRACE__(F("InternalStorage::open() failed"));
                        }
                    }
                    httpClient.stop();
                    InternalStorage.close();
                    __TRACEF__("Successfully downloaded FW (size: %d bytes)\n", fileLength);
                } else {
                    __TRACE__(F("Cannot get HTTP response"));
                    Serial.println(err);
                }
            } else {
                __TRACE__(F("Connection failed"));
            }
        } else {
            __TRACE__(F("Can't parse URI"));
        }
        // acknowledge receipt of the command
        String response_topic = (String)IOT_DIRECT_METHOD_RESPONSE_TOPIC;
        char buff[20];
        response_topic.replace(F("{request_id}"), msgId);
        response_topic.replace(F("{status}"), F("200")); //OK
        servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
        mqtt_client->publish(response_topic.c_str(), "");
        servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
    } else if (strcmp(methodName.c_str(), "APPLY_FIRMWARE") == 0) {
        // acknowledge receipt of the command
        String response_topic = (String)IOT_DIRECT_METHOD_RESPONSE_TOPIC;
        char buff[20];
        response_topic.replace(F("{request_id}"), msgId);
        response_topic.replace(F("{status}"), F("200"));  //OK
        servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
        mqtt_client->publish(response_topic.c_str(), "");
        servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
        __TRACE__("/!\\ DEVICE WILL NOW REBOOT WITH NEW FIRMWARE /!\\");
        InternalStorage.apply();
        while (true);
    }
}

void handleCloud2DeviceMessage(String topicStr, String payloadStr) {
    __TRACEF__((char*)F("Cloud to device call:\n\tPayload: %s\n"), payloadStr.c_str());
}

void handleTwinPropertyChange(String topicStr, String payloadStr) {
    __TRACE__(payloadStr.c_str());
    // read the property values sent using JSON parser
    JSON_Value *root_value = json_parse_string(payloadStr.c_str());
    JSON_Object *root_obj = json_value_get_object(root_value);
    const char* propertyKey = json_object_get_name(root_obj, 0);
    double propertyValueNum;
    double version;
    if (strcmp(propertyKey, "telemetrySendInterval") == 0) {
        JSON_Object* valObj = json_object_get_object(root_obj, propertyKey);
        propertyValueNum = json_object_get_number(valObj, "value");
        version = json_object_get_number(root_obj, "$version");
        char propertyValueStr[8];
        itoa(propertyValueNum, propertyValueStr, 10);
        if (strcmp(propertyKey, "telemetrySendInterval") == 0) {
            telemetrySendInterval = propertyValueNum * 1000;
        }

        __TRACEF__("\n%s setting change received with value: %s\n", propertyKey, propertyValueStr);
        acknowledgeSetting(propertyKey, propertyValueStr, version);
    }
    json_value_free(root_value);
}

// callback for MQTT subscriptions
void callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = (String)topic;
    topicStr.toUpperCase();
    String payloadStr = (String)((char*)payload);
    payloadStr.remove(length);

    if (topicStr.startsWith(F("$IOTHUB/METHODS/POST/"))) { // direct method callback
        handleDirectMethod(topicStr, payloadStr);
    } else if (topicStr.indexOf(F("/MESSAGES/DEVICEBOUND/")) > -1) { // cloud to device message
        handleCloud2DeviceMessage(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/PATCH/PROPERTIES/DESIRED"))) {  // digital twin desired property change
        handleTwinPropertyChange(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/RES"))) { // digital twin response
        int result = atoi(topicStr.substring(topicStr.indexOf(F("/RES/")) + 5, topicStr.indexOf(F("/?$"))).c_str());
        int msgId = atoi(topicStr.substring(topicStr.indexOf(F("$RID=")) + 5, topicStr.indexOf(F("$VERSION=")) - 1).c_str());
        if (msgId == twinRequestId) {
            // twin request processing
            twinRequestId = -1;
            // output limited to 128 bytes so this output may be truncated
            __TRACEF__((char*)F("Current state of device twin:\n\t%s\n"), payloadStr.c_str());
            
            String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
            char buff[20];
            topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
            String payload = F("{ "
                            "\"manufacturer\": \""          DEVICE_PROP_MANUFACTURER         "\", "
                            "\"model\": \""                 DEVICE_PROP_DEVICE_MODEL         "\", "
                            "\"processorManufacturer\": \"" DEVICE_PROP_PROC_MANUFACTURER    "\", "
                            "\"processorType\": \""         DEVICE_PROP_PROC_TYPE            "\", "
                            "\"fwVersion\": \""             DEVICE_PROP_FW_VERSION           "\", "
                            "\"serialNumber\": \""          "{sn}"                           "\", "
                            "\"imei\": \""                  "{imei}"                         "\", "
                            "\"iccid\": \""                 "{iccid}"                        "\", "
                            "\"telemetrySendInterval\":"    "{sendInterval}"
                        "}");

            payload.replace("{sn}", chipId());
            payload.replace("{imei}", gsmInfo.imei);
            payload.replace("{iccid}", gsmInfo.iccid);
            payload.replace("{sendInterval}", String(telemetrySendInterval / 1000));
            servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;

            __TRACEF__((char*)F("Publishing full twin:\n\t%s\n"), payload.c_str());

            mqtt_client->publish(topic.c_str(), payload.c_str());
            servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
            requestId++;  

        } else {
            if (result >= 200 && result < 300) {
                __TRACEF__((char*)F("Twin property %d updated\n"), msgId);
            } else {
                __TRACEF__((char*)F("Twin property %d update failed - err: %d\n"), msgId, result);
            }
        }
    } else { // unknown message
        __TRACEF__((char*)F("Unknown message arrived [%s]\nPayload contains: %s"), topic, payloadStr.c_str());
    }
}

// connect to Azure IoT Hub via MQTT
void connectMQTT(String deviceId, String username, String password) {
    mqttSslClient.stop(); // force close any existing connection
    servicesStatus[MQTT_STATUS] = DISCONNECTED;
    __REFRESH_DISPLAY__

    __TRACE__(F("Starting IoT Hub connection"));
    int retry = 0;
    while(retry < 3 && !mqtt_client->connected()) {     
        servicesStatus[MQTT_STATUS] = PENDING;
        __TRACEF__((char*)F("MQTT connection attempt #%d"), retry+1);
        __REFRESH_DISPLAY__
        if (mqtt_client->connect(deviceId.c_str(), username.c_str(), password.c_str())) {
                __TRACE__(F("IoT Hub connection successful"));
                mqttConnected = true;
        } else {
            servicesStatus[MQTT_STATUS] = DISCONNECTED;
            __REFRESH_DISPLAY__
            __TRACEF__((char*)F("IoT Hub connection error. MQTT rc=%d"), mqtt_client->state());

            delay(2000);
            retry++;
        }
    }

    if(mqtt_client->connected()) {
        servicesStatus[MQTT_STATUS] = CONNECTED;
        __REFRESH_DISPLAY__
        // add subscriptions
        mqtt_client->subscribe(IOT_TWIN_RESULT_TOPIC);  // twin results
        mqtt_client->subscribe(IOT_TWIN_DESIRED_PATCH_TOPIC);  // twin desired properties
        String c2dMessageTopic = IOT_C2D_TOPIC;
        c2dMessageTopic.replace(F("{device_id}"), deviceId);
        mqtt_client->subscribe(c2dMessageTopic.c_str());  // cloud to device messages
        mqtt_client->subscribe(IOT_DIRECT_MESSAGE_TOPIC); // direct messages
    } else {
        servicesStatus[MQTT_STATUS] = DISCONNECTED;
        __REFRESH_DISPLAY__
    }
}

// create an IoT Hub SAS token for authentication
String createIotHubSASToken(char *key, String url, long expire){
    __TRACE__("Creating IoT Hub SAS Token");

    url.toLowerCase();
    String stringToSign = url + "\n" + String(expire);
    int keyLength = strlen(key);

    int decodedKeyLength = base64_dec_len(key, keyLength);
    char decodedKey[decodedKeyLength];

    base64_decode(decodedKey, key, keyLength);

    Sha256 *sha256 = new Sha256();
    sha256->initHmac((const uint8_t*)decodedKey, (size_t)decodedKeyLength);
    sha256->print(stringToSign);
    char* sign = (char*) sha256->resultHmac();
    int encodedSignLen = base64_enc_len(HASH_LENGTH);
    char encodedSign[encodedSignLen];
    base64_encode(encodedSign, sign, HASH_LENGTH);
    delete(sha256);

    return (char*)F("SharedAccessSignature sr=") + url + (char*)F("&sig=") + urlEncode((const char*)encodedSign) + (char*)F("&se=") + String(expire);
}

void readSensors() {
    tempValue = ENV.readTemperature();
    humidityValue = ENV.readHumidity();

    // read the input on analog pin 0:
    int sensorValue = analogRead(ADC_BATTERY);
    // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 4.3V):
    batteryValue = sensorValue * (4.3 / 1023.0);

    // #if USE_DISPLAY
    // char buffer[128];
    // sprintf(buffer, "Temperature: %.1fC - Humidity: %.0f%%", tempValue, humidityValue);
    // drawMessage(buffer);
    // #endif

}

void readGSMInfo() {
    String response;
    // signal quality, SIM detection etc.
    MODEM.send("AT+CIND?");
    if (MODEM.waitForResponse(100, &response) == 1) {
        char* pt;
        int i = 0;
        pt = strtok ((char*)response.c_str(),",");
        // ex: '+CIND: 5,0,0,0,0,0,0,0,0,0,0,0'
        // +CIND: ("battchg",(0-5)),("signal",(0-5)),("service",(0,1)),
        // ("sounder", (0,1)),("message",(0,1)),("call",(0,1)), ("roam",(0,1)),
        // ("smsfull",(0,1)),("gprs", (0-2)),("callsetup",(0-3)),("callheld",(0 ,1)),("simind",(0-2))
        while (pt != NULL) {
            switch(i) {
                case 1:     gsmInfo.signalQuality = atoi(pt); break;
                case 6:     gsmInfo.isRoaming = (atoi(pt) == 1); break;
                case 11:    gsmInfo.isSimDetected = (atoi(pt) == 1); break;
                default: break;
            }
            int a = atoi(pt);
            printf("%d - %d\n", i, a);
            pt = strtok (NULL, ",");
            i++;
        }
    }

    if(gsmInfo.iccid[0] == 0)
       strcpy(gsmInfo.iccid, modem.getICCID().c_str());
    if(gsmInfo.imei[0] == 0)    
       strcpy(gsmInfo.imei, modem.getIMEI().c_str());

    // read up_bytes and dn_bytes
    // MODEM.send("AT+UGCNTRD");
    // if (MODEM.waitForResponse(100, &response) == 1) {
    //     Serial.println(response);
    // }

    __REFRESH_DISPLAY__
}

// http://atmel.force.com/support/articles/en_US/FAQ/Reading-unique-serial-number-on-SAM-D20-SAM-D21-SAM-R21-devices
String chipId() {
  volatile uint32_t val1, val2, val3, val4;
  volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
  val1 = *ptr1;
  volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
  val2 = *ptr;
  ptr++;
  val3 = *ptr;
  ptr++;
  val4 = *ptr;

  char buf[33];
  sprintf(buf, "%8x%8x%8x%8x", val1, val2, val3, val4);
  return String(buf);
}

void setup() {

    // Blink on startup to help with identifying that app is running
    pinMode(LED_BUILTIN, OUTPUT);
    for(int i = 0 ; i < 10 ; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
    }

    Serial.begin(115200);    
    rtc.begin();

    delay(100);
    #if USE_DISPLAY
    initScreen();
    prepareScreenDisplay();
    digitalWrite(TFT_LED, LOW);    // turn TFT backlight on;
    #endif

    #if DEBUG && !DISPLAY
    delay(5000); //add a small delay to allow time for connecting serial moitor to get full debug output
    #endif

    __TRACEF__((char*)F("Starting up the %s device\n"), DEVICE_NAME);

    if (!ENV.begin()) {
        __TRACE__(F("Failed to initialize MKR ENV shield!"));
        while (1);
    }

    #if USE_DISPLAY
    readSensors();
    #endif

    initModem();

    #if X509
    // prepare X.509 certificates (e.g retrieve them from SIM card) 
    #if ARM_PELION
    // retrieve certificate and private key from SIM card
    delay(5000); // wait a bit, AT+CSIM commands may not work immediately as per u-blox AT Commands manual §16.1.1
    
    char buffer[2000];
    memset(buffer, 2000, 0);

    selectDF();
    delay(1000);

    int fileLength = 0 ;

    // read X.509 cert stored in the SIM card
    fileLength = readEF(0xA000, buffer);
    if (fileLength > 0) {
        // enable control flow
        MODEM.send("AT&K3");

        // store the cert in the modem
        MODEM.sendf("AT+USECMNG=0,1,\"%s\",%d", "MKRCert", fileLength);
        if (MODEM.waitForPrompt() != 1) {
            // failure
        } else {
            for(int i = 0 ; i < fileLength ; i++) {
                MODEM.write(buffer[i]);
            }
            String resp;
            MODEM.waitForResponse(5000, &resp);
            Serial.print("Result of cert loading:");
            Serial.println(resp);
        }
    }

    delay(1000);

    // read X.509 private key stored in the SIM card
    fileLength = readEF(0xA001, buffer);
    if (fileLength > 0) {
        // store the private key in the modem
        MODEM.sendf("AT+USECMNG=0,2,\"%s\",%d", "MKRKey", fileLength);
        if (MODEM.waitForPrompt() != 1) {
            // failure
        } else {
            for(int i = 0 ; i < fileLength ; i++) {
                MODEM.write(buffer[i]);
            }
            String resp;
            MODEM.waitForResponse(5000, &resp);
            Serial.print("Result of private key loading:");
            Serial.println(resp);
        }
    }

    // use Cert and Key for upcoming TLS sessions
    MODEM.sendf("AT+USECPRF=0,6,\"MKRKey\"");
    MODEM.waitForResponse();
    MODEM.sendf("AT+USECPRF=0,5,\"MKRCert\"");
    MODEM.waitForResponse();
    #endif
    #endif

    __TRACE__("Getting IoT Hub host from Azure IoT DPS");
    servicesStatus[DPS_STATUS] = PENDING;
    __REFRESH_DISPLAY__

    deviceId = modem.getICCID();
    //deviceId =  iotc_deviceId ;
    char hostName[64] = {0};
    
    #if !X509 && defined IOTC_GROUP_ENROLLEMENT
    // compute derived key
    int keyLength = strlen(iotc_enrollmentKey);
    int decodedKeyLength = base64_dec_len(iotc_enrollmentKey, keyLength);
    char decodedKey[decodedKeyLength];
    base64_decode(decodedKey, iotc_enrollmentKey, keyLength);
    Sha256 *sha256 = new Sha256();
    sha256->initHmac((const uint8_t*)decodedKey, (size_t)decodedKeyLength);
    sha256->print(deviceId.c_str());
    char* sign = (char*) sha256->resultHmac();
    memset(iotc_enrollmentKey, 0, sizeof(iotc_enrollmentKey)); 
    base64_encode(iotc_enrollmentKey, sign, HASH_LENGTH);
    delete(sha256);
    #endif

    getHubHostName((char*)iotc_scopeId, (char*)deviceId.c_str(), (char*)iotc_enrollmentKey, hostName);
    servicesStatus[DPS_STATUS] = CONNECTED;
    __REFRESH_DISPLAY__

    iothubHost = hostName;
    __TRACEF__((char*)F("Hostname: %s\n"), hostName);

    // create SAS token and user name for connecting to MQTT broker
    mqttUrl = iothubHost + urlEncode(String((char*)F("/devices/") + deviceId).c_str());
    String password = "";
    #if !X509
    long expire = rtc.getEpoch() + 864000;
    password = createIotHubSASToken(iotc_enrollmentKey, mqttUrl, expire);
    #endif
    userName = iothubHost + "/" + deviceId + (char*)F("/?api-version=2018-06-30");

    __REFRESH_DISPLAY__

   // connect to the IoT Hub MQTT broker
    mqtt_client = new PubSubClient(mqttSslClient);
    mqtt_client->setServer(iothubHost.c_str(), 8883);
    mqtt_client->setCallback(callback);

    __REFRESH_DISPLAY__
    connectMQTT(deviceId, userName, password);
    __REFRESH_DISPLAY__

    // request full digital twin update
    String topic = (String)IOT_TWIN_REQUEST_TWIN_TOPIC;
    char buff[20];
    topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
    twinRequestId = requestId;
    requestId++;
    servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
    mqtt_client->publish(topic.c_str(), "");
    servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;

    __REFRESH_DISPLAY__
    // initialize timers
    lastTelemetryMillis = millis();
    lastPropertyMillis = millis();
    #if USE_DISPLAY
    displayOnTime = millis();
    #endif 
}

// main processing loop
void loop() {
    #if USE_DISPLAY
    if (Touch_Event()== true) { 
        if (touchPressed == false) {
            if (displayOn) {
                if(p.x > 160 && p.x < 280 && p.y > 120 && p.y < 220) {
                    panicButtonPressed = true;
                    // force telemetry push 
                    lastTelemetryMillis += telemetrySendInterval;
                    tone(SND_PIN,440,100);
                }
            }
            displayOnTime = millis(); // reset BL timer       
        }
        touchPressed = true;  
    } else {
        touchPressed = false;
    }

    // //automatic display BL timeout
    if ((millis() - displayOnTime) > DISPLAY_TIMEOUT) {
        digitalWrite(TFT_LED, HIGH); // Backlight off
        displayOn = false; 
    } else {
        digitalWrite(TFT_LED, LOW); // Backlight on
        displayOn = true; 
    }

    if (millis() - lastSensorReadMillis > DISPLAY_REFRESH_INTERVAL) {
        __REFRESH_DISPLAY__
        lastTimeRefreshMillis = millis();
    }
    #endif

    if (!mqtt_client->connected()) {
        __TRACE__(F("MQTT connection lost"));
        // try to reconnect
        initModem();
        servicesStatus[DPS_STATUS] = CONNECTED; // we won't reprovision so provide feedback that last DPS call succeeded.
        __REFRESH_DISPLAY__;
        #if !X509
        // since disconnection might be because SAS Token has expired or has been revoked (e.g device had been blocked), regenerate a new one
        long expire = rtc.getEpoch() + 864000;
        password = createIotHubSASToken(iotc_enrollmentKey, mqttUrl, expire);
        #endif
        connectMQTT(deviceId, userName, password);
    }

    // give the MQTT handler time to do it's thing
    mqtt_client->loop();

    // read the sensor values
    if (millis() - lastSensorReadMillis > SENSOR_READ_INTERVAL) {
        readSensors();
        readGSMInfo();
        lastSensorReadMillis = millis();
    }
    
    // send telemetry values
    if (mqtt_client->connected() && millis() - lastTelemetryMillis > telemetrySendInterval) {
        __TRACE__(F("Sending telemetry ..."));
        String topic = (String)IOT_EVENT_TOPIC;
        topic.replace(F("{device_id}"), deviceId);
        String payload = F("{"
                                "\"temp\": {temp}, \"humidity\": {humidity}, "
                                "\"signal_strength\": \"{ss}\", \"batteryRemaining\": {bat}"
                                "{TAMPERED}"
                                "{LOCATION}"
                            "}");
        payload.replace(F("{temp}"), String(tempValue, 2));
        payload.replace(F("{humidity}"),  String(humidityValue, 2));
        payload.replace(F("{bat}"), String(batteryValue, 3));
        payload.replace(F("{ss}"), SIGNAL_QUALITY_MAP[gsmInfo.signalQuality]);

        if(panicButtonPressed) {
            payload.replace(F("{TAMPERED}"), ",\"tampered\": \"ALERT!!!\"");
            panicButtonPressed = false;
        } else {
            payload.replace(F("{TAMPERED}"), "");
        }

        if (location.available()) {
            payload.replace(F("{LOCATION}"), ",\"location\": { \"lat\": {lat}, \"lon\": {lon}}");
            payload.replace(F("{lat}"), String(location.latitude(), 7));
            payload.replace(F("{lon}"), String(location.longitude(), 7));
            panicButtonPressed = false;
        } else {
            payload.replace(F("{LOCATION}"), "");
        }

        __TRACEF__("\t%s\n", payload.c_str());
        servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
        mqtt_client->publish(topic.c_str(), payload.c_str());
        servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
        lastTelemetryMillis = millis();
        __TRACE__(F("Sending telemetry ... done."));
    }

    // send property updates
    if (mqtt_client->connected() && millis() - lastPropertyMillis > PROPERTY_SEND_INTERVAL) {
        __TRACE__(F("Sending digital twin properties ..."));

        String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
        char buff[20];
        topic.replace(F("{request_id}"), itoa(requestId, buff, 10));

        String payload;
        if (location.available()) {
            payload = F("{\"location\": { \"lat\": {lat}, \"lon\": {lon}}, \"batteryRemaining\": {bat}}");
            payload.replace(F("{lat}"), String(location.latitude(), 7));
            payload.replace(F("{lon}"), String(location.longitude(), 7));
         }
        else {
            payload = F("{ \"batteryRemaining\": {bat}}");
        }
        #if PNP
        //payload.replace(F("{interfaceId}"), String("$iotiuri:https://azureiot.com/interfaces/deviceInformation/1.0.0"));
        #endif
        payload.replace(F("{bat}"), String(batteryValue, 3));
        
        servicesStatus[MQTT_STATUS] = TRANSMITTING; __REFRESH_DISPLAY__;
        mqtt_client->publish(topic.c_str(), payload.c_str());
        servicesStatus[MQTT_STATUS] = CONNECTED; __REFRESH_DISPLAY__;
        __TRACE__(F("Sending digital twin properties ... done."));

        requestId++;

        lastPropertyMillis = millis();
    }
}
