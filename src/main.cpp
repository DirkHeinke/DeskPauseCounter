#include <Arduino.h>
#include <TM1637Display.h>
#include <PinButton.h>
#include <FlashStorage_SAMD.h>

#define DISPLAY_CLK 0
#define DISPLAY_DIO 1
#define BUTTON 2
#define BUZZER 3

#define DEFAULT_DURATION_LONG 30
#define DEFAULT_DURATION_SHORT 5

enum class DisplayMode
{
  Countdown,
  Pause,
  Work,
  Off
};

enum class SystemMode
{
  CountdownLong,
  CountdownLongDone,
  CountdownShort,
  CountdownShortDone,
  Off,
};

enum ButtonClicked
{
  False,
  Short,
  Long,
  Double,
};

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
PinButton button(BUTTON);
DisplayMode displayMode = DisplayMode::Countdown;
SystemMode systemMode = SystemMode::CountdownLong;

int durationLong = 0;
int durationShort = 0;
int counterValue = durationLong;
unsigned long lastRun = 0;
ButtonClicked buttonClicked = False;
const int WRITTEN_SIGNATURE = 12345;
unsigned long nextNote = 0;
int noteCounter = 0;

void updateDisplay(unsigned long now)
{
  bool displayOn = now % 1000 > 500;
  switch (displayMode)
  {
  case DisplayMode::Work:
  {
    if (!displayOn)
    {
      display.clear();
      break;
    }
    uint8_t WORK[] = {
        SEG_F | SEG_E | SEG_D | SEG_C | SEG_B,         // U
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
        SEG_A | SEG_B | SEG_C | SEG_G | SEG_E | SEG_F, // R
        SEG_F | SEG_E | SEG_G | SEG_B | SEG_C,         // K
    };
    display.setSegments(WORK);
  }
  break;
  case DisplayMode::Pause:
  {
    if (!displayOn)
    {
      display.clear();
      break;
    }
    uint8_t WORK[] = {
        SEG_E | SEG_F | SEG_A | SEG_B | SEG_G,         // P
        SEG_A | SEG_B | SEG_C | SEG_G | SEG_E | SEG_F, // A
        SEG_F | SEG_E | SEG_D | SEG_C | SEG_B,         // U
        SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,         // S
    };
    display.setSegments(WORK);
  }
  break;
  case DisplayMode::Countdown:
  {
    int counterInSeconds = counterValue / 1000;
    int minutes = (counterInSeconds / 60);
    int seconds = (counterInSeconds - (minutes * 60)) % 60;
    display.showNumberDecEx(minutes, 0b01000000, false, 2U, 0U);
    display.showNumberDec(seconds, true, 2U, 2U);
  }
  break;
  case DisplayMode::Off:
  {
    display.clear();
  }
  break;

  default:
    break;
  }
}

/**
 * Updates Counter, true when updatable, false when done
 */
bool updateCounter(unsigned long tDiff)
{
  counterValue -= tDiff;
  if (counterValue <= 0)
  {
    counterValue = 0;
    return false;
  }
  return true;
}

void updateSystem(bool counterDone, ButtonClicked button)
{
  switch (systemMode)
  {
  case SystemMode::CountdownLong:
  {
    if (counterDone)
    {
      systemMode = SystemMode::CountdownLongDone;
      displayMode = DisplayMode::Pause;
    }
    if (button == Long)
    {
      systemMode = SystemMode::CountdownShort;
      displayMode = DisplayMode::Countdown;
      counterValue = durationShort;
    }
    if (button == Double)
    {
      systemMode = SystemMode::Off;
      displayMode = DisplayMode::Off;
    }
  }
  break;
  case SystemMode::CountdownLongDone:
  {
    if (button == Short)
    {
      systemMode = SystemMode::CountdownShort;
      displayMode = DisplayMode::Countdown;
      counterValue = durationShort;
      nextNote = 0;
      noteCounter = 0;
    }
    if (button == Double)
    {
      systemMode = SystemMode::Off;
      displayMode = DisplayMode::Off;
      nextNote = 0;
      noteCounter = 0;
    }
    // play beep melody and repeat every minute
    if (nextNote == 0)
    {
      nextNote = millis();
    }
    else if (millis() > nextNote)
    {
      switch (noteCounter)
      {
      case 0:
        tone(BUZZER, 8000, 100);
        nextNote = millis() + 200;
        noteCounter++;
        break;
      case 1:
        tone(BUZZER, 5000, 100);
        nextNote = millis() + 200;
        noteCounter++;
        break;
      case 2:
        tone(BUZZER, 8000, 100);
        nextNote = millis() + 60000;
        noteCounter = 0;
        break;
      default:
        break;
      }
    }
  }
  break;
  case SystemMode::CountdownShort:
  {
    if (counterDone)
    {
      systemMode = SystemMode::CountdownShortDone;
      displayMode = DisplayMode::Work;
    }
    if (button == Long)
    {
      systemMode = SystemMode::CountdownLong;
      displayMode = DisplayMode::Countdown;
      counterValue = durationLong;
    }
    if (button == Double)
    {
      systemMode = SystemMode::Off;
      displayMode = DisplayMode::Off;
    }
  }
  break;
  case SystemMode::CountdownShortDone:
  {
    if (button == Short)
    {
      systemMode = SystemMode::CountdownLong;
      displayMode = DisplayMode::Countdown;
      counterValue = durationLong;
    }
    if (button == Double)
    {
      systemMode = SystemMode::Off;
      displayMode = DisplayMode::Off;
    }
  }
  break;
  case SystemMode::Off:
  {
    if (button != False)
    {
      systemMode = SystemMode::CountdownLong;
      displayMode = DisplayMode::Countdown;
      counterValue = durationLong;
    }
  }
  default:
    break;
  }
}

