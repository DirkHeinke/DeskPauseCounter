#include <Arduino.h>
#include <TM1637Display.h>
#include <RotaryEncoder.h>

#define LED_PIN 13
#define DISPLAY_CLK 0
#define DISPLAY_DIO 1

#define RE_SW 4
#define RE_CLK 3
#define RE_DT 2

enum DisplayMode
{
  Countdown,
  Pause,
  Work,
  MenuLong,
  MenuShort,
};

enum SystemMode
{
  CountdownLong,
  CountdownLongDone,
  CountdownShort,
  CountdownShortDone,
};

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
RotaryEncoder *encoder = new RotaryEncoder(RE_CLK, RE_DT, RotaryEncoder::LatchMode::TWO03);

DisplayMode displayMode = Countdown;
SystemMode systemMode = CountdownLong;

int durationLong = 30 * 1000 * 60;
int durationShort = 5 * 1000 * 60;
int counterValue = durationLong;
unsigned long lastRun = 0;
bool buttonClicked = false;

void OnButtonClicked(void)
{
  Serial.println("Button 1: clicked");
  buttonClicked = true;
}

void checkPosition()
{
  encoder->tick(); // just call tick() to check the state.
}

void updateDisplay(unsigned long now)
{
  bool displayOn = now % 1000 > 500;
  switch (displayMode)
  {
  case Work:
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
  case Pause:
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
  case Countdown:
  {
    int counterInSeconds = counterValue / 1000;
    int minutes = (counterInSeconds / 60);
    int seconds = (counterInSeconds - (minutes * 60)) % 60;
    display.showNumberDecEx(minutes, 0b01000000, false, 2U, 0U);
    display.showNumberDec(seconds, true, 2U, 2U);
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

void updateSystem(bool counterDone, bool button)
{
  switch (systemMode)
  {
  case CountdownLong:
  {
    if (counterDone)
    {
      systemMode = CountdownLongDone;
      displayMode = Pause;
    }
  }
  break;
  case CountdownLongDone:
  {
    if (button)
    {
      systemMode = CountdownShort;
      displayMode = Countdown;
      counterValue = durationShort;
    }
  }
  break;
  case CountdownShort:
  {
    if (counterDone)
    {
      systemMode = CountdownShortDone;
      displayMode = Work;
    }
  }
  break;
  case CountdownShortDone:
  {
    if (button)
    {
      systemMode = CountdownLong;
      displayMode = Countdown;
      counterValue = durationLong;
    }
  }
  break;
  default:
    break;
  }
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(RE_SW, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("Starting Desk Pause Counter");

  display.setBrightness(2);
  display.showNumberDec(0, false);

  attachInterrupt(digitalPinToInterrupt(RE_CLK), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RE_DT), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RE_SW), OnButtonClicked, CHANGE);

  counterValue = durationLong;
}

void loop()
{
  const unsigned long now = millis();
  static int pos = 0;

  bool counterDone = !updateCounter(now - lastRun);
  updateSystem(counterDone, buttonClicked);
  buttonClicked = false;
  updateDisplay(now);

    int newPos = encoder->getPosition();
  if (pos != newPos)
  {
    Serial.print("pos:");
    Serial.print(newPos);
    Serial.print(" dir:");
    Serial.println((int)(encoder->getDirection()));
    pos = newPos;
  }

  lastRun = now;
}