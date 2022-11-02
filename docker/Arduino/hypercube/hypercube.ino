#ifndef CUBE_NAME
#define CUBE_NAME "hypercube9"
#endif

String CUBE_PASS = "";
#define MAX_CLOCKIFY_TIMERS 10
#define SCREEN_SAVER_TIMEOUT_SECONDS 3600
#define DEBUG

#ifdef DEBUG
#define ASYNC_HTTP_DEBUG_PORT           Serial
// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _ASYNC_HTTP_LOGLEVEL_           4
#else
#define _ASYNC_HTTP_LOGLEVEL_           0
#endif

#include "rom/rtc.h"
#include <MFRC522.h>
#include <Ticker.h>

bool Ticker::active() {
  return (bool)_timer;
}
#include <jsonlib.h>
#include "clockify_timer.h"

String sides = "";
String clockifyApiKeys = "";

int prevHeapSpace = 0;

MFRC522 mfrc522(5, 26);

bool hasCube = false;
bool cubeRemoved = false;
bool notConfigued = false;
bool hasRfid = false;
bool isConfiguring = false;
bool timerRunning = false;
bool preInitTimers = false;
bool reconnecting = false;
bool booting = true;
bool wifiReboot = false;
uint32_t idleFromTime = 0;
unsigned long lastPersistedTime = 0;

unsigned int start = 0;
int64_t startOffset;

char currentRfid[17] = "";

void updateTimerRunning() {
  bool running = false;
  for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {
    if (clockifyTimers[i].state == TIMER_RUNNING) {
      running = true;
    }

    if (clockifyTimers[i].state == TIMER_STOP_SENT) {
      running = true;
    }
  }

  #ifdef DEBUG
  Serial.println("Timer running set to: " + running ? "true" : "false");
  #endif
  
  timerRunning = running;
}

#include "ntp.h"
#include "display.h"
#include "config.h"
#include "http_client.h"

int rfidFailCount = 0;
void checkRfidReset() {
  rfidFailCount++;
  if (rfidFailCount > 10) {
    #ifdef DEBUG
    Serial.println("Reseting RFID reader");
    #endif
    mfrc522.PCD_Init(5, 26);
    rfidFailCount = 0;
  }
}

void getTagUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     uid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
     uid.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  
  uid.toUpperCase();
  uid.toCharArray(currentRfid, 16);
}

void readRfid() {
  if (hasRfid) {
    return;
  }

  #ifdef DEBUG
  Serial.println("Trying to read RFID");
  #endif
  
  if (!mfrc522.PICC_IsNewCardPresent()) {
    #ifdef DEBUG
    Serial.println("Could not read RFID");
    #endif
    checkRfidReset();
    hasRfid = false;
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    #ifdef DEBUG
    Serial.println("Could not read serial");
    #endif
    checkRfidReset();
    hasRfid = false;
    return;
  }

  rfidFailCount = 0;
  hasRfid = true;
  getTagUID();
  
  #ifdef DEBUG
  Serial.println(currentRfid);
  #endif
}

void stopTimers(bool force = false) {
  if (!timerRunning && !isPending() && !force) {
    return;
  }

  #ifdef DEBUG
  Serial.println("Stopping timers");
  #endif
  
  timerRunning = false;
  unsigned long timestamp = force ? lastPersistedTime : 0;
  String stopTime = getISO8601Time(timestamp);

  bool changes = false;
  bool needsStop = false;
  for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {
    TimerState desiredState = clockifyTimers[i].desiredState;
    if (!force && desiredState != TIMER_RUNNING && desiredState != TIMER_STARTING) {
      continue;
    }

    if (clockifyTimers[i].state == TIMER_STOPPED) {
      continue;
    }

    #ifdef DEBUG
    Serial.println("Scheduling timer to be stopped");
    #endif
    stopTime.toCharArray(clockifyTimers[i].endTime, 25);
    // If the cube was removed before the reconiliation loop had a chance to send 
    // the request to Clockify, we just set the state to stopped, as it was never started.
    if (clockifyTimers[i].state == TIMER_CHECKING || clockifyTimers[i].state == TIMER_PENDING) {
      clockifyTimers[i].state = TIMER_STOPPED;
    }

    if (strlen(clockifyTimers[i].id) == 0) {
      clockifyTimers[i].state = TIMER_STOPPED;
    }

    if (clockifyTimers[i].state == TIMER_STOPPING && secondsSince(clockifyTimers[i].endTime) > 60) {
      #ifdef DEBUG
      Serial.print("Timer " + String(i) + " has state TIMER_STOPPING, but it has been more than ");
      Serial.println("60 seconds since the cube was removed. Setting state to be TIMER_STOP_SENT to retry.");
      #endif
      clockifyTimers[i].state = TIMER_STOP_SENT;
    }
    
    clockifyTimers[i].desiredState = TIMER_STOPPED;
    printTimer(i);
    changes = true;
    if (clockifyTimers[i].state != TIMER_STOPPED) {
      needsStop = true;
    }
  }

  if (changes) {
    saveTimers();
  }

  if (needsStop) {
    displayReverseProgress(3000);
  } else {
    cancelProgress();
  }
}

