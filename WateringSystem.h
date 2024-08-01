#ifndef WATERING_SYSTEM_H
#define WATERING_SYSTEM_H

struct WaterSetting
{
  WaterSetting() {
    Serial.println("Constructor called!");
    frequency = 0;
    duration = 0;
  }
  WaterSetting(uint16_t f, uint16_t d) {
    frequency = f;
    duration = d;
  }

  uint16_t frequency = 0;                 // how often to water (days)
  uint16_t duration  = 0;                 // how long to water  (seconds)
  double   timeUntilNextWatering = 0.0;   // seconds until next watering

  const uint16_t maxValue = 99;
  const uint16_t minValue = 1;

  void decrementDuration()
  {
    duration = (duration > minValue) ? duration-1 : duration;
  }

  void incrementDuration()
  {
    duration = (duration < maxValue) ? duration+1 : duration;
  }

  void decrementFrequency()
  {
    frequency = (frequency > minValue) ? frequency-1 : frequency;
  }

  void incrementFrequency()
  {
    frequency = (frequency < maxValue) ? frequency+1 : frequency;
  }
  
  // convert setting to a 4-digit number we can display
  uint16_t toNumber(bool frequencyAndDuration)
  {
    if (frequencyAndDuration)
    {
      // display watering frequency (days) in first two digits
      // display watering duration (seconds) in trailing two digits
      // clamp duration and frequency to [0,99]
      // we expect the user to do this already but do it here for caution
      frequency = min(frequency, maxValue);
      duration  = min(duration, maxValue);
      return frequency * 100 + duration;
    }
    else
    {
      // display days until next watering in first two digits
      // display hours until next watering in trailing two digits
      timeUntilNextWatering = max(timeUntilNextWatering, 0.0);

      // TODO: should fix this weird ass casting but it works for now
      /***
       * When ready to wait days for watering, uncomment this block and get rid of block below
      uint16_t daysUntilNextWatering  = min( static_cast<int>(timeUntilNextWatering) / 60 / 60 / 24, (int)maxValue );
      uint16_t hoursUntilNextWatering = min( (static_cast<int>(timeUntilNextWatering) / 60 / 60) % 24, (int)maxValue );  
      
      return daysUntilNextWatering * 100 + hoursUntilNextWatering;
      */

      uint16_t minutesUntilNextWatering = min( static_cast<int>(timeUntilNextWatering) / 60, (int)maxValue);
      uint16_t secondsUntilNextWatering = min( static_cast<int>(timeUntilNextWatering) - minutesUntilNextWatering*60, (int)maxValue);
      return minutesUntilNextWatering * 100 + secondsUntilNextWatering;
    }
  }

  String toString(bool frequencyAndDuration)
  {
    return String(toNumber(frequencyAndDuration));
  }

};

#endif // WATERING_SYSTEM_H
