#include "mbedtls/md.h"
#include <ESPmDNS.h>

#define USING_CORE_ESP32_CORE_V200_PLUS       false
#include <AsyncHTTPSRequest_Generic.h>            // https://github.com/khoih-prog/AsyncHTTPSRequest_Generic
// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include <AsyncHTTPSRequest_Impl_Generic.h>       // https://github.com/khoih-prog/AsyncHTTPSRequest_Generic

#define _ESPASYNC_WIFIMGR_LOGLEVEL_ 0

#include <FS.h>

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#include "FS.h"
#include <LITTLEFS.h>       // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS

#define FileFS        LittleFS
#define FS_Name       "LittleFS"

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define ESP_DRD_USE_LITTLEFS    true
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false

#define DOUBLERESETDETECTOR_DEBUG false

#include <ESP_DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector

#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0

String ssid = CUBE_NAME;
String password;

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

#define FORMAT_FILESYSTEM         false

#define MIN_AP_PASSWORD_SIZE    8

#define SSID_MAX_LEN            32
#define PASS_MAX_LEN            64

typedef struct {
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
} WiFi_Credentials;

typedef struct {
  String wifi_ssid;
  String wifi_pw;
} WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

// Assuming max 49 chars
#define TZNAME_MAX_LEN            50
#define TIMEZONE_MAX_LEN          50

typedef struct {
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
  char TZ_Name[TZNAME_MAX_LEN];     // "America/Toronto"
  char TZ[TIMEZONE_MAX_LEN];        // "EST5EDT,M3.2.0,M11.1.0"
  uint16_t checksum;
} WM_Config;

WM_Config WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")

bool initialConfig;
#define USE_AVAILABLE_PAGES         false
#define USE_STATIC_IP_CONFIG_IN_CP  false
#define USE_ESP_WIFIMANAGER_NTP     false
#define USE_CLOUDFLARE_NTP          false
#define USING_CORS_FEATURE          false
#define USE_DHCP_IP                 true


#define USE_CONFIGURABLE_DNS  false
#define USE_CUSTOM_AP_IP      false
#define ASYNC_TCP_SSL_ENABLED false

#include <ESPAsync_WiFiManager.hpp>               //https://github.com/khoih-prog/ESPAsync_WiFiManager

#define HTTP_PORT     80

#include <ESPAsync_WiFiManager.h>               //https://github.com/khoih-prog/ESPAsync_WiFiManager


FS* filesystem =      &LITTLEFS;

DoubleResetDetector* drd;

#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 800L
#define WIFI_MULTI_CONNECT_WAITING_MS     500L

#include <WebSocketsServer.h>
#include <base64.h>

WebSocketsServer webSocket(81);

String host = CUBE_NAME;

#define HTTP_PORT     80

AsyncWebServer server(HTTP_PORT);
//DNSServer dnsServer;

AsyncEventSource events("/events");

String http_username = "admin";
String http_password = CUBE_PASS;

ESPAsync_WiFiManager* ESPAsync_wifiManager;

uint8_t connectMultiWiFi() {
  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));

  if ((Router_SSID != "") && (Router_Pass != "")) {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass );
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE)) {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }

  LOGERROR(F("Connecting MultiWifi..."));

  int i = 0;
  
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ((i++ < 20) && (status != WL_CONNECTED)) {
    status = WiFi.status();

    if (status == WL_CONNECTED) {
      break;
    }
    
    delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if (status == WL_CONNECTED) {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  } else {
    LOGERROR(F("WiFi not connected"));

    // To avoid unnecessary DRD
    drd->loop();  
    ESP.restart();
  }

  return status;
}

void check_WiFi() {
  if ((WiFi.status() != WL_CONNECTED)) {
    #ifdef DEBUG
    Serial.println(F("\nWiFi lost. Call connectMultiWiFi in loop"));
    #endif
    connectMultiWiFi();
  }
}

#define WIFICHECK_INTERVAL    1000L

void check_status() {
  static ulong checkstatus_timeout  = 0;
  static ulong checkwifi_timeout    = 0;

  static ulong current_millis;

  current_millis = millis();

  if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0)) {
    check_WiFi();
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }
}

int calcChecksum(uint8_t* address, uint16_t sizeToCalc) {
  uint16_t checkSum = 0;
  
  for (uint16_t index = 0; index < sizeToCalc; index++) {
    checkSum += * ( ( (byte*) address ) + index);
  }

  return checkSum;
}

