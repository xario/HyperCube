#include <CStringBuilder.h>
#include <WiFiClientSecure.h>

const char* clockify_host = "api.clockify.me";
int clockify_port = 443;

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
    Serial.println(client.readStringUntil('\n'));
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
