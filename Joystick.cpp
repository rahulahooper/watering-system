#include "Joystick.h"
#include "Arduino.h"

Joystick::Joystick(uint8_t x, uint8_t y, uint8_t s)
{
  joystickXOutPin_   = x;
  joystickYOutPin_   = y;
  joystickSelectPin_ = s;

  pinMode(joystickXOutPin_,   INPUT);
  pinMode(joystickYOutPin_,   INPUT);
  pinMode(joystickSelectPin_, INPUT);
}

JoystickState_t Joystick::handleJoystickStateMachine()
{
//  static int prevSelect = 0;
//  static int currSelect = 0;
//  static unsigned long startTime = 0;

  timeNow_ = millis();

  // handle overflow
  if (timeNow_ < stateTransitionStartTime_)
  {
    stateTransitionStartTime_ = millis();
    return joystickState_;
  }

  // read select pin. select pin goes low if pressed, so invert what 
  // we read from that pin. that way, if pin is pressed, currSelectVal goes high.
  prevSelectVal_ = currSelectVal_;
  currSelectVal_ = digitalRead(joystickSelectPin_);
  currSelectVal_ = !currSelectVal_;   

  // figure out what state to transition to
  if (joystickState_ == STATE_NORMAL)
  {
    if (prevSelectVal_ == 0 && currSelectVal_ == 1)
    {
      joystickState_ = STATE_NORMAL_TO_PROG;
      stateTransitionStartTime_ = millis();
    }
  }
  else if (joystickState_ == STATE_NORMAL_TO_PROG)
  {
    if (currSelectVal_ == 0)
    {
      joystickState_ = STATE_NORMAL;
    }
    else if (currSelectVal_ == 1 && (timeNow_ - stateTransitionStartTime_ >= normalToProgTimeMs_))
    {
      joystickState_ = STATE_PROG;
    }
  }
  else if (joystickState_ == STATE_PROG)
  {
    if (prevSelectVal_ == 0 && currSelectVal_ == 1)
    {
      joystickState_ = STATE_PROG_TO_NORMAL;
      stateTransitionStartTime_ = millis();
    }
  }
  else if (joystickState_ == STATE_PROG_TO_NORMAL)
  {
    if (currSelectVal_ == 0)
    {
      joystickState_ = STATE_PROG;
    }
    else if (currSelectVal_ == 1 && (timeNow_ - stateTransitionStartTime_ >= progToNormalTimeMs_))
    {
      joystickState_ = STATE_NORMAL;
    }
  }

  return joystickState_;
}

unsigned long Joystick::getTimeSinceStateEnteredMs()
{
  timeNow_ = millis();

  // millis() can overflow and wrap back around to zero
  // we handle this edge case here
  if (timeNow_ < stateTransitionStartTime_)
  {
    unsigned long maxValue = 0;
    maxValue -= 1;

    return (maxValue - stateTransitionStartTime_) + timeNow_;
  }

  return timeNow_ - stateTransitionStartTime_;
}
