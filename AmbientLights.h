/*
* AmbientLights.h
*
* Project: Ambient Lights - An ESP8266 and E1.31 based LED strips driver
*  Copyright (c) 2016 Christoph Rafetzeder 
* 
*  Some parts of this project are forked from:
*   1.) http://www.esp8266.com/viewtopic.php?f=152&t=8814 from RichardS 
*   2.) https://github.com/forkineye/ESPixelStick from http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it. Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#ifndef AMBIENTLIGHTS_H_
#define AMBIENTLIGHTS_H_

#include "ChannelDriver.h"
#include "Wire.h"

#ifdef USE_E131
#include "_E131.h"
#endif

/* Name and version */
const char VERSION[] = "1.0";

/* Configuration file params */
const char CONFIG_FILE[] = "/config.json";
#define CONFIG_MAX_SIZE 2048    /* Sanity limit for config file */

#define HTTP_PORT       80      /* Default web server port */
#define BUILT_IN_LED    2       /* BUILTIN_LED */

#define EEPROM_BASE     0       /* EEPROM configuration base address */
#define UNIVERSE_LIMIT  512     /* Universe boundary - 512 Channels */
#define CHANNEL_LIMIT   992     /* Total channel limit - 16 channels per pca9685 * 62 (refer to pca9685 datasheet, available adresses */
#define CONNECT_TIMEOUT 15000   /* 15 seconds */
#define REBOOT_DELAY    100     /* Delay for rebooting once reboot flag is set */
#define LOG_PORT        Serial  /* Serial port for console logging */
#define UPD_INTERVAL    1000    /*  */

/* E1.33 / RDMnet stuff - to be moved to library */
#define RDMNET_DNSSD_SRV_TYPE   "draft-e133.tcp"
#define RDMNET_DEFAULT_SCOPE    "default"
#define RDMNET_DEFAULT_DOMAIN   "local"
#define RDMNET_DNSSD_TXTVERS    1
#define RDMNET_DNSSD_E133VERS   1

// PINOUT            // Pin  Function                      ESP-8266 Pin 
                     //  see http://escapequotes.net/wemos-d1-mini-pins-and-diagram/
#define PIN_D0 16    // D0   IO                            GPIO16
#define PIN_D1  5    // D1   IO, SCL                       GPIO5
#define PIN_D2  4    // D2   IO, SDA                       GPIO4
#define PIN_D3  0    // D3   IO,Pull-up                    GPIO0
#define PIN_D4  2    // D4   IO,pull-up, BUILTIN_LED       GPIO2
#define PIN_D5  14   // D5   IO, SCK                       GPIO14
#define PIN_D6  12   // D6   IO, MISO                      GPIO12
#define PIN_D7  13   // D7   IO, MOSI                      GPIO13
#define PIN_D8  15   // D8   IO,pull-down, SS              GPIO15

/* Configuration structure */
typedef struct {
    /* Device */
    String      id;             /* Device ID */

    long        maxVal;         /*  */
    long        interVal;       /*  */
    long        slopeVal;       /*  */
    uint16_t    channel_count;  /* Number of channels */

    /* Channels */
    bool        channel_gamma;   /* Use gamma map? */
    bool        zero;            /* Reset all PCA9685 values to zero during startup */
    String      mapping;         /* Map channels to PCBA output port */

} config_t;

/* Globals */
config_t            config;

uint16_t            *mapping;       /* Mapping Array: Map DMX (index) to Output */
bool                reboot = false; /* Flag to reboot the ESP */
long                stepVal           = 0;
bool                WIFIsetUp = false;

uint16_t             demoChannelValue = 1;     /* Initial demo value */
uint8_t              demo             = 0;     /* Demo type for sequences 0=off */
static unsigned long lWaitMillis      = 1;     /* Waiting time for demo sequences */
static unsigned long lSetupFinishedMillis = 0;     /* Timestamp setup() finished */

uint8_t              demoCounter      = 0;     /* Auxiliary value for demo sequences */

/* Called from web handlers */
void saveConfig();

/* Forward Declarations */
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void loadConfig();
int  initWifi();
void initWeb();
void updateConfig();

#endif /* AMBIENTLIGHTS_H_ */
