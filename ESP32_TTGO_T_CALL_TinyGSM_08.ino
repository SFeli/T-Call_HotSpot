/**************************************************************
 * Kopie aus GitHub Xinyuan - LilyGo / TTGO-T-Call TinyGSM
 * Funktioniert mit meinen Einstellungen 
 * Works with Hardware    TTGO-T-Call
 * Compile and Flash with LOLIN D32
 * Update:
 *  *ssid ..  Your desired AccessPoint-ID
 *  *password Your desired AccessPoint-Password
 *  simPIN[]  Your Pin of the SIM-Card
 *  https://github.com/zhouhan0126/WebServer-esp32/blob/master/src/Parsing.cpp
 *
 **************************************************************/
// Libraries which has to be installed 
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800 has to be defined before the Tiny-Include
#include <Wire.h>
#include <TinyGsmClient.h>
#include "utilities.h"
#include <WiFiClient.h>   
#include <WiFi.h>         
#include <WiFiServer.h>

// Variables and desired credentials.
const char *ssid     = "T-Call";             // Access Point - SSID (Name)
const char *password = "HotSpot";            // Access Point - Password
const int APPort     = 8083;                 // Access Point -  Port

// Your GPRS credentials (leave empty, if missing)
const char apn[]      = "";                  // GPRS - Your APN
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

#define SerialMonitor Serial          // Set serial for debug console to the Serial Monitor
#define SerialAT Serial1              // Set serial for AT commands (to the module)
#define TINY_GSM_RX_BUFFER   1024     // Set RX buffer to 1Kb

// #define TINY_GSM_DEBUG SerialMonitor // Define the serial console for debug prints

WiFiServer APserver(80);                 // neu für AP
TinyGsm modem(SerialAT);
TinyGsmClient Webclient(modem);

char server[] = "vsh.pp.ua";             // Server details of demo Website
// String resource = "/TinyGSM/logo.txt";
String resource = "/windows-version.txt";
const int  port = 80;                    // Port if demo Website

void setup() {
    SerialMonitor.begin(115200);         // Set console baud rate
    SerialMonitor.println("Programm ESP32_TTGO_T_CALL_TinyGSM");  // Just to know which progm is running
    delay(10);
  
    WiFi.onEvent(WiFiEvent);             // set event-handler for WiFi
    WiFi.softAP(ssid, password);         // define credentials 
    IPAddress myIP = WiFi.softAPIP();    // inform console about own IP-Adress
    SerialMonitor.print("AccessPoint IP address: ");
    SerialMonitor.println(myIP);
    SerialMonitor.print(":");
    SerialMonitor.print(APPort);
    APserver.begin(APPort);              // AccessPoint with defined Port 
    SerialMonitor.println("WiFi Server started");  
  
// Keep power when running from battery
    Wire.begin(I2C_SDA, I2C_SCL);        // Workaround for Modem Power
    bool   isOk = setPowerBoostKeepOn(1);
    SerialMonitor.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  
    pinMode(MODEM_PWKEY, OUTPUT);        // Set-up modem reset, enable, power pins
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);
  
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_POWER_ON, HIGH);

// Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
  
    SerialMonitor.println("Initializing modem...");
//  modem.restart();                        // To skip it, call init() instead of restart()
    modem.init();                           // if you don't need the complete restart
    String modemInfo = modem.getModemInfo();
    SerialMonitor.print("ModemInfo: ");     // inform console about Modem info
    SerialMonitor.println(modemInfo);
  
// Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
         modem.simUnlock(simPIN);
    }
    SerialMonitor.print("Modemstatus: ");   // inform console about Modem info
    SerialMonitor.println(modem.getSimStatus());
}

void loop() {
// Wenn ein WLAN - Client mit dem AP verbunden ist 
     WiFiClient client = APserver.available();   // listen for incoming clients
     if (client) {                               // if you get a client,
         process(client);                        // Process request
         delay(10);
         client.stop();                          // Close connection and free resources.
         Serial.println("Client Disconnected.");
     }      // from if Client
}           // from loop()