bool loadConfigData() {
  File file = FileFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  memset((void *) &WM_config,       0, sizeof(WM_config));
  if (file) {
    file.readBytes((char *) &WM_config,   sizeof(WM_config));

    file.close();
    LOGERROR(F("OK"));

    if (WM_config.checksum != calcChecksum((uint8_t*) &WM_config, sizeof(WM_config) - sizeof(WM_config.checksum))) {
      LOGERROR(F("WM_config checksum wrong"));
      return false;
    }

    return true;
  } else {
    LOGERROR(F("failed"));
    return false;
  }
}

void saveConfigData() {
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file) {
    WM_config.checksum = calcChecksum( (uint8_t*) &WM_config, sizeof(WM_config) - sizeof(WM_config.checksum) );
    
    file.write((uint8_t*) &WM_config, sizeof(WM_config));

    file.close();
    LOGERROR(F("OK"));
  } else {
    LOGERROR(F("failed"));
  }
}

void resetWifi() {
  FileFS.remove("/wifi_cred.dat");
  ESPAsync_wifiManager->resetSettings();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      #ifdef DEBUG
      Serial.printf("[%u] Disconnected!\n", num);
      #endif
      isConfiguring = false;
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
      IPAddress ip = webSocket.remoteIP(num);
      #ifdef DEBUG
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      #endif
      isConfiguring = true;
      if (hasCube) {
        webSocket.broadcastTXT(currentRfid);
      }        
    }
    case WStype_TEXT: {
      webSocket.broadcastTXT("pong");
    }
    break;
  }
}

String randString(int length) { 
  String res = ""; 
  String anchars = "abcdefghijklmnopqrstuvwxyz0123456789"; 
  for(int i = 0; i < length; i++) {
    res += anchars[random(0, anchars.length())]; 
  }

  return res;
}

String wsToken = randString(32);

void initWebSocket() { // Start a WebSocket server
  String ws_auth_code = wsToken + String(":");
  String auth = base64::encode(ws_auth_code);
  #ifdef DEBUG
  Serial.println("WS auth token: " + auth);
  #endif
  webSocket.setAuthorization(auth.c_str());
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  #ifdef DEBUG
  Serial.println("WebSocket server started.");
  #endif
}

void persistTime() {
  unsigned long timestamp = getEpochTime(); 
  
  #ifdef DEBUG
  Serial.print("Trying to store time: ");
  Serial.println(getISO8601Time(timestamp));
  #endif

  if (!hasCube) {
    #ifdef DEBUG
    Serial.println("No cube found, skipping persistTime");
    #endif
    return;
  }
  
  #ifdef DEBUG
  Serial.println("Storing time");
  #endif
  File file = FileFS.open("/time.bin", "w");
  if (!file) {
    #ifdef DEBUG
    Serial.println("File open failed");
    #endif
    return;
  }
  
  file.write((byte *)&timestamp, sizeof(unsigned long));
  file.close();
  #ifdef DEBUG
  Serial.println("Time stored");
  #endif
}

bool getPersistedTime() {
  #ifdef DEBUG
  Serial.println("getPersistedTime");
  #endif
  File file = FileFS.open("/time.bin", "r");
  if (!file) {
    #ifdef DEBUG
    Serial.println("file open failed");
    #endif
    return false;
  }
  
  file.read((byte *)&lastPersistedTime, sizeof(unsigned long));
  file.close();
}

bool persistTimers() {
  #ifdef DEBUG
  Serial.println("persistTimers");
  #endif
  File file = FileFS.open("/timers.bin", "w");
  if (!file) {
    #ifdef DEBUG
    Serial.println("file open failed");
    #endif
    return false;
  }

  file.write((byte *)&clockifyTimers, sizeof(ClockifyTimer) * MAX_CLOCKIFY_TIMERS);
  file.close();

  hasUnpersistedTimers = false;
  return true;
}

