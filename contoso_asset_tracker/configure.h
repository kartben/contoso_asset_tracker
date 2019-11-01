#define DEBUG       1   // set to 1 to enable debug traces

#define USE_DISPLAY 1   // set to 1 if you want to use an LCD display
                        // when USE_DISPLAY=0, debug traces are written to Serial

#define USE_NTP     1   // set to 1 to use NTP for setting local time, 
                        // if USE_NTP=0, the device will get time info from cellular network

#define X509        0   // else auth will be using SYMKEY
#define ARM_PELION  0  
//#define IOTC_SINGLE_ENROLLMENT
#define IOTC_GROUP_ENROLLEMENT

// Azure IoT Central device information
static char PROGMEM iotc_enrollmentKey[] = "<replace with your IoT Central group key>";
static char PROGMEM iotc_scopeId[] = "<replace with your IoT Central Scope ID>";
static char PROGMEM iotc_modelId[] = "<replace with the model ID of your asset tracker>";

// GSM information
static char PROGMEM PINNUMBER[]     = "<replace with SIM card's PIN number, or empty string if no PIN is needed>";
// APN data
static char PROGMEM GPRS_APN[]      = "<replace with your APN>";
static char PROGMEM GPRS_LOGIN[]    = "<replace with your APN login or empty string if no login is needed";
static char PROGMEM GPRS_PASSWORD[] = "<replace with your APN password or empty string if no password is needed";

// **** DEVICE PROPERTIES ****
#define DEVICE_PROP_FW_VERSION          "1.0.0-20190704"
#define DEVICE_PROP_MANUFACTURER        "Arduino"
#define DEVICE_PROP_PROC_MANUFACTURER   "Microchip"
#define DEVICE_PROP_PROC_TYPE           "SAMD21"
#define DEVICE_PROP_DEVICE_MODEL        "MKR1400 GSM"
// ***************************
