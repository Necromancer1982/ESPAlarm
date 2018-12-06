// #####################################################################################
// #####################################################################################
// ### ESP8266 ESPAlarm                    #############################################
// ### Program for alerting FFW Biberbach  #############################################
// ### Copyright © 2018, Andreas S. Köhler #############################################
// ###                                                                        ##########
// ### This program is free software: you can redistribute it and/or modify   ##########
// ### it under the terms of the GNU General Public License as published by   ##########
// ### the Free Software Foundation, either version 3 of the License, or      ##########
// ### (at your option) any later version.                                    ##########
// ###                                                                        ##########
// ### This program is distributed in the hope that it will be useful,        ##########
// ### but WITHOUT ANY WARRANTY; without even the implied warranty of         ##########
// ### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          ##########
// ### GNU General Public License for more details.                           ##########
// ###                                                                        ##########
// ### You should have received a copy of the GNU General Public License      ##########
// ### along with this program.  If not, see <http://www.gnu.org/licenses/>.  ##########
// #####################################################################################
// #####################################################################################

// #####################################################################################
// ### Functions of ESPAlarm                                                         ###
// ###                                                                               ###
// ### Detectable alarm types:                                                       ###
// ###  - Fire alarm      ____|---|__|---|__|---|____ (Intervall 12s, Signal 60s)    ###
// ###    * Testalarm (between 10:00 and 11:00 first Saturday each month, excluded   ###
// ###      puplic holidays, if necessary shift instant one week) also detectable    ###
// ###  - Desaster alarm  ____|-|_|-|_|-|_|-|_|-|____ (Intervall 2s, Signal 60s)     ###
// ###  - All-Clear alarm ____|-----------------|____ (Continuous tone, 60s)         ###
// ###                                                                               ###
// ### + Built-in Enduser Setup to setup WiFi settings (initial operation)           ###
// ### + Built-in NTP-Client to get/sent Timestamp of alarm                          ###
// ### + Built-in Telegram Bot with following Instructionset:                        ###
// ###    * /info:    Shows status of ESPAlarm                                       ###
// ###    * /time:    Shows actual running time of ESPAlarm                          ###
// ###    * /help:    Shows Instuctionset with explanations                          ###
// ###    * /alerts:  Shows numbers of already happened alarms                       ###
// ###    * /delete:  Reset alarm counters                                           ###
// ###    * /reset:   Initiates software reset of ESPAlarm                           ###
// ###    * /alarmme: Activates / deactivates Telegram-Alarm sending                 ###
// ### + Alarm counter stored in EEPROM -> safe due to power fail                    ###
// ### + Built-in Status LED with following states.                                  ###
// ###    * green:          Every thing is fine, standby awaiting alert              ###
// ###    * blue:           Started AP-Mode, WiFi-Config possible                    ###
// ###    * pink blinking:  Not connected to WiFi, awaiting connection               ###
// ###    * orange:         WiFi-Manager timeout, rebooting module                   ###
// ###    * red flashing:   Alert, Telegram message will be send                     ###
// ###    * red blinking:   Alarm, waiting until rearmed                             ###
// ### + Built-in Arduino OTA flashing interface (Firmwareupdate via WiFi)           ###
// #####################################################################################