void print_reset_reason(int reason)
{
  switch ( reason)
  {
    case 1 : Serial.println ("POWERON_RESET");break;          /**<1,  Vbat power on reset*/
    case 3 : Serial.println ("SW_RESET");break;               /**<3,  Software reset digital core*/
    case 4 : Serial.println ("OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : Serial.println ("DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : Serial.println ("SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : Serial.println ("TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.println ("TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.println ("RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : Serial.println ("INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : Serial.println ("TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : Serial.println ("SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : Serial.println ("RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : Serial.println ("EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : Serial.println ("RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : Serial.println ("RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : Serial.println ("NO_MEAN");
  }
}

void verbose_print_reset_reason(int reason)
{
  switch ( reason)
  {
    case 1  : Serial.println ("Vbat power on reset");break;
    case 3  : Serial.println ("Software reset digital core");break;
    case 4  : Serial.println ("Legacy watch dog reset digital core");break;
    case 5  : Serial.println ("Deep Sleep reset digital core");break;
    case 6  : Serial.println ("Reset by SLC module, reset digital core");break;
    case 7  : Serial.println ("Timer Group0 Watch dog reset digital core");break;
    case 8  : Serial.println ("Timer Group1 Watch dog reset digital core");break;
    case 9  : Serial.println ("RTC Watch dog Reset digital core");break;
    case 10 : Serial.println ("Instrusion tested to reset CPU");break;
    case 11 : Serial.println ("Time Group reset CPU");break;
    case 12 : Serial.println ("Software reset CPU");break;
    case 13 : Serial.println ("RTC Watch dog Reset CPU");break;
    case 14 : Serial.println ("for APP CPU, reseted by PRO CPU");break;
    case 15 : Serial.println ("Reset when the vdd voltage is not stable");break;
    case 16 : Serial.println ("RTC Watch dog reset digital core and rtc module");break;
    default : Serial.println ("NO_MEAN");
  }
}

bool loadTimers() {
  #ifdef DEBUG
  Serial.println("loadTimers");
  #endif 

  initTimers();

  #ifdef DEBUG
  Serial.println("CPU0 reset reason:");
  print_reset_reason(rtc_get_reset_reason(0));
  verbose_print_reset_reason(rtc_get_reset_reason(0));

  Serial.println("CPU1 reset reason:");
  print_reset_reason(rtc_get_reset_reason(1));
  verbose_print_reset_reason(rtc_get_reset_reason(1));
  #endif
  
  if (rtc_get_reset_reason(0) != POWERON_RESET) {
    int timersFromRTC = 0;
    for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {    
      if (clockifyTimers[i].state != TIMER_STOPPED) {
        timersFromRTC++;
      }
    }
    
    if (timersFromRTC > 0) {
      #ifdef DEBUG
      Serial.println("skipping file load, as the board was reset, not powered on");
      Serial.print("Loaded "); Serial.print(timersFromRTC); Serial.println(" timers from RTC");
      #endif 
        
      hasUnpersistedTimers = true;
  
      #ifdef DEBUG
      for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {    
        Serial.print("--- Stored timer ");
        Serial.print(i);
        Serial.println(" ---");
        printTimer(i);
      }
      #endif 
      return true;
    }
  }
  
  File file = FileFS.open("/timers.bin", "r");
  if (!file) {
    #ifdef DEBUG
    Serial.println("file open failed");
    #endif
    return false;
  }

  file.read((byte *)&clockifyTimers, sizeof(ClockifyTimer) * MAX_CLOCKIFY_TIMERS);
  file.close();

  #ifdef DEBUG
  for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {    
    Serial.print("--- Stored timer ");
    Serial.print(i);
    Serial.println(" ---");
    printTimer(i);
  }
  #endif 
  
  return true;
}

String wsAuth;

bool storeJson(const char* json) {
  #ifdef DEBUG
  Serial.println("storeJson");
  #endif
  File file = FileFS.open("/data.json", "w");
  if (!file) {
    #ifdef DEBUG
    Serial.println("file open failed");
    #endif
    return false;
  }

  file.print(json);
  file.close();
  return true;
}

void resetAll() {
  FileFS.remove("/timers.bin");
  File file = FileFS.open("/data.json", "w");
  file.print("{\"apiKeys\":{\"clockify\":[],\"jira\":\"\"},\"sides\":{}}");
  delay(1);
  file.close();
  
  resetWifi();
}

String sha256(String input) {
  char const *payload = input.c_str();
  byte shaResult[32];
  
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  const size_t payloadLength = strlen(payload);
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) payload, payloadLength);
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  
  String res = "";
  for(int i= 0; i< sizeof(shaResult); i++) {
    char str[3];
    sprintf(str, "%02x", (int)shaResult[i]);
    res += String(str);
  }

  return res;
}

bool is_authenticated(AsyncWebServerRequest *request) {
  if (request->hasHeader("Cookie")) {
    String cookie = request->header("Cookie");
    String token = sha256(String(CUBE_PASS) + ":" + request->client()->remoteIP().toString());

    if (cookie.indexOf("ESPSESSIONID=" + token) != -1) {
      return true;
    }
  }
  return false;
}

bool handleFileRead(AsyncWebServerRequest *request, String path) {
  if (!is_authenticated(request)) {
    AsyncWebServerResponse *response = request->beginResponse(301);
    response->addHeader("Location", "/login");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    return true;
  } else {
    if (path == "/") { 
      path = "/index.html";
    }

    if (path == "/data") { 
      path = "/data.json";
    }
  }
  
  if(FileFS.exists(path)) {
    AsyncWebServerResponse *response = request->beginResponse(FileFS, path);
    request->send(response);

    return true;
  }
  return false;
}

void initConfig() {
  if (FORMAT_FILESYSTEM) {
    LITTLEFS.format();
  }

  if (!LITTLEFS.begin(true)) {
    #ifdef DEBUG
    Serial.println(F("SPIFFS/LittleFS failed! Already tried formatting."));
    #endif
    
    if (!LITTLEFS.begin()) {     
      // prevents debug info from the library to hide err message.
      delay(100);
      #ifdef DEBUG
      Serial.println(F("LittleFS failed!. Please use SPIFFS or EEPROM. Stay forever"));
      #endif

      while (true) {
        delay(1);
      }
    }
  }
  
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

  unsigned long startedAt = millis();
  AsyncWebServer webServer(HTTP_PORT);
  DNSServer dnsServer;
  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, CUBE_NAME);
  ESPAsync_wifiManager.setMinimumSignalQuality(-1);
  ESPAsync_wifiManager.setConfigPortalChannel(0);
  Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
  Router_Pass = ESPAsync_wifiManager.WiFi_Pass();
  #ifdef DEBUG
  Serial.println("ESP Self-Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);
  #endif
  ssid.toUpperCase();
  password = CUBE_PASS;

  bool configDataLoaded = false;

  if ((Router_SSID != "") && (Router_Pass != "")) {
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

    ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println(F("Got ESP Self-Stored Credentials. Timeout 120s for Config Portal"));
  }
  if (loadConfigData()) {
    configDataLoaded = true;

    ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println(F("Got stored Credentials. Timeout 120s for Config Portal"));
  } else {
    // Enter CP only if no stored SSID on flash and file
    Serial.println(F("Open Config Portal without Timeout: No stored Credentials."));
    initialConfig = true;
  }
  if (drd->detectDoubleReset()) {
    // DRD, disable timeout.
    ESPAsync_wifiManager.setConfigPortalTimeout(0);
    
    Serial.println(F("Open Config Portal without Timeout: Double Reset Detected"));
    initialConfig = true;
  }

  if (initialConfig) {
    wifiReboot = true;
    booting = false;
    delay(1000);
    cancelProgress("SSID: " + String(ssid) + "\nPASS: " + String(password));
    
    #ifdef DEBUG
    Serial.print(F("Starting configuration portal @ "));
    
    Serial.print(F("192.168.4.1"));
    Serial.print(F(", SSID = "));
    Serial.print(ssid);
    Serial.print(F(", PWD = "));
    Serial.println(password);
    #endif

    if (!ESPAsync_wifiManager.startConfigPortal((const char *) ssid.c_str(), password.c_str())) {
      #ifdef DEBUG
      Serial.println(F("Not connected to WiFi, yet..."));
      #endif
      initialConfig = false;
    } else {
      #ifdef DEBUG
      Serial.println(F("WiFi connected...yeey :)"));
      #endif
    }

    memset(&WM_config, 0, sizeof(WM_config));

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
      String tempSSID = ESPAsync_wifiManager.getSSID(i);
      String tempPW   = ESPAsync_wifiManager.getPW(i);

      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1) {
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      } else {
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);
      }
      
      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1) {
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      } else {
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);
      }
      
      if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE)) {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    saveConfigData();
  }
  
  startedAt = millis();

  if (!initialConfig) {
    if (!configDataLoaded) {
      loadConfigData();
    }

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
      if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE)) {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if (WiFi.status() != WL_CONNECTED) {
      #ifdef DEBUG
      Serial.println(F("ConnectMultiWiFi in setup"));
      #endif
      connectMultiWiFi();
    }
  }

  #ifdef DEBUG
  Serial.print(F("After waiting "));
  Serial.print((float) (millis() - startedAt) / 1000);
  Serial.print(F(" secs more in setup(), connection result is "));
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("connected. Local IP: "));
    Serial.println(WiFi.localIP());   
  } else {
    Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
  }
  #endif
    
  if (!MDNS.begin(CUBE_NAME)) {
    #ifdef DEBUG      
    Serial.println(F("Error starting MDNS responder!"));
    #endif
  }
  
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", HTTP_PORT);

  //SERVER INIT
  events.onConnect([](AsyncEventSourceClient * client) {
    client->send("hello!", NULL, millis(), 1000);
  });
  
  server.addHandler(&events);

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate("admin", CUBE_PASS.c_str())) {
      return request->requestAuthentication();
    }
    
    AsyncWebServerResponse *response = request->beginResponse(301);
    response->addHeader("Location", "/");
    response->addHeader("Cache-Control", "no-cache");

    String token = sha256(String(CUBE_PASS) + ":" + request->client()->remoteIP().toString());
    response->addHeader("Set-Cookie", "ESPSESSIONID=" + token);
    
    request->send(response);
  });

  server.on("/wifi", HTTP_OPTIONS, [](AsyncWebServerRequest * request) {
    if (!is_authenticated(request)) {
      return request->send(403);
    }
    
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    request->send(response);    
  });
  
  server.on("/wifi", HTTP_DELETE, [](AsyncWebServerRequest * request) {
    if (!is_authenticated(request)) {
      return request->send(403);
    }   
    
    resetWifi();
    request->send(200);
  });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (!is_authenticated(request)) {
      return request->send(403);
    }   
    
    resetAll();
    request->send(200);
  });
  
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!is_authenticated(request)) {
      return request->send(403);
    }

    const char* operation = index == 0 ? FILE_WRITE : FILE_APPEND;
    File file = FileFS.open("/data.json", operation);
    if(len) {
      file.write(data,len);
    }

    #ifdef DEBUG
    Serial.println("Receiving data: ");
    for(size_t i=0; i<len; i++){
      Serial.write(data[i]);
    }
    #endif

    file.close();
    if(index + len == total){
      file = FileFS.open("/data.json", "r");
      String json = file.readString();
      file.close();
      #ifdef DEBUG
      Serial.println("JSON: ");
      Serial.println(json);
      #endif
      sides = jsonExtract(json, "sides");

      #ifdef DEBUG
      Serial.println("Sides: ");
      Serial.println(sides);
      #endif
      request->send(200, "text/json", "{\"success\":true}");
    }
  });

  server.on("/token", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!is_authenticated(request)) {
      return request->send(403);
    }
    
    request->send(200, "application/json", "{\"token\":\"" + wsToken + "\"}"); 
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (!handleFileRead(request, request->url())) {
      request->send(404);
    }
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.begin();
  #ifdef DEBUG
  Serial.print(F("HTTP server started @ "));
  Serial.println(WiFi.localIP());
  #endif
  
   // Set time via NTP, as required for x.509 validation
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  #ifdef DEBUG
  Serial.print("Waiting for NTP time sync: ");
  #endif
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
    now = time(nullptr);
  }
  
  #ifdef DEBUG
  Serial.println("");
  #endif
  
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  
  #ifdef DEBUG
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
  #endif

  File jsonFile = FileFS.open("/data.json", "r");
  if (jsonFile) {
    String jsonString = jsonFile.readString();
    jsonFile.close();
    #ifdef DEBUG
    Serial.println("Loading json from /data.json");
    Serial.println(jsonString);
    #endif
    sides = jsonExtract(jsonString, "sides");
    #ifdef DEBUG
    Serial.println("Sides: ");
    Serial.println(sides);
    #endif
  } else {
    #ifdef DEBUG
    Serial.println("Error opening /data.json, no data loaded");
    #endif
  }
  
  if (loadTimers()) {
    preInitTimers = true;
  }

   initWebSocket();
}

void handleConfig() {
  drd->loop();
  check_status();
  webSocket.loop();
}
