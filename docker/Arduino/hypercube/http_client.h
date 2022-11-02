#include <CStringBuilder.h>
#include <WiFiClientSecure.h>

const char* clockify_host = "api.clockify.me";
int clockify_port = 443;
String userIds[10] = {"", "", "", "", "", "", "", "", "", ""};

int getAPIKeyIndex(int i) {
  #ifdef DEBUG
  Serial.println("Trying to get API key index...");
  Serial.print("Current API key: ");
  Serial.println(clockifyTimers[i].apiKey);
  #endif
  String apiKey = "";
  for (int j = 0; j < 10; j++) {
    apiKey = jsonIndexList(clockifyApiKeys, j);
    apiKey = apiKey.substring(1, apiKey.length() - 1);
    #ifdef DEBUG
    Serial.print("API key number ");
    Serial.print(j);
    Serial.print(": ");
    Serial.println(apiKey);
    #endif
    if (strcmp(apiKey.c_str(), clockifyTimers[i].apiKey) == 0) {
      return j;
    }
  }

  return -1;
}

bool fetchUserId(int apiKeyIndex, int i) {
  #ifdef DEBUG
  Serial.println("Trying to fetch userId...");
  #endif
  if (apiKeyIndex < 0) {
    #ifdef DEBUG
    Serial.println("API key not found in list. Failed to fetch userId");
    #endif
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(clockify_host, clockify_port)) {
    #ifdef DEBUG
    Serial.println("Connection failed");
    #endif
    return false;
  }

  client.println("GET /api/v1/user HTTP/1.1");
  client.print("Host: ");
  client.println(clockify_host);
  client.print("X-Api-Key: ");
  client.println(clockifyTimers[i].apiKey);
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.println();

  yield();
  String line = client.readStringUntil('\n');
  String returnCode = line.substring(9, 12);
  #ifdef DEBUG
  Serial.println("Return code: " + returnCode);
  #endif

  yield();
  while (client.connected()) {
    line = client.readStringUntil('\n');
    if (line == "\r") {
      #ifdef DEBUG
      Serial.println("Headers received");
      #endif
      break;
    }
  }

  yield();
  if (returnCode != "200") {
    #ifdef DEBUG
    Serial.println("Failed to fetch userId:");
    Serial.println(line);
    while(client.available()) {
      Serial.println(client.readString());
    }
    #endif
    return false;
  }

  yield();
  line = client.readStringUntil(',');
  String userId = line.substring(7, 31);
  #ifdef DEBUG
  Serial.println("UserId found: " + userId);
  #endif
  userIds[apiKeyIndex] = userId;
  client.stop();
  return true;
}

String getUserId(int i) {
  #ifdef DEBUG
  Serial.println("Getting user id");
  #endif
  int apiKeyIndex = getAPIKeyIndex(i);
  if (apiKeyIndex < 0) {
    #ifdef DEBUG
    Serial.println("API key not found, cannot find user!");
    #endif
    return "";
  }

  if (userIds[apiKeyIndex].length() == 0 && !fetchUserId(apiKeyIndex, i)) { 
    #ifdef DEBUG
    Serial.println("Could not get userId");
    #endif
    return "";
  }

  return userIds[apiKeyIndex];
}