void stopRunningTimers() {
  bool force = true;
  stopTimers(force);
  preInitTimers = false;
}

void setCubeRemoved() {
  hasCube = false;
  hasRfid = false;
  notConfigued = false;
  if (!cubeRemoved) {
    #ifdef DEBUG
    Serial.println("Cube removed");
    #endif
    cubeRemoved = true;
    idleFromTime = millis();
    //mfrc522.PCD_Init(5, 26);
    stopTimers();
    if (isConfiguring) {
      #ifdef DEBUG
      Serial.println("WS clearing side");
      #endif
      webSocket.broadcastTXT("");
    }
  }
}

void showNotConfigured() {
  notConfigued = true;
  displayText("Conf: http://" + String(CUBE_NAME) + " or " + WiFi.localIP().toString());
}

void startTimers() {
  if (!hasCube || !hasRfid || timerRunning || notConfigued) {
    return;
  }

  if (isPending()) {
    return;
  }

  #ifdef DEBUG
  Serial.println("Starting timers");
  #endif

  #ifdef DEBUG
  Serial.println("Sides:");
  Serial.println(sides);
  Serial.print("Current rfid: ");
  Serial.println(currentRfid);
  #endif
  
  String side = jsonExtract(sides, currentRfid);
  if (side == "") {
    #ifdef DEBUG
    Serial.println("Could not find side");
    #endif
    stopTimers();
    showNotConfigured();
    return;
  }
  
  String clockifyConfigs = jsonExtract(side, "clockify");
  #ifdef DEBUG
  Serial.print("ClockifyConfigs: ");
  Serial.println(clockifyConfigs);
  #endif
  if (clockifyConfigs == "" || clockifyConfigs == "[]") {
    #ifdef DEBUG
    Serial.println("Could not find clockify config for side");
    #endif
    stopTimers();
    showNotConfigured();
    return;
  }

  #ifdef DEBUG
  Serial.println("Starting timer");
  #endif

  String startTime = getISO8601Time();
  start = timeClient.getEpochTime();
  startOffset = esp_timer_get_time();
  int j = 0;
  bool changes = false;

  String lastConfig = "";
  for (int i = 0; i < 10; i++) {
    String config = jsonIndexList(clockifyConfigs, i);
    if (config == lastConfig) {
      break;
    }

    #ifdef DEBUG
    Serial.print("Timer config ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(config);
    Serial.println("Checking for resumeable timers...");
    #endif

    String apiKey = jsonExtract(config, "apiKey");
    lastConfig = config;
    int emptySlot = 0;
    bool resumed = false;
    bool hasRunningTimer = false;
    for(; j < MAX_CLOCKIFY_TIMERS; j++) {
      TimerState state = clockifyTimers[j].state;
      TimerState desiredState = clockifyTimers[j].desiredState;
      #ifdef DEBUG
      Serial.print("Stored timer config ");
      Serial.print(j);
      Serial.print(" - (id: ");
      Serial.print(clockifyTimers[j].id);
      Serial.print(", state: ");
      Serial.print(stateToString(state));
      Serial.println(")");
      #endif

      if (state != TIMER_STOPPED) {
        if (!preInitTimers) {
          continue;  
        }

        if ((state == TIMER_PENDING || state == TIMER_CHECKING) && strcmp(clockifyTimers[j].apiKey, apiKey.c_str()) == 0) {
          #ifdef DEBUG
          Serial.println("Found existing timer with same API key in slot " + String(j) + ": ");
          printTimer(j);
          #endif
          clockifyTimers[j].state = TIMER_STOPPED;
          clockifyTimers[j].desiredState = TIMER_STOPPED;
          continue;
        }

        if (desiredState == TIMER_STOPPED) {
          clockifyTimers[j].state = strlen(clockifyTimers[j].id) == 0 ? TIMER_STOPPED : TIMER_STOP_SENT;
          continue;
        }

        if (state == TIMER_STOPPING || state == TIMER_STOP_SENT) {
          clockifyTimers[j].state = TIMER_STOP_SENT;
          clockifyTimers[j].desiredState = TIMER_STOPPED;
          continue;
        }

        if (strcmp(clockifyTimers[j].sideId, currentRfid) != 0) {
          hasRunningTimer = true;
          continue;
        }

        start = clockifyTimers[j].start;
        #ifdef DEBUG
        Serial.print("Resuming stored timer: ");
        Serial.println(j);
        printTimer(j);
        #endif
        resumed = true;
        break;
      }
      
      emptySlot = j;
    }

    if (resumed) {
      continue;
    }

    if (hasRunningTimer) {
      #ifdef DEBUG
      Serial.println("Running timer found, force stopping...");
      #endif
      bool forceStop = true;
      stopTimers(forceStop);
    }
    
    j = emptySlot;
    #ifdef DEBUG
    Serial.print("Storing timer at slot: ");
    Serial.println(j);
    #endif

    clockifyTimers[j].start = start;
    strcpy(clockifyTimers[j].id, "");
    strcpy(clockifyTimers[j].sideId, currentRfid);
    startTime.toCharArray(clockifyTimers[j].startTime, 25);
    strcpy(clockifyTimers[j].endTime, "");
    String workspaceId = jsonExtract(config, "workspaceId");
    String projectId = jsonExtract(config, "projectId");
    String taskId = jsonExtract(config, "taskId");
    String description = jsonExtract(config, "taskDesc");
    #ifdef DEBUG
    Serial.print("JSON apiKey: "); Serial.println(apiKey);
    Serial.print("JSON workspaceId: "); Serial.println(workspaceId);
    Serial.print("JSON projectId: "); Serial.println(projectId);
    Serial.print("JSON taskId: "); Serial.println(taskId);
    Serial.print("JSON description: "); Serial.println(description);
    #endif
    strcpy(clockifyTimers[j].apiKey, apiKey.c_str());
    strcpy(clockifyTimers[j].workspaceId, workspaceId.c_str());
    strcpy(clockifyTimers[j].projectId, projectId.c_str());
    strcpy(clockifyTimers[j].taskId, taskId.c_str());
    strcpy(clockifyTimers[j].description, description.c_str());
    clockifyTimers[j].state = TIMER_CHECKING;
    clockifyTimers[j].desiredState = TIMER_STARTING;
    printTimer(j);
    
    changes = true;
  }
  
  if (changes) {
    saveTimers();
    printTimers();
    displayProgress(3000, "Initializing timer");
  }
  
  timerRunning = true;
}

int apiFailures = 0;
void reconciliateTimers(void * parameter) {
  while (true) {
    delay(100);
    
    #ifdef DEBUG
    int heapSpace = ESP.getFreeHeap();
    if (abs(prevHeapSpace - heapSpace) > 2000) {
      prevHeapSpace = heapSpace;
      Serial.print("Free heap space: "); Serial.println(heapSpace);  
    }
    #endif
  
    if (hasUnpersistedTimers) {
      persistTimers();
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      #ifdef DEBUG
      Serial.println("Reconciliation: WiFi not online, skipping");
      #endif
      continue;
    }
  
    reconnecting = false;
  
    bool hadChanges = false;
    for(int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {
      if (clockifyTimers[i].desiredState == TIMER_STARTING && clockifyTimers[i].state == TIMER_CHECKING) {
        if (!checkTimer(i)) {
          apiFailures++;
          #ifdef DEBUG
          Serial.print("Reconciliation: Check timer failed. Fail count: "); Serial.println(apiFailures);
          #endif
        } else {
          apiFailures = 0;
          break;
        }
      }
      
      if (clockifyTimers[i].desiredState == TIMER_STARTING && clockifyTimers[i].state == TIMER_PENDING) {
        int elapsed = timeClient.getEpochTime() - clockifyTimers[i].start;
        if (elapsed >= 3) {
          clockifyTimers[i].desiredState = TIMER_RUNNING;
          clockifyTimers[i].state = TIMER_STARTING;
        }
      }
      
      if (clockifyTimers[i].desiredState == TIMER_RUNNING && clockifyTimers[i].state == TIMER_STARTING) {
        displayProgress(2000, "Starting timer");
        if (!startTimer(i)) {
          apiFailures++;
          #ifdef DEBUG
          Serial.print("Reconciliation: Start timer failed. Fail count: "); Serial.println(apiFailures);
          #endif
        } else {
          apiFailures = 0;
          break;
        }
      }
  
      if (clockifyTimers[i].desiredState == TIMER_STOPPED && clockifyTimers[i].state == TIMER_RUNNING) {
        if (!stopTimer(i)) {
          apiFailures++;
          #ifdef DEBUG
          Serial.print("Reconciliation: Stop timer failed. Fail count: "); Serial.println(apiFailures);
          #endif
        } else {
          apiFailures = 0;
          break;
        }
      }
  
      if (clockifyTimers[i].desiredState == TIMER_STOPPED && clockifyTimers[i].state == TIMER_STOP_SENT) {
        if (!stopTimer(i)) {
          apiFailures++;
          #ifdef DEBUG
          Serial.print("Reconciliation: Stop timer failed. Fail count: "); Serial.println(apiFailures);
          #endif
        } else {
          apiFailures = 0;
          break;
        }
      }
    }
  
    if (apiFailures > 10) {
      saveTimers();
      apiFailures = 0;
      #ifdef DEBUG
      Serial.println("Reconciliation: Too many API failures, disconnecting WiFi");
      #endif
      reconnecting = true;
      preInitTimers = true;
      showElapsed();
      WiFi.disconnect();
    }
  }
}

void getElapsedTime(char* out) {
  int diff = timeClient.getEpochTime() - start;
  int hours = diff / 3600;
  diff -= hours * 3600;
  int minutes = diff / 60;
  sprintf_P(out, PSTR("%s%02d:%02d%s"), reconnecting ? "! " : "", hours, minutes, reconnecting ? " !" : "");
}

bool isPending() {
  for(int j = 0; j < MAX_CLOCKIFY_TIMERS; j++) {
    if (clockifyTimers[j].state == TIMER_CHECKING) {
      return true;
    }

    if (clockifyTimers[j].state == TIMER_PENDING) {
      return true;
    }

    if (clockifyTimers[j].desiredState == TIMER_RUNNING && clockifyTimers[j].state != TIMER_RUNNING) {
      return true;
    }
  }

  return false;
}

void showElapsed() {
  if (!timerRunning) {
    return;
  }

  if (isPending()) {
    return;
  }
  
  char elapsedTime[10]; 
  getElapsedTime(elapsedTime);
  displayText(elapsedTime, FONT_SIZE_36);
}

void handleLogic() {
  if (isCubePlaced()) {
    readRfid();
    if (hasRfid) {
      if (!hasCube) {
        hasCube = true;
        cubeRemoved = false;
        idleFromTime = 0;
        #ifdef DEBUG
        Serial.println(currentRfid);
        #endif
        if (isConfiguring) {
          #ifdef DEBUG
          Serial.print("WS setting side: ");
          Serial.println(currentRfid);
          #endif
          webSocket.broadcastTXT(currentRfid);
        }
      } else if (notConfigued && timerRunning) {
        stopTimers();
      }

      if (isConfiguring) {
        displayText("Configuring");
      } else {
        if (!timerRunning) {
          startTimers();
        } else {
           showElapsed();
        }
      }
      
      preInitTimers = false;
    } else {
      displayText("Place cube to begin");
      hasCube = false;
    }
  } else {
    setCubeRemoved();
    hasRfid = false;
    displayText("Place cube to begin");
    if (preInitTimers) {
      stopRunningTimers();
    } else {
      stopTimers();
    }
  }
}

#define PIN_INPUT_REED 33

bool isCubePlaced() {
  return digitalRead(PIN_INPUT_REED) == LOW;
}

void setPassword() {
  char ssid[23];
  uint64_t chipid = ESP.getEfuseMac();
  uint16_t chip = (uint16_t)(chipid >> 32);
  snprintf(ssid, 23, "MCUDEVICE-%04X%08X", chip, (uint32_t)chipid);
  CUBE_PASS = sha256(String(ssid) + String(CUBE_NAME)).substring(0, 8); 
}

Ticker persistTimer;

void setup() {
  setPassword();

  pinMode(PIN_INPUT_REED, INPUT_PULLUP);
  
  Serial.begin(115200);  
  while (!Serial);

  initDisplayQueue();
  displayProgress(10000, "Booting...");
 
  #ifdef DEBUG
  Serial.setDebugOutput(true);
  #else
  Serial.setDebugOutput(false);
  #endif
  
  SPI.begin();
  mfrc522.PCD_Init();
  initConfig();
  initNtp();
  if (errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY == xTaskCreatePinnedToCore(reconciliateTimers, "reconciliation", 50000, NULL, 0, NULL, 1)) {
    Serial.println("Could not create reconciliation thread, sleeping forever...");
    while(true) {
      delay(10000);
    }
  }

  #ifdef DEBUG
  Serial.print("CUBE_NAME: "); Serial.println(CUBE_NAME);
  Serial.print("CUBE_PASS: "); Serial.println(CUBE_PASS);
  #endif

  getPersistedTime();
  if (lastPersistedTime > 0) {
    #ifdef DEBUG
    Serial.print("Loaded lastPersistedTime: ");
    Serial.println(getISO8601Time(lastPersistedTime));
    #endif
  }

  // Persist the time every minute
  persistTimer.attach_ms(60 * 1000, persistTime);
}

void loop() {
  handleConfig();
  handleNTP();
  if (isNTPReady()) {
    if (booting) {
      cancelProgress();
      idleFromTime = millis();
      persistTime();
    }
    if (wifiReboot) {
      #ifdef DEBUG
      Serial.println("Rebooting after WiFi config to ensure mDNS hostname");
      #endif
      ESP.restart();
    }
    booting = false;
    handleLogic();
  }
}
