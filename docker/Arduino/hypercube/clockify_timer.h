enum TimerState {TIMER_STOPPED, TIMER_CHECKING, TIMER_PENDING, TIMER_STARTING, TIMER_START_SENT, TIMER_RUNNING, TIMER_STOPPING, TIMER_STOP_SENT};

typedef struct  {
    char id[25];
    char sideId[17];
    unsigned int start;
    char startTime[25];
    char endTime[25];
    char apiKey[49];
    char workspaceId[25];
    char projectId[25];
    char taskId[25];
    char description[65];
    TimerState state;
    TimerState desiredState;
} ClockifyTimer;

RTC_NOINIT_ATTR ClockifyTimer clockifyTimers[MAX_CLOCKIFY_TIMERS];

String stateToString(TimerState state) {
  switch (state) {
    case TIMER_STOPPED: {
      return "TIMER_STOPPED";
    }
    case TIMER_CHECKING: {
      return "TIMER_CHECKING";
    }
    case TIMER_PENDING: {
      return "TIMER_PENDING";
    }
    case TIMER_STARTING: {
      return "TIMER_STARTING";
    }
    case TIMER_START_SENT: {
      return "TIMER_START_SENT";
    }
    case TIMER_RUNNING: {
      return "TIMER_RUNNING";
    }
    case TIMER_STOPPING: {
      return "TIMER_STOPPING";
    }
    case TIMER_STOP_SENT: {
      return "TIMER_STOP_SENT";
    }
  }

  return "STATE NOT FOUND";
}

void initTimers() {
  for (int timerId = 0; timerId < MAX_CLOCKIFY_TIMERS; timerId++) {
    if (stateToString(clockifyTimers[timerId].state       ) != "STATE NOT FOUND" && 
        stateToString(clockifyTimers[timerId].desiredState) != "STATE NOT FOUND") {
      continue;   
    }

    memset(&clockifyTimers[timerId], 0, sizeof(ClockifyTimer));
  }
}

void clearTimers() {
  for (int timerId = 0; timerId < MAX_CLOCKIFY_TIMERS; timerId++) {
    memset(&clockifyTimers[timerId], 0, sizeof(ClockifyTimer));
  }
}

void printTimer(int timerId) {
  #ifdef DEBUG
  Serial.print("- id: "); Serial.println(clockifyTimers[timerId].id);
  Serial.print("- sideId: "); Serial.println(clockifyTimers[timerId].sideId);
  Serial.print("- startTime: "); Serial.println(clockifyTimers[timerId].startTime);
  Serial.print("- endTime: "); Serial.println(clockifyTimers[timerId].endTime);
  Serial.print("- apiKey: "); Serial.println(clockifyTimers[timerId].apiKey);
  Serial.print("- workspaceId: "); Serial.println(clockifyTimers[timerId].workspaceId);
  Serial.print("- projectId: "); Serial.println(clockifyTimers[timerId].projectId);
  Serial.print("- taskId: "); Serial.println(clockifyTimers[timerId].taskId);
  Serial.print("- description: "); Serial.println(clockifyTimers[timerId].description);
  Serial.print("- state: "); Serial.println(stateToString(clockifyTimers[timerId].state));
  Serial.print("- desiredState: "); Serial.println(stateToString(clockifyTimers[timerId].desiredState));
  #endif
}

void printTimers() {
  #ifdef DEBUG
  for (int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {
    if (clockifyTimers[i].state == TIMER_STOPPED && clockifyTimers[i].desiredState == TIMER_STOPPED) {
      continue;
    }

    Serial.println("+++ Timer " + String(i) + " +++");
    printTimer(i);
    Serial.println("--- Timer " + String(i) + " ---");
  }
  #endif
}

bool hasUnpersistedTimers = false;

void saveTimers() {
  #ifdef DEBUG
  Serial.println("saveTimers");
  #endif
  hasUnpersistedTimers = true;
}

void updateTimerDesiredState(int timerId, TimerState newState) {
  #ifdef DEBUG
  Serial.print("Updating timer "); 
  Serial.print(timerId); Serial.print(" desiredState from "); 
  Serial.print(stateToString(clockifyTimers[timerId].desiredState)); 
  Serial.print(" to "); 
  Serial.println(stateToString(newState));
  #endif
  clockifyTimers[timerId].desiredState = newState;
  saveTimers();
}

void updateTimerState(int timerId, TimerState newState) {
  #ifdef DEBUG
  Serial.print("Updating timer "); 
  Serial.print(timerId); Serial.print(" state from "); 
  Serial.print(stateToString(clockifyTimers[timerId].state)); 
  Serial.print(" to "); 
  Serial.println(stateToString(newState));
  #endif
  clockifyTimers[timerId].state = newState;
  if (newState == TIMER_RUNNING) {
    for(int i = 0; i < MAX_CLOCKIFY_TIMERS; i++) {
      if (i == timerId) {
        continue;
      }

      if (clockifyTimers[i].state == TIMER_STOPPED && clockifyTimers[i].desiredState == TIMER_STOPPED) {
        continue;
      }

      if (strcmp(clockifyTimers[i].apiKey, clockifyTimers[timerId].apiKey) == 0) {
        #ifdef DEBUG
        Serial.print("Found timer " + String(i) + " with same API key as newly started timer, ");
        Serial.println("setting as stopped as only one timer can be started per account");
        #endif
        updateTimerState(i, TIMER_STOPPED);
        updateTimerDesiredState(i, TIMER_STOPPED);
      }
    }
  }
  saveTimers();
}