void process(WiFiClient client) {
      Serial.println("New Session.");            // print a message out the serial port
      String currentLine = "";                   // make a String to hold incoming data from the client
      while (client.connected()) {               // loop while the client's connected
        if (client.available()) {                // if there's bytes to read from the client,
            char c = client.read();              // read a byte, then
            Serial.write(c);                     // print it out the serial monitor
            
            if (c == '\n') {                     // if the byte is a newline character
              // if the current line is blank, you got two newline characters in a row.
              // that's the end of the client HTTP request, so send a response:
              if (currentLine.length() == 0) {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();
                // the content of the HTTP response follows the header:
                client.print("Klick <a href=\"/vsh.pp.ua\">here</a> to read sample page.<br>");
                // The HTTP response ends with another blank line:
                client.println();
                // break out of the while loop:
                break;
              } else {                // if you got a newline, then clear currentLine:
                currentLine = "";
              }
            }
            
            else if (c != '\r') {     // if you got anything else but a carriage return character,
              currentLine += c;       // add it to the end of the currentLine
            }
            
            // Check to see if the client request was "GET //vsh.pp.ua":
            if (currentLine.endsWith(" HTTP/1.1")) {
                // First line of HTTP request looks like "GET /path HTTP/1.1"
                // Retrieve the "/path" part by finding the spaces or "/"
              int addr_start = currentLine.indexOf(' ');
              int addr_midl = currentLine.indexOf('/', addr_start + 2);              
              int addr_end = currentLine.indexOf('HTTP');
 /*           SerialMonitor.print("addr_start:  ");              
              SerialMonitor.println(addr_start);
              SerialMonitor.print("addr_midl:  ");   
              SerialMonitor.println(addr_midl);
              SerialMonitor.print("addr_end:  ");   
              SerialMonitor.println(addr_end);
              SerialMonitor.println();
              SerialMonitor.print("currentLine:  ");
              SerialMonitor.println(currentLine);     */
              String req_server = currentLine.substring(addr_start + 2, addr_midl);
              
              SerialMonitor.print("req_server.length():  ");   
              SerialMonitor.println(req_server.length());
              SerialMonitor.println(req_server);
              
              if (req_server.length() == 0) {
                // No server defined -> Some Help
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();
                // the content of the HTTP response follows the header:
                client.print("Klick <a href=\"/vsh.pp.ua/TinyGSM/logo.txt\">here</a> um das Logo aufzurufen.<br>");
                // The HTTP response ends with another blank line:
                client.println();
                // break out of the while loop:
                break;
              }
              String req_page = currentLine.substring(addr_midl, addr_end - 3);
              strcpy(server, req_server.c_str());
              resource = req_page;

              SerialMonitor.print("resource:  ");   
              SerialMonitor.println(resource);
              
              SerialMonitor.print("Connecting to ");
              SerialMonitor.print(server);
              if (!Webclient.connect(server, port)) {
                  SerialMonitor.println(" fail");
                  delay(10000);
                  return;
              } 
              SerialMonitor.println(" OK");

              // Make a HTTP GET request:
              SerialMonitor.println("Performing HTTP GET request...");
              Webclient.print(String("GET ") + resource + " HTTP/1.1\r\n");
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
                // Verindung zum Web-Server abbauen
              Webclient.stop();
              SerialMonitor.println(F("Web-Server disconnected"));
            }
        }
    }
}

void WiFiEvent(WiFiEvent_t event) {
// Handler of WIFI-Event 
    switch (event) {

    case SYSTEM_EVENT_WIFI_READY:
        Serial.println("SYSTEM_EVENT_WIFI_READY");
        break;

    case SYSTEM_EVENT_SCAN_DONE:
        Serial.println("SYSTEM_EVENT_SCAN_DONE");
        break;

    case SYSTEM_EVENT_STA_START:
        Serial.println("SYSTEM_EVENT_STA_START");
        break;

    case SYSTEM_EVENT_STA_STOP:
        Serial.println("SYSTEM_EVENT_STA_STOP");
        break;

    case SYSTEM_EVENT_STA_CONNECTED://or STARTED ?
        Serial.println("SYSTEM_EVENT_STA_CONNECTED");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("SYSTEM_EVENT_STA_DISCONNECTED");
        break;

    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
        Serial.println("SYSTEM_EVENT_STA_AUTHMODE_CHANGE");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("SYSTEM_EVENT_STA_GOT_IP");
        break;

    case SYSTEM_EVENT_STA_LOST_IP:
        Serial.println("SYSTEM_EVENT_STA_LOST_IP");
        break;

    case SYSTEM_EVENT_AP_START:
        Serial.println("SYSTEM_EVENT_AP_START");
        break;

    case SYSTEM_EVENT_AP_STOP:
        Serial.println("SYSTEM_EVENT_AP_STOP");
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        Serial.println("SYSTEM_EVENT_AP_STACONNECTED");
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        Serial.println("SYSTEM_EVENT_AP_STADISCONNECTED");
        modem.gprsDisconnect();
        SerialMonitor.println(F("GPRS disconnected"));
        // Do nothing forevermore
        SerialMonitor.println("www.thingsmobile.com user lora@felmayer.de");
        break;

    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        Serial.println("SYSTEM_EVENT_AP_STAIPASSIGNED");
        // wenn eine WLAN-Verbindung aufgebaut ist !!!
        // Netzwerk initialisieren und Modem tiiii tööööö ti tit ti so wie bei alten Modem 14400 ... 
        SerialMonitor.print("Waiting for network...");
        if (!modem.waitForNetwork(240000L)) {
            SerialMonitor.println(" fail");
            delay(10000);
            return;
        }
        SerialMonitor.println(" OK");
        if (modem.isNetworkConnected()) {
            SerialMonitor.println("Network connected");
        }
        SerialMonitor.print(F("Connecting to APN: "));
        SerialMonitor.print(apn);
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMonitor.println(" fail");
            delay(10000);
            return;
         }
         SerialMonitor.println(" OK");  
         break;

    case SYSTEM_EVENT_AP_PROBEREQRECVED:
        Serial.println("SYSTEM_EVENT_AP_PROBEREQRECVED");
        break;

    default:
        Serial.println("UNKNOWN EVENT: " + event);
        break;
    }
}
