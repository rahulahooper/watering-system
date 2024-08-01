#include "Display.h"

Display::Display(const uint8_t* dp, const uint8_t* sp)
{
  // assign digit select pins
  for (int i = 0; i < 4; i++)
  {
    digitPins[i] = dp[i];
    pinMode(digitPins[i], OUTPUT);
  }

  // assign segment select pins
  for (int i = 0; i < 7; i++)
  {
    segmentPins[i] = sp[i];
    pinMode(segmentPins[i], OUTPUT);
  }
}

void Display::clear()
{
  // wipe out what is currently on the display
  for (int d = 0; d < NUM_DIGITS; d++)
  {
    digitalWrite(digitPins[d], LOW);
  }

  for (int i = 0; i < NUM_SEGMENTS; i++)
  {
    digitalWrite(segmentPins[i], LOW);
  }
}

void Display::enableProgramming(bool en)
{
  programmingEnabled_ = en;
}

uint16_t Display::getNumToWrite()
{
  return numToWrite_;
}

void Display::enableBlinking(uint16_t mask)
{
  // set blinking bitmask. If bit is 1, then this digit
  // will blink according to an internal timer. If bit is 0
  // we just display it as is, with no blinking.
  blinkMask_ = (mask & 0b1111);

  blinkingEnabled_ = true;
  blinkStatus_     = BLINK_OFF;
  
  // reset internal timer
  timeOfBlinkToggle_ = millis();
}

void Display::disableBlinking()
{
  // clear out blinking bitmask
  blinkMask_ = 0b0000;
  blinkingEnabled_ = false;
}

void Display::writeDigit(uint16_t num, uint16_t digit)
{
  assert(num >= 0 && num <= 9);
  assert(digit < NUM_DIGITS);

  clear();

  // enable the digit we want to write to
  digitalWrite(digitPins[digit], HIGH);

  // figure out if blinking applies to this digit
  // if it does, then don't light it up on the display
  uint8_t** digitSegments = nums[num];
  if (blinkingEnabled_ && blinkStatus_ == BLINK_OFF && (blinkMask_ & (0b1 << digit)))
  {
    digitSegments = none;
  }
  //uint8_t** digitSegments = (blinkMask_ & (0b1 << digit)) ? nums[num] : none;

  // figure out how many segments we will write to for this digit
  int numSegments = 0;
  while (digitSegments[numSegments] != NULL)
  {
    numSegments++;
  }

  // enable all segments within digit
  for (int i = 0; i < numSegments; i++)
  {
    digitalWrite(*digitSegments[i], HIGH);
  } 
}


void Display::handleProgramNone(Joystick& joystick, WaterSetting& waterSetting)
{
  // display time until next watering
  numToWrite_ = waterSetting.toNumber(false);
  
  // get joystick information
  JoystickState_t currJoystickState = joystick.getState();
  uint8_t JOYSTICK_X_OUT_PIN = joystick.getXOutPin();
  uint8_t JOYSTICK_Y_OUT_PIN = joystick.getYOutPin();

  // if joystick is in programming mode, put display into programming mode
  if (currJoystickState == STATE_PROG)
  {
    // enableBlinking(0b0011);
    currDisplayState_ = PROGRAM_DURATION;
  }
}


