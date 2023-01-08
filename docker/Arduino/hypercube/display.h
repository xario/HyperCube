#include "fonts.h"
#include "SSD1306Wire.h" // https://github.com/ThingPulse/esp8266-oled-ssd1306 

SSD1306Wire* display = NULL;
QueueHandle_t displayQueue;

enum FontSize {FONT_SIZE_12, FONT_SIZE_36};
enum ProgressDirection {PROGRESS_DIRECTION_LEFT_TO_RIGHT, PROGRESS_DIRECTION_RIGHT_TO_LEFT};
enum DisplayItemType {DI_TYPE_TEXT, DI_TYPE_TEXT_PROGRESS, DI_TYPE_PROGRESS, DI_TYPE_PROGRESS_CANCEL, DI_TYPE_TEXT_PROGRESS_CANCEL};
class DisplayItem {
public:
  DisplayItem(
    DisplayItemType _type,
    String _text = "",
    FontSize _fontSize = FONT_SIZE_12,
    int32_t _progressStartTime = 0,
    int32_t _progressEndTime = 0,
    ProgressDirection _progressDirection = PROGRESS_DIRECTION_LEFT_TO_RIGHT
  ): type(_type), 
     fontSize(_fontSize),
     progressStartTime(_progressStartTime),
     progressEndTime(_progressEndTime),
     progressDirection(_progressDirection) 
  {
    strcpy(text, _text.c_str());
  }

  DisplayItem():
    type(DI_TYPE_TEXT), 
    text(""),
    fontSize(FONT_SIZE_12),
    progressStartTime(0),
    progressEndTime(0),
    progressDirection(PROGRESS_DIRECTION_LEFT_TO_RIGHT) 
  {
  }
  
  DisplayItemType type;
  char text[255];
  FontSize fontSize;
  int64_t progressStartTime;
  int64_t progressEndTime;
  ProgressDirection progressDirection;
};

DisplayItem progressItem;
DisplayItem lastItem;

bool progressShown = false;
bool isDisplayOff = false;
String lastMessage = "";
int lastColor = 0;

bool isScreenSaverActive() {
  if (idleFromTime == 0) {
    return false;
  }

  uint32_t now = millis();
  uint32_t diff = now - idleFromTime;
  return diff > SCREEN_SAVER_TIMEOUT_SECONDS * 1000; 
}

void initDisplay() {
  if (display != NULL) {
    return;
  }
  
  #ifdef DEBUG
  Serial.println("Initializing display...");
  #endif
  
  display = new SSD1306Wire(0x3c, SDA, SCL, GEOMETRY_128_32);
  display->init();
  display->flipScreenVertically();
  display->setBrightness(128); 
}

void drawItem(DisplayItem& item) {
  switch (item.type) {
    case DI_TYPE_TEXT_PROGRESS:
      #ifdef DEBUG
      //Serial.println("TEXT PROGRESS");
      #endif
      lastItem = item;
    case DI_TYPE_PROGRESS:
      #ifdef DEBUG
      //Serial.println("PROGRESS");
      #endif
      progressItem = item;
      break;
    case DI_TYPE_TEXT_PROGRESS_CANCEL:
      #ifdef DEBUG
      //Serial.println("TEXT_PROGRESS_CANCEL");
      #endif
      lastItem = item;
    case DI_TYPE_PROGRESS_CANCEL:
      #ifdef DEBUG
      //Serial.println("PROGRESS_CANCEL");
      #endif
      progressItem = item;
      break;
    case DI_TYPE_TEXT:
      #ifdef DEBUG
      //Serial.println("TEXT");
      #endif
      lastItem = item;
      break;
  }
}