// ##############################################################################################################################################################################
// ### Definitions & Variables ##################################################################################################################################################
// ##############################################################################################################################################################################
// *** Needed Librarys **********************************************************************************************************************************************************
#include <ESP8266WiFi.h>                                                              // Library for ESP8266
#include <WiFiClientSecure.h>                                                         // Library to handle a secure WiFi-Client
#include <WiFiManager.h>                                                              // Library for EndUser-Setup
#include <Adafruit_NeoPixel.h>                                                        // Library for WS2812 LEDs
#include <EEPROM.h>                                                                   // Library for EEPROM-Access
#include <TimeLib.h>                                                                  // Library to handle Systemtime (Calendar)
#include <NtpClientLib.h>                                                             // Library for NTP-Usage
#include <UniversalTelegramBot.h>                                                     // Library for Telegram Bot
#include <ESP8266mDNS.h>                                                              // Library to handle DNS
#include <WiFiUdp.h>                                                                  // Library for UDP-Usage
#include <ArduinoOTA.h>                                                               // Library for OTA-Flash
extern "C" {
  #include "user_interface.h"
}
// *** Needed Variables *********************************************************************************************************************************************************
int numberOfInterrupts = 0;                                                           // Counter for Interrupts
int numberOfAlerts = 0;                                                               // Counter for real alerts
int numberOfTestAlerts = 0;                                                           // Counter for test alerts
int numberOfDesasterAlarms = 0;                                                       // Counter for desaster alarm
int number_phf;                                                                       // Public holiday flag as integer
static char hostname[] = "ESPTelegramAlarm";                                          // Networkname of Module                                    *** Hostname of Module ***
int loopcounter = 0;                                                                  // Loopcounterf for MainProgram
int cf = 0;                                                                           // WiFi Connection flag
int addr_alert = 0;                                                                   // Address of number of real alerts in EEPROM
int addr_test_alert = 1;                                                              // Address of number of test alerts in EEPROM
int addr_phf = 2;                                                                     // Address of public holiday flag in EEPROM
int addr_desaster_alarm = 3;                                                          // Address of number of desaster alarms in EEPROM
byte alerts = 0;                                                                      // Variable for EEPROM content (real alerts)
byte test_alerts = 0;                                                                 // Variable for EEPROM content (test alerts)
byte desaster_alarms = 0;                                                             // Variable for EEPROM content (desaster alarms)
byte phf = 0;                                                                         // Variable for EEPROM content (public holiday flag)
int WiFiManagerTimeout = 180;                                                         // Define Timeout for Wifi-Manager and set it to 180s       *** Timeout for WiFi-Manager ***
int8_t timeZone = 1;                                                                  // Definition of time zone                                  *** Actual Time Zone ***
int8_t minutesTimeZone = 0;                                                           // Definition of time zone minutes
bool wifiFirstConnected = false;                                                      // Flag for first WiFi connection
String AlertMessage = "leer";                                                         // Definition of string for alert message
String tag = "leer";                                                                  // Define variable for weekday
bool probealarm = false;                                                              // Flag for test alert
char dStr[50];                                                                        // Define variable for Date-String
char tStr[50];                                                                        // Define variable for Time-String
char dfph[50];                                                                        // Define variable for date in format xx.xx. (day + month)
bool deleteflag = false;                                                              // Flag for deleting number of alerts
bool resetflag = false;                                                               // Flag for reseting ESP
bool alarmmeflag = false;                                                             // Flag to toggle AlarmMe
bool firstalert = true;                                                               // Flag for very first alert
bool adminflag = false;                                                               // Define variable for Admin flag
bool intflag = false;                                                                 // Interrupt flag for every occurence
int NTPUpdateInterval = 28800;                                                        // Updateintervall for NTP                                  *** Updateintervall for NTP ***
String BotMessage = "";                                                               // Variable for message from bot
bool public_holiday_flag = false;                                                     // Flag to handle public holidays
String admins[2]={"<ADMIN_ID_1>", "<ADMIN_ID_2>"};                                    // ID of Admins (Andi, Sebastian)                           *** Chat-IDs of Admins ***
String andiid = "<SUPER_USER_ID>";                                                    // ID of Admin (Status messages)                            *** Chat-ID of Admin to get Status-Messages ***
unsigned long realert = 20000;                                                        // Minimum time between two alerts (90s -> 5x 12s + safetiness)                    
const int interruptPin = 0;                                                           // Define Interrupt Pin (GPIO 0)                            *** Pin for alart ***
volatile byte interruptCounter = 0;                                                   // Define Interrupt Counter
bool ButtonPressed = false;                                                           // Flag to store if button was pressed
bool ButtonReleased = false;                                                          // Flag to store if button was released
bool ActButtonPressed = false;                                                        // Flag for actual button status
bool InterruptHappened = false;                                                       // Flag if interrupt has occured
bool DesasterAlarm = false;                                                           // Flag if Desaster Alarm has occured
static unsigned long last_interrupt_time = 0;                                         // Set variable for time of last interrupt occurence to 0
String string1, string2, string3;                                                     // Define variables for alerm message
String pub_holi[9]={"01.01.", "06.01.", "01.05.", "08.08.", "15.08.", "03.10.", "01.11.", "25.12.", "26.12."};  // Array of strings, filld with dates of public holidays
bool AlarmMe = true;                                                                  // Flag for enableing Telegram-Alarm
String ProgVersion = "V1.2";                                                          // Actual Program Version                                   *** Version of Program ***
// *** Needed Constants *********************************************************************************************************************************************************
#define numbAdmins 2                                                                  // Number of Admins                                         *** Number of Admins ***
#define botName "<NAME OF BOT>"                                                       // Name of Telegram Bot                                     *** Name of Telegram-Bot ***
#define botUserName "<USERNAME OF BOT>"                                               // Bot Username                                             *** Username of Telegram-Bot ***
#define botToken "<TOKEN OF BOT>"                                                     // Bot Token                                                *** Token of Telegram-Bot ***
#define adminID "<CHAT-ID OF CHANEL TO BE INFORMED"                                   // ChatID of Chanel                                         *** Chat-ID of Chanel ***
#define PIN 2                                                                         // Pin for status LED                                       *** Pin for WS2812-Status-LED ***
#define NUMPIXELS 1                                                                   // Only one Status LED
// *** Needed Services **********************************************************************************************************************************************************
WiFiClientSecure client;                                                              // Create a secure client
UniversalTelegramBot bot(botToken, client);                                           // Create a telegram bot
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);   // Define Status LED
boolean syncEventTriggered = false;                                                   // True if a time event has been triggered
NTPSyncEvent_t ntpEvent;                                                              // Last triggered event

// ##############################################################################################################################################################################
// ### Calback for Wifi got IP ##################################################################################################################################################
// ##############################################################################################################################################################################
void onSTAGotIP (WiFiEventStationModeGotIP ipInfo) {                                  // Callback if WiFi connection is established and IP gotten
  wifiFirstConnected = true;                                                          // Set flag
}

// ##############################################################################################################################################################################
// ### Calback for NTP-Sync Event ###############################################################################################################################################
// ##############################################################################################################################################################################
void processSyncEvent (NTPSyncEvent_t ntpEvent) {                                     // Callback if NTP-Event happens
    if (ntpEvent) {                                                                   // If NTP-Error happens
        Serial.print ("Time Sync error: ");                                           // Debug printing
        if (ntpEvent == noResponse)                                                   // Dependent on kind of Error:
            Serial.println ("NTP server not reachable");                              // Debug printing
        else if (ntpEvent == invalidAddress)
            Serial.println ("Invalid NTP server address");
    } else {                                                                          // If no error occures
        Serial.print ("Got NTP time: ");                                              // Debug print Date/Time
        Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
    }
}

