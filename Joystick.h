#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

typedef enum
{
  STATE_NORMAL,
  STATE_NORMAL_TO_PROG,
  STATE_PROG,
  STATE_PROG_TO_NORMAL
} JoystickState_t;

class Joystick
{
  public:

    Joystick(uint8_t x, uint8_t y, uint8_t s);

    JoystickState_t handleJoystickStateMachine();

    JoystickState_t getState() { return joystickState_; }
    void setState(const JoystickState_t js) { joystickState_ = js; }

    uint8_t getXOutPin() { return joystickXOutPin_; }
    uint8_t getYOutPin() { return joystickYOutPin_; }
    
    unsigned long getTimeSinceStateEnteredMs();

  private:
  
    JoystickState_t joystickState_ = STATE_NORMAL;
    
    uint8_t joystickXOutPin_;
    uint8_t joystickYOutPin_;
    uint8_t joystickSelectPin_;

    int prevSelectVal_ = 0;
    int currSelectVal_ = 0;

    unsigned long stateTransitionStartTime_;
    unsigned long timeNow_;

    const unsigned long normalToProgTimeMs_ = 3000;
    const unsigned long progToNormalTimeMs_ = 2000;
    
};

#endif // JOYSTICK_H
