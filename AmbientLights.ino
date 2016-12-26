/*
* AmbientLights.ino
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

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Hash.h>
#include "AmbientLights.h"

#include "helpers.h"

/* Output Drivers */
#include "ChannelDriver.h"
ChannelDriver       channels;           /* Channel object */

/* Public declarations */
AsyncWebServer      web(HTTP_PORT);     /* Web Server */

/* Web pages */
#include "page_config_channel.h"
#include "page_config_channel_mapping.h"
#include "page_demo_channel.h"

/* Common Web pages and handlers */
#include "page_root.h"
#include "page_admin.h"

void setup() {
    /* Setup serial log port */
    LOG_PORT.begin(115200);

    /* Initial pin states */
    pinMode(BUILT_IN_LED, OUTPUT); 
    digitalWrite(BUILT_IN_LED, HIGH); 
    
    pinMode(0, INPUT); 

    /* Generate and set hostname */
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    String hostname = "Ambient_" + String(chipId);
    WiFi.hostname(hostname);

    /* Enable SPIFFS */
    SPIFFS.begin();

    LOG_PORT.println("");
    LOG_PORT.print(F("Ambient Lights v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.println("");

    /* Load configuration from SPIFFS */
    loadConfig();

    /* Update Config and PCA9685 */
    updateConfig();

    lSetupFinishedMillis = millis();
}

/* Configure and start the web server */
void initWeb() {
    /* Handle OTA update from asynchronous callbacks */
    Update.runAsync(true);

    /* Heap status handler */
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    /* Config file handler for testing */
    web.serveStatic("/configfile", SPIFFS, "/config.json");

    /* JSON Config Handler */
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) {
        String jsonString;
        serializeConfig(jsonString);
        request->send(200, "text/json", jsonString);
    });

    /* AJAX Handlers */
    web.on("/rootvals", HTTP_GET, send_root_vals);
    web.on("/adminvals", HTTP_GET, send_admin_vals);

    web.on("/config/channelvals", HTTP_GET, send_config_channel_vals);
    web.on("/config/channelmappingvals", HTTP_GET, send_config_channel_mapping_vals);
    web.on("/demo/getbuttons", HTTP_GET, send_demo_button_vals);
        
    /* POST Handlers */
    web.on("/admin.html", HTTP_POST, send_admin_html, handle_fw_upload);
    web.on("/config/chmapdef", HTTP_POST, receive_config_channel_mapping_default);
    web.on("/demo/receivevals", HTTP_POST, send_demo_receive_vals);

    /* Static handler */
    web.on("/config_channel.html", HTTP_POST, send_config_channel_html);
    web.on("/config_ch_map.html", HTTP_POST, send_config_channel_mapping_html);
    web.on("/demo_channel.html", HTTP_POST, send_demo_channel_html);
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("channel.html");

    web.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Page '" + String(request->url()) + "' not found :( ");
    });

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

/* Configuration Validations */
void validateConfig() {
    /* Generic channel limits for channels */
    if (config.channel_count > CHANNEL_LIMIT)
        config.channel_count = CHANNEL_LIMIT;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    /*  */
    if (config.maxVal > 255)
        config.maxVal = 255;
    else if (config.maxVal < 0)
        config.maxVal = 0;

    if (config.interVal < 0)
        config.interVal = 0;

    if (config.slopeVal <= 0)
        config.slopeVal = 1;

}

void updateConfig() {
    /* Validate first */
    validateConfig();

    /* Initialize pca9685 */
    channels.begin(config.channel_count); 
    channels.setGamma(config.channel_gamma);
    channels.setupPCA9685(config.zero, config.channel_count);

    LOG_PORT.print(F("- Number of output channels: "));
    LOG_PORT.println(config.channel_count);
    
}

/* Load configugration JSON file */
void loadConfig() {
    /* Zeroize Config struct */
    memset(&config, 0, sizeof(config));

    /* Load CONFIG_FILE json. Create and init with defaults if not found */
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        saveConfig();
    } else {
        /* Parse CONFIG_FILE json */
        size_t size = file.size();
        if (size > CONFIG_MAX_SIZE) {
            LOG_PORT.println(F("*** Configuration File too large ***"));
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        if (!json.success()) {
            LOG_PORT.println(F("*** Configuration File Format Error ***"));
            return;
        }

        /* Device */
        config.id = json["device"]["id"].as<String>();

        /* Channel */
        config.maxVal        = json["channel"]["maxval"];
        config.interVal      = json["channel"]["interval"];
        config.slopeVal      = json["channel"]["slopeval"];
        config.channel_count = json["channel"]["channel_count"];
        config.channel_gamma = json["channel"]["gamma"];
        config.zero = json["channel"]["zero"];
        config.mapping = json["channel"]["mapping"].as<String>();

        /* Mapping */
        if (mapping) free(mapping);
        mapping = static_cast<uint16_t *>(malloc(config.channel_count*2));
        
        String value = String(config.mapping);
        uint16_t idx = 0;
        for ( uint16_t i = 0; i < config.channel_count; i++){
          idx = value.indexOf('|');
          if(idx > 0) {
            mapping[i] = value.substring(0,idx).toInt();
            value = value.substring(idx+1);
          }else if(value.length()){
            mapping[i] = value.toInt();
            value = "";
          }else{
            mapping[i] = i + 1;  
          }
        }
        
        LOG_PORT.println(F("- Configuration loaded."));
    }

    /* Validate it */
    validateConfig();
}