bool checkTimer(int i) {
  #ifdef DEBUG
  Serial.println("Checking for running timer...");
  #endif
  String userId = getUserId(i);
  if (userId.length() == 0) {
    return false;
  }

  #ifdef DEBUG
  Serial.println("Trying to fetch id of running timer...");
  #endif
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(clockify_host, clockify_port)) {
    #ifdef DEBUG
    Serial.println("Connection failed");
    #endif
    return false;
  }

  client.print("GET /api/v1/workspaces/");
  client.print(clockifyTimers[i].workspaceId);
  client.print("/user/");
  client.print(userId);
  client.println("/time-entries?in-progress=1 HTTP/1.1");
  client.print("Host: ");
  client.println(clockify_host);
  client.print("X-Api-Key: ");
  client.println(clockifyTimers[i].apiKey);
  client.println("User-Agent: ESP32");
  client.println();

  yield();
  String line = client.readStringUntil('\n');
  String returnCode = line.substring(9, 12);
  #ifdef DEBUG
  Serial.println("Return code: " + returnCode);
  #endif

  yield();
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      #ifdef DEBUG
      Serial.println("Headers received");
      #endif
      break;
    }
  }

  yield();
  if (returnCode != "200") {
    #ifdef DEBUG
    Serial.println("Failed to fetch running timer:");
    Serial.println(line);
    while(client.available()) {
      Serial.println(client.readString());
    }
    #endif
    return false;
  }

  yield();
  line = client.readStringUntil('\n');
  if (line.length() == 2) {
    #ifdef DEBUG
    Serial.println("No running timer found");
    #endif
    updateTimerState(i, TIMER_PENDING);
    return true;
  }

  String object = line.substring(1, line.length() - 1);
  String timerId = jsonExtract(object, "id");
  String projectId = jsonExtract(object, "projectId");
  String taskId = jsonExtract(object, "taskId");
  String timeInterval = jsonExtract(object, "timeInterval");
  String startTime = jsonExtract(timeInterval, "start");

  #ifdef DEBUG
  Serial.println("Running timer found: ");
  Serial.println(" - Id: " + timerId);
  Serial.println(" - ProjectId: " + projectId);
  Serial.println(" - TaskId: " + taskId);
  Serial.println(" - StartTime: " + startTime);
  #endif

  if (taskId == "null") {
    taskId = "";
  }
  
  if (strcmp(projectId.c_str(), clockifyTimers[i].projectId) != 0) {
    #ifdef DEBUG
    Serial.println("Project id does not match");
    #endif
    updateTimerState(i, TIMER_PENDING);
    return true;
  }

  if (strcmp(projectId.c_str(), clockifyTimers[i].projectId) != 0) {
    #ifdef DEBUG
    Serial.println("Project id does not match");
    #endif
    updateTimerState(i, TIMER_PENDING);
    return true;
  }

  if (strlen(clockifyTimers[i].taskId) > 0 && strcmp(taskId.c_str(), clockifyTimers[i].taskId) != 0) {
    #ifdef DEBUG
    Serial.println("Task id does not match");
    #endif
    updateTimerState(i, TIMER_PENDING);
    return true;
  }

  #ifdef DEBUG
  Serial.println("Running timer matches the current one, updating start time of timer");
  #endif

  strcpy(clockifyTimers[i].id, timerId.c_str());
  strcpy(clockifyTimers[i].startTime, startTime.c_str());
  clockifyTimers[i].start = start = isoToUnixTime(startTime);
  updateTimerDesiredState(i, TIMER_RUNNING);
  updateTimerState(i, TIMER_RUNNING);
  updateTimerRunning();
  cancelProgress();

  return true;
}

bool startTimer(int i) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(clockify_host, clockify_port)) {
    #ifdef DEBUG
    Serial.println("Connection failed");
    #endif
    return false;
  }
  yield();
  char buffer[280];
  CStringBuilder sb(buffer, sizeof(buffer));
  #ifdef DEBUG
  Serial.print("ApiKey: ");
  Serial.println(clockifyTimers[i].apiKey);
  Serial.println("Creating body");
  #endif
  sb.print(F("{\"start\":\"")); // 10
  sb.print(clockifyTimers[i].startTime); // 24
  sb.print(F("\"")); // 1
  sb.print(F(",\"description\":\"")); // 16
  sb.print(clockifyTimers[i].description); // 120
  sb.print(F("\",\"projectId\":\"")); // 15
  sb.print(clockifyTimers[i].projectId); // 24
  if (strlen(clockifyTimers[i].taskId) > 0) {
    sb.print(F("\",\"taskId\":\"")); // 12
    sb.print(clockifyTimers[i].taskId); // 24
  }

  sb.print(F("\"")); // 2
  sb.print(F("}")); // 1
  yield();
  #ifdef DEBUG
  Serial.println("Body:");
  Serial.println(buffer);
  Serial.println("Sending request");
  #endif
  client.print("POST /api/v1/workspaces/");
  client.print(clockifyTimers[i].workspaceId);
  client.println("/time-entries/ HTTP/1.1");
  client.print("Host: ");
  client.println(clockify_host);
  client.print("X-Api-Key: ");
  client.println(clockifyTimers[i].apiKey);
  client.println("Content-Type: application/json");
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(sb.length());
  client.println();
  client.println(buffer);
  yield();
  updateTimerState(i, TIMER_START_SENT);

  String line = client.readStringUntil('\n');
  String returnCode = line.substring(9, 12);
  #ifdef DEBUG
  Serial.println("Return code: " + returnCode);
  #endif

  yield();
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      #ifdef DEBUG
      Serial.println("Headers received");
      #endif
      break;
    }
  }

  yield();
  if (returnCode != "201") {
    #ifdef DEBUG
    Serial.println("Failed to start timer:");
    Serial.println(line);
    while(client.available()) {
      Serial.println(client.readString());
    }
    #endif
    updateTimerState(i, TIMER_STARTING);
    return false;
  }

  yield();
  line = client.readStringUntil(',');
  String timerId = line.substring(7, 31);
  #ifdef DEBUG
  Serial.println("Timer id: " + timerId);
  #endif
  strcpy(clockifyTimers[i].id, timerId.c_str());
  client.stop();
  yield();

  updateTimerState(i, TIMER_RUNNING);
  updateTimerRunning();
  cancelProgress();
  return true;
}