void printCurrentDurations()
{
  Serial.print("New durations in milliseconds WORK/PAUSE ");
  Serial.print(durationLong);
  Serial.print("/");
  Serial.println(durationShort);
}

void readDurationsFromEeprom()
{
  int signature;
  EEPROM.get(0, signature);

  if (signature != WRITTEN_SIGNATURE)
  {
    durationLong = DEFAULT_DURATION_LONG * 1000 * 60;
    durationShort = DEFAULT_DURATION_SHORT * 1000 * 60;
    Serial.println("EEPROM empty, filled with defaults");
  }
  else
  {
    int eeAddress = sizeof(WRITTEN_SIGNATURE);
    EEPROM.get(eeAddress, durationLong);
    eeAddress += sizeof(durationLong);
    EEPROM.get(eeAddress, durationShort);
    Serial.println("Read from EEPROM");
  }
  printCurrentDurations();
}

void writeDurationsToEeprom()
{
  int eeAddress = 0;
  EEPROM.put(eeAddress, WRITTEN_SIGNATURE);
  eeAddress += sizeof(WRITTEN_SIGNATURE);
  EEPROM.put(eeAddress, durationLong);
  eeAddress += sizeof(durationLong);
  EEPROM.put(eeAddress, durationShort);
  EEPROM.commit();
}

void readSerial()
{
  if (Serial.available() > 0)
  {
    String newTimes = Serial.readStringUntil('\n');
    newTimes.trim();
    int slashIndex = newTimes.indexOf("/");
    if (slashIndex != 0 && newTimes.length() >= slashIndex + 1)
    {
      durationLong = newTimes.substring(0, slashIndex).toInt() * 1000 * 60;
      durationShort = newTimes.substring(slashIndex + 1).toInt() * 1000 * 60;
      if (durationLong != 0 || durationShort != 0)
      {
        printCurrentDurations();
        writeDurationsToEeprom();
        NVIC_SystemReset();
        return;
      }
    }

    Serial.println("To set new duration, send WORK/PAUSE in minutes. E.g. 30/5");
  }
}

void setup()
{
  delay(1000);
  Serial.begin(9600);

  Serial.println("Starting Desk Pause Counter");

  readDurationsFromEeprom();

  display.setBrightness(2);
  display.showNumberDec(0, false);

  counterValue = durationLong;
}

void loop()
{
  const unsigned long now = millis();

  bool counterDone = !updateCounter(now - lastRun);
  updateSystem(counterDone, buttonClicked);
  buttonClicked = False;
  updateDisplay(now);

  button.update();
  if (button.isSingleClick())
  {
    tone(BUZZER, 8000, 200);
    Serial.println("Short");
    buttonClicked = Short;
  }
  else if (button.isLongClick())
  {
    tone(BUZZER, 8000, 200);
    Serial.println("Long");
    buttonClicked = Long;
  }
  else if (button.isDoubleClick())
  {
    tone(BUZZER, 8000, 200);
    Serial.println("Double");
    buttonClicked = Double;
  }

  readSerial();
  lastRun = now;
}