/* Serialize the current config into a JSON string */
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    /* Create buffer and root object */
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    /* Device */
    JsonObject &device = json.createNestedObject("device");
    device["id"] = config.id.c_str();

    /* Channel */
    JsonObject &channel = json.createNestedObject("channel");
    channel["maxval"] = config.maxVal;
    channel["interval"] = config.interVal;
    channel["slopeval"] = config.slopeVal;
    channel["channel_count"] = config.channel_count;
    channel["gamma"] = config.channel_gamma;
    channel["zero"] = config.zero;

    /* Mapping */
    String mappingStr = "";
    for (uint16_t i = 0; i < config.channel_count-1 ; i++)
       mappingStr += String(mapping[i]) + "|";
    mappingStr += String(mapping[config.channel_count-1]);
    channel["mapping"] = mappingStr.c_str();

    if (pretty)
        json.prettyPrintTo(jsonString);
    else
        json.printTo(jsonString);
}

/* Save configuration JSON file */
void saveConfig() {
    /* Update Config */
    updateConfig();

    /* Serialize Config */
    String jsonString;
    serializeConfig(jsonString, true, true);

    /* Save Config */
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

/* Main Loop */
void loop() {

    /* Reboot handler */
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    /* DEMO handler */
    if (demo) {
        switch (demo) {
          channels.enableOutput();
            case 1: /* ALL */
                for (int i = 0; i < config.channel_count; i++) {
                    channels.setValue(mapping[i]-1, demoChannelValue);
                }
                channels.show();
                // stay in demo1 mode
                //demo = 0;
                lWaitMillis = 0;
                break;
                
            case 2: /* ON-OFF */
                if( (long)( millis() - lWaitMillis ) >= 0){
                  lWaitMillis = millis() + demoChannelValue;                  
                  for (int i = 0; i < config.channel_count; i++) {
                      if(demoCounter)
                        channels.setValue(mapping[i]-1, 255);
                      else
                        channels.setValue(mapping[i]-1, 0);
                  }
                  channels.show();
                  if(demoCounter)
                    demoCounter = 0;
                  else
                    demoCounter = 1;
                }

                break;
                
            case 3: /* HOPPING */
                if( (long)( millis() - lWaitMillis ) >= 0){
                  lWaitMillis = millis() + demoChannelValue;                  
                  for (int i = 0; i < config.channel_count; i++) {
                      if(i == demoCounter)
                        channels.setValue(mapping[i]-1, 255);
                      else
                        channels.setValue(mapping[i]-1, 0);
                  }
                  channels.show();
                  demoCounter = ++demoCounter % config.channel_count;
                }
                break;
                
            case 4: /* FLIPPING */
                if( (long)( millis() - lWaitMillis ) >= 0){
                  lWaitMillis = millis() + demoChannelValue;                  
                  for (int i = 0; i < config.channel_count; i++) {
                      if(i%2 == demoCounter)
                        channels.setValue(mapping[i]-1, 255);
                      else
                        channels.setValue(mapping[i]-1, 0);
                  }
                  channels.show();
                  demoCounter = ++demoCounter % 2;
                }
                break;
                
            case 5: /* RANDOM */
                if( (long)( millis() - lWaitMillis ) >= 0){
                  lWaitMillis = millis() + demoChannelValue;                  
                  for (int i = 0; i < config.channel_count; i++) {
                    channels.setValue(mapping[i]-1, (uint8_t) random(0, 256));
                  }
                  channels.show();
                }
                break;
                
            case 6: /* single set */
                channels.show();
                // stay in demo6 mode
                // demo = 0;
                lWaitMillis = 0;
                break;
             
            case 0:
                Serial.println(F("No DEMO should not bring you to this line!"));
        }

        yield();
        return;
    }

    if( (long)( millis() - lSetupFinishedMillis - ( config.interVal * (config.channel_count + 1) +  2 * config.slopeVal)) <= 0 ){      
      for (int i = 0; i < config.channel_count; i++) {
          stepVal = (long)((millis() - lSetupFinishedMillis - config.interVal * i) * config.maxVal) / config.slopeVal;
          
          if(stepVal < 0)                                           channels.setValue(mapping[i]-1, 0);
          else if(stepVal >= config.maxVal) channels.setValue(mapping[i]-1, config.maxVal);
          else                                                      {channels.setValue(mapping[i]-1, (uint8_t) stepVal ); /*Serial.print("." + String( stepVal));*/ }
          
      }
      channels.show();
    }

    if(!WIFIsetUp && !digitalRead(0) ){
      
        /* Generate and set hostname */
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        String hostname = "Ambient_" + String(chipId);
        WiFi.hostname(hostname);
        
        WiFi.mode(WIFI_AP);
        String ssid = "Ambient_" + String(chipId);
        WiFi.softAP(ssid.c_str());
        Serial.println(WiFi.softAPIP()); 
                
        /* Configure and start the web server */
        initWeb();
    
        /* Setup mDNS / DNS-SD */
        //TODO: Reboot or restart mdns when config.id is changed?
        MDNS.setInstanceName(config.id + " (" + String(chipId) + ")");
        if (MDNS.begin(hostname.c_str())) {
            MDNS.addService("http", "tcp", HTTP_PORT);
        } else {
            LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
        }
    
        digitalWrite(BUILT_IN_LED, LOW);
        
        WIFIsetUp = true;
    }

    /* perform background stuff */
    yield();
    
}