void refreshDisplay() {
  if (isScreenSaverActive()) {
    display->displayOff();
    isDisplayOff = true;
    return;
  }

  if (isDisplayOff) {
    display->displayOn();
    isDisplayOff = false;
  }
  
  String text = lastItem.text;
  bool updateText = false;
  if (lastMessage != text) {
    #ifdef DEBUG
    Serial.println(text);
    #endif
    lastMessage = text;
    updateText = true;
  }

  int color = rtc.getHour(true) % 2 != 0;
  bool updateColor = lastColor != color;
  
  lastColor = color;

  uint32_t diff = progressItem.progressEndTime - progressItem.progressStartTime;
  uint32_t now = millis();
  bool updateProgress = diff > 0 && progressItem.progressEndTime > now;

  if (!updateText && !updateColor && !updateProgress && !progressShown) {
    return;
  }

  display->clear();

  if (color != 0) {
    display->invertDisplay();
  } else {
    display->normalDisplay();
  }

  display->setColor(INVERSE);
  
  if (updateProgress) {
    progressShown = true;
    uint32_t elapsed = now - progressItem.progressStartTime;
    int16_t width = 128 * elapsed / diff;
    
    if (progressItem.progressDirection == PROGRESS_DIRECTION_RIGHT_TO_LEFT) {
      display->fillRect(128 - width, 0, 128, 32);  
    } else {
      display->fillRect(0, 0, width, 32);
    }
  } else {
    progressShown = false;
  }
  
  if (lastItem.fontSize == FONT_SIZE_12) {
    display->setFont(Roboto_Condensed_12);
  } else {
    display->setFont(Roboto_Condensed_36);
  }
    
  int lineBreakIndex = text.indexOf('\n');
  if (lineBreakIndex > 0) {
    String line1 = text.substring(0, lineBreakIndex);
    String line2 = text.substring(lineBreakIndex, text.length());
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawStringMaxWidth(0, 0, 128, line1);
    display->drawStringMaxWidth(0, 16, 128, line2);
  } else {
    display->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    int yOffset = 16;
    if (display->getStringWidth(text) > 128) {
      yOffset = 8;
    }

    display->drawStringMaxWidth(64, yOffset, 128, text);
  }

  display->display();
}

void updateDisplay(void * parameter) {
  initDisplay();
  while (true) {
    DisplayItem di;
    unsigned short lastBank = 0;
    while(xQueueReceive(displayQueue, &di, 0) == pdTRUE) {
      drawItem(di);
    }

    refreshDisplay();
    delay(10);
  }
}

void initDisplayQueue() {
  displayQueue = xQueueCreate(10, sizeof(DisplayItem));
  if (errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY == xTaskCreatePinnedToCore(updateDisplay, "display", 11000, NULL, 10, NULL, 0)) {
    Serial.println("Could not create display task, sleeping forever...");
    while(true) {
      delay(10000);
    }
  }
}

void displayText(String text, FontSize fontSize = FONT_SIZE_12) {
  DisplayItem item(
    DI_TYPE_TEXT,
    text,
    fontSize
  );
  
  xQueueSend(displayQueue, &item, portMAX_DELAY);
}

void displayProgress(uint32_t duration, String text = "", FontSize fontSize = FONT_SIZE_12) {
  DisplayItem item(
    text == "" ? DI_TYPE_PROGRESS : DI_TYPE_TEXT_PROGRESS,
    text,
    fontSize,
    millis(),
    millis() + duration
  );
  
  xQueueSend(displayQueue, &item, portMAX_DELAY);
}

void displayReverseProgress(uint32_t duration, String text = "", FontSize fontSize = FONT_SIZE_12) {
    DisplayItem item(
    text == "" ? DI_TYPE_PROGRESS : DI_TYPE_TEXT_PROGRESS,
    text,
    fontSize,
    millis(),
    millis() + duration,
    PROGRESS_DIRECTION_RIGHT_TO_LEFT
  );
  
  xQueueSend(displayQueue, &item, portMAX_DELAY);
}

void cancelProgress(String text = "", FontSize fontSize = FONT_SIZE_12) {
  DisplayItem item(
    text == "" ? DI_TYPE_PROGRESS_CANCEL : DI_TYPE_TEXT_PROGRESS_CANCEL,
    text,
    fontSize
  );
  
  xQueueSend(displayQueue, &item, portMAX_DELAY);
}
