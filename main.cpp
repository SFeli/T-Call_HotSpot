#include <Arduino.h>
/**************************************************************
 * Kopie aus GitHub Xinyuan - LilyGo / TTGO-T-Call TinyGSM
 * Funktioniert mit meinen Einstellungen Version 09
 * Works with Hardware    TTGO-T-Call
 * Compile and Flash with LOLIN D32
 * for individual customizing to update:
 *  *ssid ..  Your desired AccessPoint-ID
 *  *password Your desired AccessPoint-Password
 *  simPIN[]  Your Pin of the SIM-Card
 *  https://github.com/zhouhan0126/WebServer-esp32/blob/master/src/Parsing.cpp
 *
 **************************************************************/
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800 has to be defined before the Tiny-Include
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Libraries which has to be installed
#include <Arduino.h>
#include <TinyGsmClient.h>         // Library by Volodymyr Shymanskyy version Y 0.11.7
#include <Wire.h>
#include <WiFiClient.h>   
#include <WiFi.h> 
#include <WiFiAP.h>        
#include "esp_log.h"

// Variables and desired credentials for AccessPoint.
const char *ssid     = "T-Call";             // Access Point - SSID (Name)
const char *password = "12345678";           // Access Point - Password
const int  APPort    = 8083;                 // Access Point - Port

// Your GPRS credentials (leave empty, if missing)
const char apn[]      = "TM";                // GPRS - Your APN
const char gprsUser[] = "";                  // GPRS - User
const char gprsPass[] = "";                  // GPRS - Password
const char simPIN[]   = "1503";              // GPRS - SIM card PIN code, if any

// TTGO T-Call pin definitions
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

#define SerialMonitor Serial          // HW-Serial Set serial for debug console to the serial Monitor
#define SerialAT Serial1              // HW-Serial Set serial for AT commands (to the module)

WiFiServer APserver(80);              // neu für AccessPoint
TinyGsm modem(SerialAT);              // Modem
TinyGsmClient Webclient(modem);       // Modem acts as a WebClient

char server[] = "vsh.pp.ua";             // Server details of demo Website
String web_page = "/TinyGSM/logo.txt";   // default page of website
const int  port = 80;                    // default port of demo Website    http://www.vsh.pp.ua/TinyGSM/logo.txt

// function declarations, which are implemented at bottom
void WiFiEvent(WiFiEvent_t);                // all events of WiFi - Server 
void process(WiFiClient);                   // the process routine
bool setPowerBoostKeepOn(int);              // function to set Boost-Power

char* TAG = "GSM_module";

/**
 * Mainroutine: Setup
 * Parameter:    no
 * Function:    initialisation
 *      1. start access point to connect a device to the ESP32
 *      2. set PowerBoost to modem
 *      3. initialize modem     
 *      4. get some status and print to console
*/
void setup() {
    SerialMonitor.begin(115200);         // Set console to baud rate 115200
    delay(200);

    WiFi.onEvent(WiFiEvent);             // set event-handler for WiFi - Server -> AccessPoint
    WiFi.softAP(ssid, password);         // define credentials
    WiFi.mode(WIFI_AP);                  // set WiFi-Module in AccessPoint - mode
    IPAddress myIP = WiFi.softAPIP();    // inform console about own IP-Adress
    APserver.begin(APPort);              // AccessPoint with defined Port 

    ESP_LOGI(TAG, "ESP32_TTGO_T_CALL_TinyGSM_09");
    ESP_LOGI(TAG, "AccessPoint started with IP address: %s:%d", myIP.toString(), APPort );
    delay(20);

// Keep power when running from battery Workaround for Modem Power
    Wire.begin(I2C_SDA, I2C_SCL);        // setup I2C 
    bool isOk = setPowerBoostKeepOn(1);
    if (isOk) {
        ESP_LOGI(TAG, "IP5306 PowerBoost is set");
    } else {
        ESP_LOGE(TAG, "IP5306 PowerBoost is not set");
    }
  
    pinMode(MODEM_PWKEY, OUTPUT);        // Set-up modem reset, enable, power pins
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);
  
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_POWER_ON, HIGH);

// Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
  
    ESP_LOGI(TAG, "Initializing modem...");
//    modem.restart();                        // To skip it, call init() instead of restart()
    modem.init();                           // if you don't need the complete restart
        delay(20);

// Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
         modem.simUnlock(simPIN);
    }

// inform console about Modem status and info
    String modemInfo = modem.getModemInfo();
    ESP_LOGI(TAG, "Modem Info: %s", modemInfo);
    ESP_LOGI(TAG, "Modem IMEI: %s", modem.getIMEI().c_str());
    ESP_LOGI(TAG, "Modem Connected: %d", modem.isGprsConnected());
    ESP_LOGI(TAG, "Modem Status:    %d", modem.getSimStatus());  
}

/**
 * Loop-Routine
 * check if a client is connected on WIFI-AP
 * -- > call routine Process with parameter "client"
*/
void loop() {
    WiFiClient client = APserver.available();   // listen for incoming clients
    if (client) {                               // if a client is connected to the ESP32 via wifi
        ESP_LOGI(TAG, "new client connected");
        process(client);                        // sub routine "process" to readout the desired webpage and call that page via modem - results to console and device
        delay(100);
        client.stop();                          // Close connection and free resources.
        ESP_LOGI(TAG, "client disconnected");
    }                    // from if Client
}                        // from loop()

/**
 * Handler of WIFI-Events
 * Input-Parameter:  event (decimal 0 - 100)
 * Function:         Log coresponding Text of the event
*/
void WiFiEvent(WiFiEvent_t event) {
    switch(event) {

    case SYSTEM_EVENT_WIFI_READY:
        ESP_LOGD(TAG,"WiFi-Event: 00 -> SYSTEM_EVENT_WIFI_READY");
        break;

    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGD(TAG,"WiFi-Event: 01 -> SYSTEM_EVENT_SCAN_DONE");
        break;

    case SYSTEM_EVENT_STA_START:
        ESP_LOGD(TAG,"WiFi-Event: 02 -> SYSTEM_EVENT_STA_START");
        break;

    case SYSTEM_EVENT_STA_STOP:
        ESP_LOGD(TAG,"WiFi-Event: 03 -> SYSTEM_EVENT_STA_STOP");
        break;

    case SYSTEM_EVENT_STA_CONNECTED:            //or STARTED ?
        ESP_LOGD(TAG,"WiFi-Event: 04 -> SYSTEM_EVENT_STA_CONNECTED");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGD(TAG,"WiFi-Event: 05 -> SYSTEM_EVENT_STA_DISCONNECTED");
        break;

    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
        ESP_LOGD(TAG,"WiFi-Event: 06 -> SYSTEM_EVENT_STA_AUTHMODE_CHANGE");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGD(TAG,"WiFi-Event: 07 -> SYSTEM_EVENT_STA_GOT_IP");
        break;

    case SYSTEM_EVENT_STA_LOST_IP:
        ESP_LOGD(TAG,"WiFi-Event: 08 -> SYSTEM_EVENT_STA_LOST_IP");
        break;

    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
        ESP_LOGD(TAG,"WiFi-Event: 10 -> ESP32 station wps succeeds in enrollee mode");
        break;

    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGD(TAG,"WiFi-Event: 12 -> ESP32 station wps timeout in enrollee mode");
        break;

    case SYSTEM_EVENT_STA_WPS_ER_PIN:
        ESP_LOGD(TAG,"WiFi-Event: 13 -> ESP32 station wps pin code in enrollee mode");
        break;

    case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:
        ESP_LOGD(TAG,"WiFi-Event: 14 -> ESP32 station wps overlap in enrollee mode");
        break;

    case SYSTEM_EVENT_AP_START:
        ESP_LOGD(TAG,"WiFi-Event: 15 -> SYSTEM_EVENT_AP_START");
        break;

    case SYSTEM_EVENT_AP_STOP:
        ESP_LOGD(TAG,"WiFi-Event: 16 -> SYSTEM_EVENT_AP_STOP");
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGD(TAG,"WiFi-Event: 17 -> SYSTEM_EVENT_AP_STACONNECTED");
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGD(TAG,"WiFi-Event: 18 -> SYSTEM_EVENT_AP_STADISCONNECTED");
        break;

    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGD(TAG,"WiFi-Event: 19 -> SYSTEM_EVENT_AP_STAIPASSIGNED");
        break;

    case SYSTEM_EVENT_AP_PROBEREQRECVED:
        ESP_LOGD(TAG,"WiFi-Event: 20 -> SYSTEM_EVENT_AP_PROBEREQRECVED");
        break;

    default:
        ESP_LOGD(TAG,"WiFi-Event: %d -> UNKNOWN EVENT ", event);
        break;
    }
}
/**
 * Function process
 * InputParameter:      Client
 *          1. read the request from the client
*/
void process(WiFiClient client) {
    ESP_LOGI(TAG,"new session started");
    while (client.connected()) {               // loop while the client's connected
        String currentLine = "";                   // make a String to hold incoming data from the client
        int retry = 0;
        if (client.available()) {
            do {
                currentLine = client.readStringUntil('\r');
                ++retry;
            } while (currentLine.length() == 0 && retry < 3);
        }

        if (currentLine.length() != 0) {
            ESP_LOGI(TAG, "CurrentLine: %s", currentLine.c_str());
            // Check to see if the client request was "GET /GET?vsh.pp.ua/Tiny ....":
            if (currentLine.endsWith(" HTTP/1.1")) {
                // First line of HTTP request looks like "GET /path HTTP/1.1"
                // Retrieve the "/path" part by finding the "?" or "/"
                uint addr_start = currentLine.indexOf('?') + 1;
                uint addr_midl  = currentLine.indexOf('/', (addr_start));              
                uint addr_end   = currentLine.indexOf("HTTP");
                ESP_LOGD(TAG, "start midl end: %d %d %d", addr_start, addr_midl, addr_end);

                String req_server = currentLine.substring(addr_start, addr_midl);
                ESP_LOGI(TAG, "request server and length %s %d", req_server, req_server.length());

                if (req_server.length() < 6) {
                    // No server defined -> Some Help
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();
                    // the content of the HTTP response follows the header:
                    client.print("Klick <a href=\"/GET?vsh.pp.ua/TinyGSM/logo.txt\">here</a> um das Logo aufzurufen.<br>");
                    // The HTTP response ends with another blank line:
                    client.println();
                    // break out of the while loop:
                    break;
                }

                String req_page = currentLine.substring(addr_midl, addr_end - 1);
                ESP_LOGI(TAG, "The requested Page is : %s", req_page.c_str());
                strcpy(server, req_server.c_str());
                web_page = req_page;

                // wenn eine WLAN-Verbindung aufgebaut ist !!!
                // Netzwerk initialisieren und Modem tiiii tööööö ti tit ti so wie bei alten Modem 14400 ... 
                ESP_LOGI(TAG,"Waiting for network ...");
                if (!modem.waitForNetwork(240000L)) {
                    ESP_LOGE(TAG,"Modem - network failed");
                    delay(10000);
                    return;
                }
                ESP_LOGI(TAG, "Modem network aviable");
                if (modem.isNetworkConnected()) {
                    ESP_LOGD(TAG,"Modem - network connected");
                }

                if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                    ESP_LOGE(TAG, "Modem APN-connection failed");
                    delay(10000);
                    return;
                }
                ESP_LOGI(TAG, "Modem APN-connection established with APN: %s", apn);

                bool modemInfo = modem.isGprsConnected();
                ESP_LOGI(TAG,"Modem connected: %d", modemInfo);

                if (!Webclient.connect(server, port)) {
                  ESP_LOGE(TAG,"Modem connection to server: %s failed", server);
                  delay(10000);
                  return;
                } 
                ESP_LOGI(TAG,"Modem connection to server: %s established", server);

                // Make a HTTP GET request:
                ESP_LOGD(TAG,"Performing HTTP GET request...");
                Webclient.print(String("GET ") + web_page + " HTTP/1.1\r\n");
                Webclient.print(String("Host: ") + server + "\r\n");
                Webclient.print("Connection: close\r\n\r\n");
                Webclient.println();
    
                unsigned long timeout = millis();
                while (Webclient.connected() && millis() - timeout < 10000L) {
                    // Print available data
                    while (Webclient.available()) {
                    char c = Webclient.read();
                    SerialMonitor.print(c);
                    client.print(c);        // Ausgabe gleich auf den Client
                    timeout = millis();
                    }
                }
                SerialMonitor.println();
                Webclient.stop();          // Verindung zum Web-Server abbauen
                ESP_LOGI(TAG,"Web-Server disconnected");
            }
        }
    }
}

/**
 * set Power Boost Keep On
*/
bool setPowerBoostKeepOn(int en)
{
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