bool stopTimer(int i) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(clockify_host, clockify_port)) {
    #ifdef DEBUG
    Serial.println("Connection failed");
    #endif
    return false;
  }

  yield();
  char buffer[280];
  CStringBuilder sb(buffer, sizeof(buffer));
  #ifdef DEBUG
  Serial.print("ApiKey: ");
  Serial.println(clockifyTimers[i].apiKey);
  Serial.println("Creating body");
  #endif
  sb.print(F("{\"start\":\"")); // 10
  sb.print(clockifyTimers[i].startTime); // 24
  sb.print(F("\",\"end\":\"")); // 9
  sb.print(clockifyTimers[i].endTime); // 24
  sb.print(F("\"")); // 1
  sb.print(F(",\"description\":\"")); // 16
  sb.print(clockifyTimers[i].description); // 120
  sb.print(F("\",\"projectId\":\"")); // 15
  sb.print(clockifyTimers[i].projectId); // 24
  if (strlen(clockifyTimers[i].taskId) > 0) {
    sb.print(F("\",\"taskId\":\"")); // 12
    sb.print(clockifyTimers[i].taskId); // 24
  }

  sb.print(F("\"")); // 2
  sb.print(F("}")); // 1
  yield();
  
  #ifdef DEBUG
  Serial.println("Body:");
  Serial.println(buffer);
  Serial.println("Sending request");
  #endif

  yield();
  client.print("PUT /api/v1/workspaces/");
  client.print(clockifyTimers[i].workspaceId);
  client.print("/time-entries/");
  client.print(clockifyTimers[i].id);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(clockify_host);
  client.print("X-Api-Key: ");
  client.println(clockifyTimers[i].apiKey);
  client.println("Content-Type: application/json");
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(sb.length());
  client.println();
  client.println(buffer);
  yield();
  
  updateTimerState(i, TIMER_STOP_SENT);
  yield();
  
  #ifdef DEBUG
  Serial.println("Request sent");
  #endif
  String line = client.readStringUntil('\n');
  client.stop();
  String returnCode = line.substring(9, 12);
  #ifdef DEBUG
  Serial.println("Return code: " + returnCode);
  #endif
  yield();

  if (returnCode != "200") {
    unsigned long secondsSinceStart = getEpochTime() - clockifyTimers[i].start;
    #ifdef DEBUG
    Serial.println("Timer failed to stop. Seconds since start of timer: " + String(secondsSinceStart));
    Serial.println(line);
    while(client.available()) {
      Serial.println(client.readString());
    }
    #endif
    updateTimerState(i, secondsSinceStart < 60 ? TIMER_STOP_SENT : TIMER_STOPPED);
    updateTimerRunning();
    return false;
  }
  yield();
  
  updateTimerState(i, TIMER_STOPPED);
  updateTimerRunning();
  return true;
}