void Display::handleProgramFrequency(Joystick& joystick, WaterSetting& waterSetting)
{
  // display frequency and duration
  numToWrite_ = waterSetting.toNumber(true);

  // check if blinking enabled and blinking bitmask is correct
  if (!blinkingEnabled_)
  {
    enableBlinking(0b1100);
  }
  blinkMask_ = 0b1100;
  
  // get joystick information
  JoystickState_t currJoystickState = joystick.getState();
  uint8_t JOYSTICK_X_OUT_PIN = joystick.getXOutPin();
  uint8_t JOYSTICK_Y_OUT_PIN = joystick.getYOutPin();

  // if joystick is in normal mode, put display into
  // normal mode and turn off blinking
  if (currJoystickState == STATE_NORMAL)
  {
    disableBlinking();
    currDisplayState_ = PROGRAM_NONE;
    return;
  }

  prevXOut = xOut;
  prevYOut = yOut;
  xOut = analogRead(JOYSTICK_X_OUT_PIN);
  yOut = analogRead(JOYSTICK_Y_OUT_PIN);

  // if joystick toggled in x direction, change to programming watering duration
  if ((xOut < 10 || xOut > 4080) && (prevXOut >= 10 && prevXOut <= 4080))
  {
    currDisplayState_ = PROGRAM_DURATION;
  }
  else if (yOut < 10 && prevYOut >= 10)
  {
    // if joystick toggled downwards, decrement watering frequency
    waterSetting.decrementFrequency(); 
  }
  else if (yOut > 4080 && prevYOut <= 4080)
  {
    // if joystick toggled upwards, increment watering frequency
    waterSetting.incrementFrequency();
  }

}


void Display::handleProgramDuration(Joystick& joystick, WaterSetting& waterSetting)
{
  // display frequency and duration
  numToWrite_ = waterSetting.toNumber(true);

  // check if blinking enabled and blinking bitmask is correct
  if (!blinkingEnabled_)
  {
    enableBlinking(0b0011);
  }
  blinkMask_ = 0b0011;

  // get joystick information
  JoystickState_t currJoystickState = joystick.getState();
  uint8_t JOYSTICK_X_OUT_PIN = joystick.getXOutPin();
  uint8_t JOYSTICK_Y_OUT_PIN = joystick.getYOutPin();

  // if joystick in normal state, transition display back to normal state
  if (currJoystickState == STATE_NORMAL)
  {
    disableBlinking();
    currDisplayState_ = PROGRAM_NONE;
    return;
  }

  // otherwise, read joystick and handle values
  prevXOut = xOut;
  prevYOut = yOut;
  xOut = analogRead(JOYSTICK_X_OUT_PIN);
  yOut = analogRead(JOYSTICK_Y_OUT_PIN);
  
  if ((xOut < 10 || xOut > 4080) && (prevXOut >= 10 && prevXOut <= 4080))
  {
    currDisplayState_ = PROGRAM_FREQUENCY;
  }
  else if (yOut < 10 && prevYOut >= 10)
  {
    waterSetting.decrementDuration();
  }
  else if (yOut > 4080 && prevYOut <= 4080)
  {
    waterSetting.incrementDuration();
  }
}


void Display::handleDisplayStateMachine(Joystick& joystick, WaterSetting& waterSetting)
{

  JoystickState_t currJoystickState = joystick.getState();
  uint8_t JOYSTICK_X_OUT_PIN = joystick.getXOutPin();
  uint8_t JOYSTICK_Y_OUT_PIN = joystick.getYOutPin();

  // handle internal timer for blinking
  // handle integer overflow
  unsigned long timeNow = millis();
  if (timeNow < timeOfBlinkToggle_)
  {
    timeOfBlinkToggle_ = millis();
    timeNow = millis();
  }
  
  unsigned long timeSincePrevBlinkToggle = timeNow - timeOfBlinkToggle_;
  if (blinkingEnabled_ && timeSincePrevBlinkToggle > displayBlinkPeriodMs)
  {
    timeOfBlinkToggle_ = millis();
    blinkStatus_ = !blinkStatus_;  
  }
  
  // if programming disabled, force state to PROGRAM_NONE
  if (!programmingEnabled_)
  {
    currDisplayState_ = PROGRAM_NONE;
    joystick.setState(STATE_NORMAL);
  }
  
  // handle display state
  switch(currDisplayState_)
  {
    case PROGRAM_NONE:
      handleProgramNone(joystick, waterSetting);
    break;

    case PROGRAM_FREQUENCY:
      handleProgramFrequency(joystick, waterSetting);
    break;
    
    case PROGRAM_DURATION:
      handleProgramDuration(joystick, waterSetting);
    break;

    default:
      Serial.println("Display in unknown state!!");
  }
}