// ##############################################################################################################################################################################
// ### Calback for Wifi Config Mode #############################################################################################################################################
// ##############################################################################################################################################################################
void configModeCallback (WiFiManager *myWiFiManager) {                                // Callback if WiFi-Manager enters config mode
    Serial.println("Entered config mode");                                            // Debug printing
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
    pixels.setPixelColor(0, pixels.Color(0, 0, 20));                                  // Set status LED blue
    pixels.show();  
  }
  
// ##############################################################################################################################################################################
// ### Setup Routine ############################################################################################################################################################
// ##############################################################################################################################################################################
void setup() {
  static WiFiEventHandler e1;                                                         // Define WiFi-Handler
  e1 = WiFi.onStationModeGotIP (onSTAGotIP);                                          // As soon WiFi is connected, start NTP Client
// *** Initialize status LED ****************************************************************************************************************************************************  
  pixels.begin();                                                                     // Initialize NeoPixel library.
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));                                     // Status LED off
  pixels.show();                                                                      // Show status
// *** Initialize serial comunication *******************************************************************************************************************************************
  Serial.begin(115200);                                                               // Start serial communication
// *** Initialize WiFi **********************************************************************************************************************************************************
  delay(3000);                                                                        // Delay 3s
  wifi_station_set_hostname(hostname);                                                // Set station hostname
  WiFiManager wifiManager;                                                            // Define WiFi Manager
  wifiManager.setAPCallback(configModeCallback);                                      // Definition of callback for AP-Mode
  wifiManager.setConfigPortalTimeout(WiFiManagerTimeout);                             // Definition of timeout for AP-Mode
  if (!wifiManager.autoConnect("ESPAlarm")) {                                         // Start WiFi-Manager and check if connection is established, if not:
    Serial.println("Failed to connect and reboot");                                   // Debug printing
    pixels.setPixelColor(0, pixels.Color(255, 80, 0));                                // Set status LED orange
    pixels.show();                                                                    // Show status
    delay(3000);                                                                      // Wait 3s
    ESP.restart();                                                                    // Software reset ESP
    delay(5000);                                                                      // Wait 5s
  }
// *** Initialize inputs ********************************************************************************************************************************************************
  pinMode(interruptPin, INPUT_PULLUP);                                                // Define Pin0 as Interrupt Pin
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, CHANGE);      // Define Interrupt on Pin0 (Interrupt on Pin change)
// *** Serial printing status ***************************************************************************************************************************************************
  Serial.print("\n  Connecting to WiFi ");                                            // Debug printing
  pixels.setPixelColor(0, pixels.Color(0, 10, 0));                                    // If connected: Status green
  pixels.show();                                                                      // Show status color
  Serial.println("\n\nWiFi connected.");                                              // Debug printing
  cf = 1;                                                                             // Set WiFi Conection Flag
  Serial.print("  IP address: " + WiFi.localIP().toString() + "\n");                  // Debug printing
  Serial.print("  Host name:  " + String(hostname) + "\n");
  Serial.print("- - - - - - - - - - - - - - - - - - -\n\n");
  delay(3000);                                                                        // Wait 3s
// *** Initialize EEPROM ********************************************************************************************************************************************************
  EEPROM.begin(512);                                                                  // Initialize EEPROM with 512 Bytes size
  alerts = EEPROM.read(addr_alert);                                                   // Read EEPROM
  numberOfAlerts = (int) alerts;                                                      // Set global variable according EEPROM value
  test_alerts = EEPROM.read(addr_test_alert);                                         // Read EEPROM
  numberOfTestAlerts = (int) test_alerts;                                             // Set global variable according EEPROM value
  desaster_alarms = EEPROM.read(addr_desaster_alarm);                                 // Read EEPROM
  numberOfDesasterAlarms = (int) desaster_alarms;                                     // Set global variable according EEPROM value
  phf = EEPROM.read(addr_phf);                                                        // Read EEPROM
  number_phf = (int) phf;                                                             // Set global variable according EEPROM value
  EEPROM.end();                                                                       // Free RAM copy of structure
// *** Start Telegram Bot *******************************************************************************************************************************************************
  bot.sendMessage(andiid, "ESPAlarm online!", "");                                    // Send message "online" to Admins
// *** NTP-Initialisation *******************************************************************************************************************************************************  
  NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {                                      // Handler for NTP-Events
    ntpEvent = event;                                                                 // Store NTP-Error Event
    syncEventTriggered = true;                                                        // Set flag for NTP-Sync
  });
