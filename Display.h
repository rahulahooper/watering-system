#ifndef DISPLAY_H
#define DISPLAY_H

#include "Arduino.h"
#include <stdint.h>
#include "Joystick.h"
#include "WateringSystem.h"

typedef enum
{
  PROGRAM_NONE,           // not in programming mode
  PROGRAM_DURATION,       // programming duration setting
  PROGRAM_FREQUENCY       // programming frequency setting
} DisplayProgramState_t;

class Display
{
  public:
    Display(const uint8_t* dp, const uint8_t* sp);

    void handleDisplayStateMachine(Joystick& joystick,
                                   WaterSetting& WaterSetting);

    uint16_t getNumToWrite();
    void writeNumber(uint16_t num);
    void writeDigit(uint16_t num, uint16_t digit);
    void clear();
    void enableProgramming(bool en);
    void enableBlinking(uint16_t mask);
    void disableBlinking();

  private:

    /// State machine logic
    DisplayProgramState_t currDisplayState_ = PROGRAM_NONE;
    void handleProgramNone(Joystick& joystick, WaterSetting& waterSetting);
    void handleProgramDuration(Joystick& joystick, WaterSetting& waterSetting);
    void handleProgramFrequency(Joystick& joystick, WaterSetting& waterSetting);
    
    bool programmingEnabled_ = true;    // can only enter programming modes if this is true

    /// Blinking logic
    enum
    {
      BLINK_OFF = 0,
      BLINK_ON  = 1
    };
    
    const unsigned long displayBlinkPeriodMs = 500;
    unsigned long timeOfBlinkToggle_ = 0;    // absolute time that blink setting was last toggled
    bool blinkingEnabled_ = false;
    int blinkStatus_ = BLINK_ON;             // toggles after `displayBlinkPeriodMs` have elapsed
    uint16_t blinkMask_ = 0b1111;

    uint16_t numToWrite_ = 0;                // number we will write to the display depending on state and watering settings
    uint16_t prevXOut = 0, xOut = 0, prevYOut = 0, yOut = 0;

    const uint8_t NUM_DIGITS   = 4;
    const uint8_t NUM_SEGMENTS = 7;
    
    uint8_t digitPins[4];
    uint8_t segmentPins[7];

    uint8_t* SEG_A = &segmentPins[0];
    uint8_t* SEG_B = &segmentPins[1];
    uint8_t* SEG_C = &segmentPins[2];
    uint8_t* SEG_D = &segmentPins[3];
    uint8_t* SEG_E = &segmentPins[4];
    uint8_t* SEG_F = &segmentPins[5];
    uint8_t* SEG_G = &segmentPins[6];

    // define which segments should be lit up for which characters (null-terminated)
    uint8_t* none[1]  = {NULL};
    uint8_t* one[3]   = {SEG_B, SEG_C, NULL};
    uint8_t* two[6]   = {SEG_A, SEG_B, SEG_G, SEG_E, SEG_D, NULL};
    uint8_t* three[8] = {SEG_A, SEG_B, SEG_G, SEG_C, SEG_D, NULL};
    uint8_t* four[5]  = {SEG_F, SEG_G, SEG_B, SEG_C, NULL};
    uint8_t* five[6]  = {SEG_A, SEG_F, SEG_G, SEG_C, SEG_D, NULL};
    uint8_t* six[7]   = {SEG_A, SEG_F, SEG_G, SEG_C, SEG_D, SEG_E, NULL};
    uint8_t* seven[5] = {SEG_F, SEG_A, SEG_B, SEG_C, NULL};
    uint8_t* eight[8] = {SEG_A, SEG_B, SEG_F, SEG_G, SEG_C, SEG_D, SEG_E, NULL};
    uint8_t* nine[6]  = {SEG_A, SEG_B, SEG_F, SEG_G, SEG_C, NULL};
    uint8_t* zero[7]  = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, NULL};

    // array of arrays
    uint8_t** nums[10] = {zero, one, two, three, four, five, six, seven, eight, nine};
}; 

#endif // DISPLAY_H