// *** OTA-Initialisation *******************************************************************************************************************************************************  
  ArduinoOTA.setHostname("ESPAlarm");                                                 // Set Hostname for OTA-Mode
  ArduinoOTA.onStart([]() {                                                           // OTA-Event onStart
    String type;                                                                      // Define string "type"
    if (ArduinoOTA.getCommand() == U_FLASH) {                                         // Dependent on flashtype
      type = "sketch";                                                                // Set type "sketch"
    } else { // U_SPIFFS
      type = "filesystem";                                                            // Set type "filesystem"
    }
    Serial.println("Start updating " + type);                                         // Debug print type
  });
  ArduinoOTA.onEnd([]() {                                                             // OTA-Event onEnd
    Serial.println("\nEnd");                                                          // Debug printing
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {               // OTA-Event onProgress
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));                    // Debug print
  });
  ArduinoOTA.onError([](ota_error_t error) {                                          // OTA-Event onError
    Serial.printf("Error[%u]: ", error);                                              // Debug print errornumber
    if (error == OTA_AUTH_ERROR) {                                                    // Dependent on errornumber
      Serial.println("Auth Failed");                                                  // Print error type
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();                                                                 // Start OTA
}

// ##############################################################################################################################################################################
// ### Interrupt Service Routine ################################################################################################################################################
// ##############################################################################################################################################################################
void handleInterrupt() {
  unsigned long interrupt_time = millis();                                            // Get systemtime when interrupt occures
  if ((interrupt_time - last_interrupt_time) > 200){                                  // Only detect interrupt if time between this and last interrupt > 200ms
    interruptCounter++;                                                               // Increment interrupt counter
    if (!digitalRead(interruptPin)) {                                                 // Check if Button is pressed
      ButtonPressed = true;                                                           // Write state of button to flag
    } else {
      ButtonReleased = true;                                                          // Write state of button to flag
    }
  }
  last_interrupt_time = interrupt_time;                                               // Set new time of last happened interrupt
}

// ##############################################################################################################################################################################
// ### Routine to get actual button state #######################################################################################################################################
// ##############################################################################################################################################################################
void ButtonState () {
  if (!digitalRead(interruptPin)) {                                                   // Check if Button is pressed
    ActButtonPressed = true;                                                          // Write state of button to flag
  } else {
    ActButtonPressed = false;                                                         // Write state of button to flag
  }
}

// ##############################################################################################################################################################################
// ### Routine to Handle Fire-/Testalarm ########################################################################################################################################
// ##############################################################################################################################################################################
void FireAlarm () {
// *** Check if today is a public holiday ***************************************************************************************************************************************
  if ((weekday() == 7) && (day() <= 7)) {                                             // If weekday is saturday and date <= 7 -> first saturday in month
    for (int i=0; i<=8; i++) {                                                        // Loop for all public holidays
      if (pub_holi[i] == String(dfph)) {                                              // Check if today is a public holiday
        Serial.println("Feiertag!");                                                  // If so, debug printing
        public_holiday_flag = true;                                                   // Set public holiday flag
        number_phf = 1;
        EEPROM.begin(512);                                                            // Initialize EEPROM with 512 Bytes size
        EEPROM.write(addr_phf, number_phf);                                           // Write number of public holiday flag to EEPROM
        delay(200);                                                                   // Wait 0.2s
        EEPROM.commit();                                                              // Transfer data to EEPROM
        EEPROM.end();                                                                 // Free RAM copy of structure
      }
    }
  }
// *** Set test alarm flag if first saturday isn't a public holiday *************************************************************************************************************
  if ((hour() == 10) && (weekday() == 7) && (day() <= 7)) {                           // Check if time is between 10:00 and 11:00 and weekday is saturday and date <= 7 -> first saturday in month
    EEPROM.begin(512);                                                                // Declare size of EEPROM
    phf = EEPROM.read(addr_phf);                                                      // Read EEPROM
    number_phf = (int) phf;                                                           // Set global variable according EEPROM value
    EEPROM.end();                                                                     // Free RAM copy of structure
    if (number_phf == 1) {                                                            // Set public holiday flag according EEPROM value
      public_holiday_flag == true;
    } else {
      public_holiday_flag == false;
    }
    if (public_holiday_flag == false) {                                               // If today is no public holiday
      probealarm = true;                                                              // Set test alert flag
    }
  }
// *** Set test alarm flag for second saturday if first was a public holiday ****************************************************************************************************       
  if ((hour() == 10) && (weekday() == 7) && ((day() >= 8) && (day() <= 14))) {        // Check if time is between 10:00 and 11:00 and weekday is saturday and date between 8th and 14th -> 2nd saturday in month
    EEPROM.begin(512);                                                                // Declare size of EEPROM
    phf = EEPROM.read(addr_phf);                                                      // Read EEPROM
    number_phf = (int) phf;                                                           // Set global variable according EEPROM value
    EEPROM.end();                                                                     // Free RAM copy of structure
    if (number_phf == 1) {                                                            // Set public holiday flag according EEPROM value
      public_holiday_flag == true;
    } else {
      public_holiday_flag == false;
    }
    if (public_holiday_flag == true) {                                                // If public holiday flag is set
      probealarm = true;                                                              // Set test alert flag
      public_holiday_flag = false;                                                    // Release public holiday flag
      number_phf = 0;
      EEPROM.begin(512);                                                              // Initialize EEPROM with 512 Bytes size
      EEPROM.write(addr_phf, number_phf);                                             // Write number of public holiday flag to EEPROM
      delay(200);                                                                     // Wait 0.2s
      EEPROM.commit();                                                                // Transfer data to EEPROM
      EEPROM.end();                                                                   // Free RAM copy of structure
    }
  }     
  if (probealarm) {                                                                   // If test alert flag is set
    string1 = String("Probealarm für die FFW Biberbach ! ! !");                       // Send message for test alert
    numberOfTestAlerts++;                                                             // Increment number of test alerts
    EEPROM.begin(512);                                                                // Initialize EEPROM with 512 Bytes size
    EEPROM.write(addr_test_alert, numberOfTestAlerts);                                // Write number of alerts (interrupts) to EEPROM
    delay(200);                                                                       // Wait 0.2s
    EEPROM.commit();                                                                  // Transfer data to EEPROM
    EEPROM.end();                                                                     // Free RAM copy of structure
    probealarm = false;                                                               // Reset test alaert flag
  } else {                                                                            // Else
    string1 = String("<b>Alarm für die FFW Biberbach ! ! !</b>");                     // Send message for real alert
    numberOfAlerts++;                                                                 // Increment number of real alerts
    EEPROM.begin(512);                                                                // Initialize EEPROM with 512 Bytes size
    EEPROM.write(addr_alert, numberOfAlerts);                                         // Write number of alerts (interrupts) to EEPROM
    delay(200);                                                                       // Wait 0.2s
    EEPROM.commit();                                                                  // Transfer data to EEPROM
    EEPROM.end();                                                                     // Free RAM copy of structure
  }
} 

// ##############################################################################################################################################################################
// ### Routine to Handle AllClear-Alarm ########################################################################################################################################
// ##############################################################################################################################################################################
void AllClear() {
  Serial.println ("All-Clear");                                                       // Debug printing
  DesasterAlarm = false;                                                              // Reset desaster alarm flag
  string1 = String("<b>Entwarnung für Katastrophenalarm ! ! !</b>");                  // Send message all clear
}

// ##############################################################################################################################################################################
// ### Routine to Handle Desaster-Alarm #########################################################################################################################################
// ##############################################################################################################################################################################
void Desaster() {
  string1 = String("<b>Katasrophenalarm! ! !\nRadio einschalten!</b>");               // Send message for desaster alert
  DesasterAlarm = true;                                                               // Set desaster alarm flag to detect All-Clear "alarm"
  ButtonReleased = false;                                                             // Reset button released flag
  numberOfDesasterAlarms++;                                                           // Increment number of test alerts
  EEPROM.begin(512);                                                                  // Initialize EEPROM with 512 Bytes size
  EEPROM.write(addr_desaster_alarm, numberOfDesasterAlarms);                          // Write number of alerts (interrupts) to EEPROM
  delay(200);                                                                         // Wait 0.2s
  EEPROM.commit();                                                                    // Transfer data to EEPROM
  EEPROM.end();                                                                       // Free RAM copy of structure
}
  
// ##############################################################################################################################################################################
// ### Main programm ############################################################################################################################################################
// ##############################################################################################################################################################################
void loop() {
  if (wifiFirstConnected) {                                                           // Start NTP if IP is gotten
    wifiFirstConnected = false;                                                       // Reset Flag, NTP-Initialisation just 1x
    NTP.begin ("pool.ntp.org", timeZone, true, minutesTimeZone);                      // Start NTP
    NTP.setInterval (NTPUpdateInterval);                                              // Set NTP-Intervall to 63s
  }
  if (syncEventTriggered) {                                                           // If Sync-Event, handle it
    processSyncEvent (ntpEvent);                                                      // Do syncronisation
    syncEventTriggered = false;                                                       // Reset Trigger-Flag
  }
  ArduinoOTA.handle();                                                                // Start OTA-Handle
// *** Check for occured interrupt will be done every cycle (0.1s) **************************************************************************************************************
  if (!AlarmMe) {
    goto NoAlarm;
  }
  if (interruptCounter > 0) {                                                         // If interrupt occured
    numberOfInterrupts++;                                                             // Increment nuber of interrupts
    Serial.print("An alert has occured. Total: ");                                    // Debug printing
    Serial.println(numberOfInterrupts);                                               // Debug printing
    sprintf (tStr, "%02d:%02d:%02d", hour(), minute(), second());                     // Get time as formatted string
    sprintf (dStr, "%02d.%02d.%4d", day(), month(), year());                          // Get date as formatted string
    sprintf (dfph, "%02d.%02d.", day(), month());                                     // Get actual date in format xx.xx. (day + month)
    unsigned long act_time = millis();                                                // Get actual systemtime
    unsigned long start_time = millis();                                              // Get systemtime when interrupt occures
    while (millis() <= (start_time + 500)) {                                          // Wait for 0,5s
      delay(50);                                                                      // Delay
      Serial.print(".");                                                              // Debug printing
    }
    Serial.print("\n");
    ButtonState();                                                                    // Get actual state of Button
    if (!ActButtonPressed) {                                                          // If button no more pressed (after0,5s)
      goto NoAlarm;                                                                   // No Alarm!! -> goto Bot,GetNewMessage
    }
    while (millis() <= (start_time + 3000)) {                                         // Wait for 3s
      delay(100);                                                                     // Delay
      Serial.print(".");                                                              // Debug printing
    }
    Serial.print("\n");
    ButtonState();                                                                    // Get actual state of Button
    if (ActButtonPressed) {                                                           // If button still pressed (after 3s)
      if (DesasterAlarm) {                                                            // And if Desaster Alarm was decleared
        while (millis() <= (start_time + 15000)) {                                    // Wait until 15s passed after interrupt occurence
          delay(500);                                                                 // Delay
          Serial.print(".");                                                          // Debug printing
        }
        Serial.print("\n");
        ButtonState();                                                                // Get actual state of Button
        if (ActButtonPressed) {                                                       // If button still pressed (after 15s) -> ALL-CLEAR-ALARM
          Serial.println("Entwarung");                                                // Debug printing
          AllClear();                                                                 // Call All-Clear-Routine
          DesasterAlarm = false;                                                      // Reset Desaster declaration
        } else {                                                                      // If after 15s button isn't longer pressed -> FIRE-ALARM
          Serial.println("Feueralarm");                                               // Debug printing
          FireAlarm();                                                                // Call Fire-Alarm-Routine
        }
      } else {                                                                        // If no desaster alarm is decleared, direct FIRE-ALARM 
        Serial.println("Feueralarm");                                                 // Debug printing
        FireAlarm();                                                                  // Call Fire-Alarm-Routine
      }
    } else {                                                                          // If 3s after interrupt, button not longer pressed -> DESASTER-ALARM
      Serial.println("Katastrophenalarm");                                            // Debug printing
      Desaster();                                                                     // Call Desaster-Alarm-Routine
      DesasterAlarm = true;                                                           // Decleare desaster alarm
    }
    string2 = String("\n");
    switch(weekday()){                                                                // Switch/Case to get name of weekday
      case 1:
        tag = "Sonntag";
      break;
      case 2:
        tag = "Montag";
        break;
      case 3:
        tag = "Dienstag";
      break;
      case 4:
        tag = "Mittwoch";
      break;
      case 5:
        tag = "Donnerstag";
      break;
      case 6:
        tag = "Freitag";
      break;
      case 7:
        tag = "Samstag";
      break;
    }
    string3 = String(tag) += String(", ") += String(dStr) += String("\n") += String(tStr) += String(" Uhr");  // Build Date/Time string
    AlertMessage = string1 += string2 += string3;                                     // Build alert message
    bot.sendMessage(adminID, AlertMessage, "HTML");                                   // Send alert to Bot
    for (int x = 0; x < 10; x++) {                                                    // Show alert at status LED
      pixels.setPixelColor(0, pixels.Color(255, 0, 0));                               // Set color red
      pixels.show();                                                                  // show color
      delay(50);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));                                 // set no color
      pixels.show();
      delay(100);
    }
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(50);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(300);
    bool blinking = false;
    while (millis() <= (start_time + 65000)) {                                         // Wait until 65s have passed after interrupt occurence and rearme ESPAlarm
      delay(500);                                                                     // Delay
      Serial.print(".");                                                              // Debug printing
      if (blinking) {
        pixels.setPixelColor(0, pixels.Color(10, 0, 0));
        pixels.show();
        blinking = false;
      } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        pixels.show();
        blinking = true;
      }
    }
    pixels.setPixelColor(0, pixels.Color(0, 10, 0));
    pixels.show();
    Serial.print("\n");
    Serial.println("A R M E D");
    ButtonPressed = false;                                                            // Reset all button- and interrupt-related flags
    ButtonReleased = false;
    ActButtonPressed = false; 
  }
NoAlarm:
  interruptCounter = 0; 
  loopcounter++;                                                                      // Increment loopcounter
// *** Telegram Bot reading will be done every 10th cycle (1s) ******************************************************************************************************************
  if (loopcounter > 10) {                                                             // Every 10th loop
    loopcounter = 0;                                                                  // Reset Loopcounter
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);               // Get number of new messages
    for (int i = 0; i < numNewMessages; i++) {                                        // For every message
      String chat_id = String(bot.messages[i].chat_id);                               // Get ID of chat
      String text = bot.messages[i].text;                                             // Get message text
      for (int i = 0; i < numbAdmins; i++) {                                          // Check if chat_id is listed in Array of admins
        if (chat_id == admins[i]) {
          adminflag = true;                                                           // If so, communication will be acepted
        }
      }
      if (adminflag == true) {                                                        // Only Admin is allowed to talk to the bot
        adminflag = false;                                                            // Reset Admin flag
        Serial.print(text);                                                           // Debug printing
        if ((text == "/info") or (text == "/Info")) {                                 // ******************************************* Keyword "Info":
          BotMessage = "Hallo " + bot.messages[i].from_name + ",\n";                  // Build bot message
          BotMessage += "ESPAlarm ist online und stets bei ber Arbeit!\n\n";
          if (AlarmMe) {
            BotMessage += "Telegram-Alarmierung: <b>aktiviert</b>\n\n";
          } else {
            BotMessage += "Telegram-Alarmierung: <b>deaktiviert</b>\n\n";
          }
          BotMessage += "ESPAlarm " + ProgVersion + "  © 2018, Andreas S. Köhler";
          bot.sendMessage(chat_id, BotMessage, "HTML");                               // Send bot message
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/time") or (text == "/Time")) {                            // ******************************************* Keyword "Time":
          unsigned long runtime = millis() / 1000;                                    // Get Runtime in seconds
          int runtime_sec = runtime % 60;                                             // Calculate seconds
          int runtime_min = (runtime / 60) % 60;                                      // Calculate minutes
          int runtime_hr = (runtime / 3600) % 24;                                     // Calculate hours
          int runtime_d = (runtime / 86400) % 365;                                    // Calculate days
          int runtime_y = runtime / 31536000;                                         // Calculate years
          BotMessage = "Bei der Arbeit seit:\n  ";                                    // Build bot message
          BotMessage += String(runtime_y) + " Jahre\n  ";
          BotMessage += String(runtime_d) + " Tage\n  ";
          BotMessage += String(runtime_hr) + " Stunden\n  ";
          BotMessage += String(runtime_min) + " Minuten\n  ";
          BotMessage += String(runtime_sec) + " Sekunden";
          bot.sendMessage(chat_id, BotMessage, "");                                   // Send bot message
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/help") or (text == "/Help")) {                            // ******************************************* Keyword "Help":
          String BotMessage = "Folgende Befehle kenne ich:\n\n";                      // Build bot message
          BotMessage += "<b>/start</b>: startet den Bot\n";
          BotMessage += "<b>/reset</b>: führt Software-Reset des Moduls durch\n";
          BotMessage += "<b>/info</b>: zeigt den Bot-Status an\n";
          BotMessage += "<b>/time</b>: zeigt an wie lange der Bot schon aktiv läuft\n";
          BotMessage += "<b>/help</b>: zeigt diesen Hilfe-Dialog an\n";
          BotMessage += "<b>/alerts</b>: gibt die Anzahl der bisherigen Alarmierungen zurück\n";
          BotMessage += "<b>/alarmme</b>: Aktiviert / deaktiviert die Telegram-Alarmierung\n";
          BotMessage += "<b>/delete</b>: setzt den Alamierungszähler auf Null";
          bot.sendMessage(chat_id, BotMessage, "HTML");                               // Send bot message
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/alerts") or (text == "/Alerts")) {                        // ******************************************* Keyword "Alerts"
          EEPROM.begin(512);                                                          // Declare size of EEPROM
          alerts = EEPROM.read(addr_alert);                                           // Read number of alterts from EEPROM
          test_alerts = EEPROM.read(addr_test_alert);                                 // Read number of test alerts from EEPROM
          desaster_alarms = EEPROM.read(addr_desaster_alarm);                         // Read number of desaster alarms from EEPROM
          EEPROM.end();                                                               // Free RAM copy of structure
          BotMessage = "Bisher wurden:\n";                                            // Build bot message
          BotMessage += " - " + String(alerts) + " Alamierungen und\n";
          BotMessage += " - " + String(test_alerts) + " Sirenenproben ausgelöst!\n";
          BotMessage += " - " + String(desaster_alarms) + " Katastrophenalarme ausgelöst!\n";
          bot.sendMessage(chat_id, BotMessage, "");                                   // Send bot message
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/delete") or (text == "/Delete")) {                        // ******************************************* Keyword "Delete"
          String keyboardJson = "[[{ \"text\" : \"Ja\", \"callback_data\" : \"/ja\" }],";  // Build Keyboard JSON
          keyboardJson += "[{ \"text\" : \"Abbruch\", \"callback_data\" : \"/abbruch\" }]]";
          bot.sendMessageWithInlineKeyboard(chat_id, "Soll die Anzahl der Alamierungen zurückgesetzt werden?", "", keyboardJson);  // Send bot message including inline keyboard
          deleteflag = true;                                                          // Set delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if (text == "/start") {                                                  // ******************************************* Keyword "Start"
          String keyboardJson = "[[\"/alerts\", \"/delete\"],[\"/time\", \"/info\"]]";    // Build Keyboard JSON
          BotMessage = "Hallo " + bot.messages[i].from_name + ",\n";                  // Build bot message
          BotMessage += "willkommen bei ESPAlarm! Was kann ich für dich tun?\n\n";
          BotMessage += "/help zeigt dir was die Befehle bedeuten";
          bot.sendMessageWithReplyKeyboard(chat_id, BotMessage, "", keyboardJson, true);  // Send bot message including reply keyboard
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if (text == "/ja") {                                                     // ******************************************* Keyword "Ja"
          if (deleteflag == true) {                                                   // If delete flag is true:
            deleteflag = false;                                                       // Reset delete flag
            numberOfAlerts = 0;                                                       // Reset number of alerts
            numberOfTestAlerts = 0;                                                   // Reset number of test alerts
            numberOfDesasterAlarms = 0;                                               // Reset number of desaster alarms
            EEPROM.begin(512);                                                        // Declare size of EEPROM
            EEPROM.write(addr_alert, 0);                                              // Write "0" to alerts in EEPROM
            EEPROM.write(addr_test_alert, 0);                                         // Write "0" to test alerts in EEPROM
            EEPROM.write(addr_desaster_alarm, 0);                                     // Write "0" to desaster alarms in EEPROM
            delay(200);                                                               // Delay 200ms
            EEPROM.commit();                                                          // Commit EEPROM
            EEPROM.end();                                                             // Free RAM copy of structure
            bot.sendMessage(chat_id, "Anzahl der Alarme wurde gelöscht!", "");        // Delete: Yes!            
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");  // Message not understand
          }
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if (text == "/abbruch") {                                                // ******************************************* Keyword "Abbruch"
          if (deleteflag == true) {                                                   // If delete flag is false:
            bot.sendMessage(chat_id, "Abbruch: Anzahl der Alarme wurde nicht gelöscht!", "");  // Delete: Cancel!
            deleteflag = false;                                                       // Reset delete flag
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");       // Message not understand
          }
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/reset") or (text == "/Reset")) {                          // ******************************************* Keyword "Reset"
          String keyboardJson = "[[{ \"text\" : \"Ja\", \"callback_data\" : \"/j_reset\" }],";  // Build Keyboard JSON
          keyboardJson += "[{ \"text\" : \"Nein\", \"callback_data\" : \"/n_reset\" }]]";
          bot.sendMessageWithInlineKeyboard(chat_id, "Soll ein Software-Reset durchgeführt werden?", "", keyboardJson);  // Send bot message including inline keyboard
          resetflag = true;                                                           // Set reset flag
          deleteflag = false;                                                         // Reset delete flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if (text == "/j_reset") {                                                     // ******************************************* Keyword "j_reset"
          if (resetflag == true) {                                                    // If reset flag is true:
            resetflag = false;                                                        // Reset reset flag
            bot.sendMessage(chat_id, "Software-Reset wird initiiert!", "");           // Reset: Yes!            
            delay(2000);                                                              // Delay 2s
            ESP.restart();                                                            // Software reset ESP
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");  // Message not understand
          }
          deleteflag = false;                                                         // Reset delete flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if (text == "/n_reset") {                                                // ******************************************* Keyword "n_reset"
          if (resetflag == true) {                                                    // If reset flag is true:
            bot.sendMessage(chat_id, "Abbruch: Es wird kein Reset durchgeführt!", "");  // Reset: Cancel!
            resetflag = false;                                                        // Reset reset flag
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");   // Message not understand
          }
          deleteflag = false;                                                         // Reset delete flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
        else if ((text == "/alarmme") or (text == "/AlarmMe")) {                      // ******************************************* Keyword "AlarmMe"
          String keyboardJson = "[[{ \"text\" : \"Ja\", \"callback_data\" : \"/j_alarmme\" }],";  // Build Keyboard JSON
          keyboardJson += "[{ \"text\" : \"Nein\", \"callback_data\" : \"/n_alarmme\" }]]";
          if (AlarmMe) {
            bot.sendMessageWithInlineKeyboard(chat_id, "Soll die aktive Alarmierung über Telegram <b>deaktiviert</b> werden?", "HTML", keyboardJson);  // Send bot message including inline keyboard
          } else {
            bot.sendMessageWithInlineKeyboard(chat_id, "Soll die aktive Alarmierung über Telegram <b>aktiviert</b> werden?", "HTML", keyboardJson);  // Send bot message including inline keyboard
          }
          resetflag = false;                                                          // Set reset flag
          deleteflag = false;                                                         // Reset delete flag
          alarmmeflag = true;                                                         // Set AlarmMe flag
        }
        else if (text == "/j_alarmme") {                                              // ******************************************* Keyword "j_alarmme"
          if (alarmmeflag == true) {                                                  // If AlarmMe flag is true:
            alarmmeflag = false;                                                      // Reset AlarmMe flag
            if (AlarmMe) {
              bot.sendMessage(chat_id, "Alarmierung über Telegram deaktiviert!", ""); // AlarmMe: deactivated         
              AlarmMe = false;   
            } else {
              bot.sendMessage(chat_id, "Alarmierung über Telegram aktiviert!", "");   // AlarmMe: deactivated         
              AlarmMe = true;   
            }
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");  // Message not understand
          }
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
        }
        else if (text == "/n_alarmme") {                                              // ******************************************* Keyword "n_alarmme"
          if (alarmmeflag == true) {                                                  // If AlarmMe flag is true:
            if (AlarmMe) {
              bot.sendMessage(chat_id, "Abbruch: Die Telegram-Alarmierung bleibt aktiv!", "");  // AlarmMe: Cancel!
            } else {
              bot.sendMessage(chat_id, "Abbruch: Die Telegram-Alarmlierung bleibt deaktiviert!", "");  // AlarmMe: Cancel!
            }
            alarmmeflag = false;                                                      // Reset reset flag
          } else {
            bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");   // Message not understand
          }
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Set reset flag
        }
        else {
          bot.sendMessage(chat_id, "Alles verstehe ich nun auch wieder nicht...", "");     // Message not understand
          deleteflag = false;                                                         // Reset delete flag
          resetflag = false;                                                          // Reset reset flag
          alarmmeflag = false;                                                        // Reset AlarmMe flag
        }
      }
      else {
        bot.sendMessage(chat_id, "Access denied. Your ID: " + chat_id, "");           // Only Admin can talk to the bot
        deleteflag = false;                                                           // Reset delete flag
        resetflag = false;                                                            // Reset reset flag
        alarmmeflag = false;                                                        // Reset AlarmMe flag
      }
    }
  } 
// *** Check for WiFi connection will be done every cycle (0.1s) ****************************************************************************************************************
  if ((WiFi.status() != WL_CONNECTED)) {                                              // Check for connection
    cf = 0;                                                                           // Reset WiFi Connection Flag
    bool an = false;
    while (WiFi.status() != WL_CONNECTED) {                                           // As long as not connected
      if(!an) {
        pixels.setPixelColor(0, pixels.Color(20, 0, 20));                             // Not connected: Status pink flashing
        an = true;
      } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));                               // Not connected: Status pink flashing
        an = false;
      }
      pixels.show();                                                                  // Show status color
      delay(500);                                                                     // Every 0.5s
      Serial.print(".");                                                              // Debug printing "."
    }
  }
  if ((WiFi.status() == WL_CONNECTED)) {                                              // If connected:
    if (cf == 0) {                                                                    // If reconnection detected
      bot.sendMessage(andiid, "ESPAlarm reconnection!", "");                          // Send message "reconnection" to Bot  
    }
    cf = 1;                                                                           // Set WiFi Connection Flag
    pixels.setPixelColor(0, pixels.Color(0, 10, 0));                                  // Set status green
    pixels.show();                                                                    // Show status
  }
  
  delay(100);
}
