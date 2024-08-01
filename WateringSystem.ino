#include <ESP32Time.h>
#include <time.h>
#include "driver/rtc_io.h"
#include "Joystick.h"
#include "Display.h"
#include "WateringSystem.h"

#define BOOT_REASON_MESSAGE_LEN 150

///////////////////////////////////////////////////////////////
/// GPIO PINS
///////////////////////////////////////////////////////////////

// Joystick pins and instances
const uint8_t JOYSTICK_X_OUT_PIN  = 26;
const uint8_t JOYSTICK_Y_OUT_PIN  = 27;
const uint8_t JOYSTICK_SELECT_PIN = 34;

// Display pins and instances
const uint8_t DIGIT0_PIN = 4;      // most significant digit
const uint8_t DIGIT1_PIN = 16;
const uint8_t DIGIT2_PIN = 2;
const uint8_t DIGIT3_PIN = 15;     // least significant digit

const uint8_t DIGITS[4] = { 
  DIGIT0_PIN, 
  DIGIT1_PIN, 
  DIGIT2_PIN, 
  DIGIT3_PIN 
};

// Pins for each segment on display
const uint8_t SEGMENT_A_PIN = 23;
const uint8_t SEGMENT_B_PIN = 17; // SEG_B and DP are shorted together for my hex display
const uint8_t SEGMENT_C_PIN = 18;
const uint8_t SEGMENT_D_PIN = 21;
const uint8_t SEGMENT_E_PIN = 19;
const uint8_t SEGMENT_F_PIN = 22;
const uint8_t SEGMENT_G_PIN = 5;


const uint8_t SEGMENTS[7] = { 
  SEGMENT_A_PIN, 
  SEGMENT_B_PIN, 
  SEGMENT_C_PIN, 
  SEGMENT_D_PIN, 
  SEGMENT_E_PIN, 
  SEGMENT_F_PIN, 
  SEGMENT_G_PIN 
};

// Relay signal pin for turning on water pump
const gpio_num_t RELAY_SIGNAL_PIN = (gpio_num_t)32;



///////////////////////////////////////////////////////////////
/// Joystick object
///////////////////////////////////////////////////////////////

Joystick joystick(JOYSTICK_X_OUT_PIN, JOYSTICK_Y_OUT_PIN, JOYSTICK_SELECT_PIN);



///////////////////////////////////////////////////////////////
/// Real-Time Clock (RTC)
///
/// Real-time clock for tracking time between sleep cycles
/// The rtc keeps time even during sleep, so we only set it
/// once in setup(). To prevent setting it multiple times we
/// keep a flag in RTC SRAM, which only resets on power cycles
///////////////////////////////////////////////////////////////

ESP32Time rtc;
RTC_DATA_ATTR bool rtcIsSet = false;
RTC_DATA_ATTR time_t timeOfLastWatering;  

// set the values that we will write to the display
RTC_DATA_ATTR uint8_t rtcDataWateringFrequency = 15;    // watering time (seconds) (need to rename this so it sounds more important!)
RTC_DATA_ATTR uint8_t rtcDataWateringDuration  = 3;     // watering frequency (currently in seconds) (need to rename this so it sounds more important!)
RTC_DATA_ATTR WaterSetting waterSetting(rtcDataWateringFrequency, 
                                            rtcDataWateringDuration);  


///////////////////////////////////////////////////////////////
/// Deep-Sleep attributes
///
/// To preserve battery power, we put the ESP32 into deep sleep
/// when it has been idle for a certain amount of time.
///////////////////////////////////////////////////////////////

// struct for calculating sleep duration (TODO - delete this)
struct duration_t
{
  uint8_t hours_;
  uint8_t minutes_;
  uint8_t seconds_;

  uint8_t hoursToMinutes(uint8_t h) const
  {
    return h * 60;
  }

  uint8_t minutesToSeconds(uint8_t m) const
  {
    return m * 60;
  }

  uint64_t secondsToUsecs(uint8_t s) const
  {
    return s * 1e6;
  }

  uint64_t toUsecs() const
  {
    return secondsToUsecs(seconds_)
           + secondsToUsecs(minutesToSeconds(minutes_))
           + secondsToUsecs(minutesToSeconds(hoursToMinutes(hours_)));
  }

  uint64_t toSeconds() const
  {
    return seconds_ 
           + minutesToSeconds(minutes_)
           + minutesToSeconds(hoursToMinutes(hours_));
  }
};

const int SLEEP_TIME_HOURS   = 0;                      // hours of sleep
const int SLEEP_TIME_MINUTES = 0;                       // + minutes of sleep
const int SLEEP_TIME_SECONDS = 10;                       // + seconds of sleep
duration_t SleepDuration{SLEEP_TIME_HOURS, SLEEP_TIME_MINUTES, SLEEP_TIME_SECONDS};

const unsigned long SLEEP_AFTER_IDLE_MS = 5000;         // go to sleep if system is idle for this long
RTC_DATA_ATTR bool sleepEnteredOnPrevEventLoop = true;  // did we go to sleep on the previous iteration of loop()?



///////////////////////////////////////////////////////////////
/// Seven-Segment Hex Display
///////////////////////////////////////////////////////////////

Display segDisplay(DIGITS, SEGMENTS);

// timer interrupt for display
hw_timer_t *displayTimer = NULL;

volatile int displayInterruptDigitWrite = 0;
const int NUM_DIGITS = 4;

/// onDisplayTimer():
/// A timer-based interrupt for updating the display. The interrupt ensures 
/// that the joystick is read and display is written in a timely manner. On
/// each call to this interrupt we update and handle the joystick and display
/// state machines. We then clear and write a single digit to the display.
/// The interrupt timer fires fast enough so that, to mortal eyes, the display
/// appears to be showing all four digits at the same time.
void IRAM_ATTR onDisplayTimer()
{ 
  // poll joystick, update display settings if necessary
  const JoystickState_t state = joystick.handleJoystickStateMachine();
  segDisplay.handleDisplayStateMachine(joystick, waterSetting);

  // extract number we'd like to write from `segDisplay`
  // shift number down so that the digit we'd like to write
  // is the least significant digit of `numToWrite`
  // (e.g. if writing digit 1, shift number down by 2)
  int numShifts = displayInterruptDigitWrite;
  int numToWrite = segDisplay.getNumToWrite(); 
  for (int i = 0; i < numShifts; i++)
  {
    numToWrite /= 10;
  }
  numToWrite = numToWrite % 10;

  // clears the display then writes only the digit we are interested in
  segDisplay.writeDigit(numToWrite, displayInterruptDigitWrite);    

  // update the digit we'll write to for the next interrupt call
  displayInterruptDigitWrite = (displayInterruptDigitWrite + 1) % NUM_DIGITS;
  
}


///////////////////////////////////////////////////////////////
/// setup()
///////////////////////////////////////////////////////////////
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // print boot reason (ESP32 sometimes resets randomly, 
  // would be nice to know why)
  char bootReasonMessage[BOOT_REASON_MESSAGE_LEN];
  getBootReasonMessage(bootReasonMessage, BOOT_REASON_MESSAGE_LEN);
  Serial.println(bootReasonMessage);

  // configure relay pin. we set relay pin as an rtc 
  // gpio so we can control it during deep sleep
  rtc_gpio_init(RELAY_SIGNAL_PIN);       
  rtc_gpio_hold_dis(RELAY_SIGNAL_PIN);
  rtc_gpio_set_direction(RELAY_SIGNAL_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(RELAY_SIGNAL_PIN, 0);
  
  // enable timer interrupts
  displayTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(displayTimer, &onDisplayTimer, true);
  timerAlarmWrite(displayTimer, 500, true);
  timerAlarmEnable(displayTimer);
  
  // set real-time clock (only once)
  if (!rtcIsSet)
  {
    if(setRtc())
    {
      Serial.println("Error setting up RTC!");
      halt();
    }

  }

  const uint64_t sleepTimeUsecs = SleepDuration.toUsecs();
  Serial.print("Waking up uC every "); Serial.print(sleepTimeUsecs); Serial.print(" microseconds\n");

  // set timer as wakeup source
  if (esp_sleep_enable_timer_wakeup(sleepTimeUsecs) != ESP_OK)
  {
    Serial.println("Unable to set ESP32 sleep timer");
    halt();
  }

  // set joystick "press" as wakeup source (pin will short to ground when pressed)
  if (esp_sleep_enable_ext0_wakeup((gpio_num_t)JOYSTICK_SELECT_PIN, 0) != ESP_OK)
  {
    Serial.println("Unable to set GPIO wakeup");
    halt();
  }
}

///////////////////////////////////////////////////////////////
/// loop()
///////////////////////////////////////////////////////////////
void loop() {

  // get time since we last watered and compare to watering frequency
  struct tm currentTimeTmStruct = rtc.getTimeStruct();
  time_t currentTime = mktime(&currentTimeTmStruct);
  time_t timeSinceLastWatering = difftime(currentTime, timeOfLastWatering);
  time_t wateringFrequency = waterSetting.frequency; // * 24 * 60 * 60;   // days to seconds
  time_t timeUntilNextWatering = max(difftime(wateringFrequency, timeSinceLastWatering), 0.0);
  waterSetting.timeUntilNextWatering = timeUntilNextWatering;               // TODO: add accessor functions to more easily trace changes to this value
  
  // if we just woke up, print out wakeup reason and any other useful information
  if (sleepEnteredOnPrevEventLoop)
  {
    Serial.println();
    Serial.println("Woke up!");
    print_wakeup_reason();

    // re-enable display interrupts
    timerAlarmEnable(displayTimer);
    Serial.print("timeSinceLastWater | timeUntilNextWater | waterEvery: ");
    Serial.print(timeSinceLastWatering);    Serial.print("s | ");
    Serial.print(timeUntilNextWatering);    Serial.print("s | ");
    Serial.print(wateringFrequency);        Serial.println("s");
  }

  // toggle pump if it has been long enough since the last time we watered
  if (timeSinceLastWatering >= wateringFrequency)
  {
    Serial.print("Toggling pump relay for ");
    Serial.print(waterSetting.duration);
    Serial.println(" seconds");

    // disable display programming while we are watering
    segDisplay.enableProgramming(false);

    // blink all digits while watering
    segDisplay.enableBlinking(0b1111);
    
    // turn on pump (function blocks for `duration` seconds)
    togglePump(waterSetting.duration);

    // stop blinking display
    segDisplay.disableBlinking();
    
    // re-enable display programming
    segDisplay.enableProgramming(true);

    
    // reset time-related variables so we don't go to sleep immediately
    currentTimeTmStruct = rtc.getTimeStruct();
    currentTime = mktime(&currentTimeTmStruct);
    timeOfLastWatering = currentTime;
    timeSinceLastWatering = 0;    // update so that we don't go to sleep immediately after watering
  }


  // go to sleep if joystick has not been programmed and we
  // haven't watered in a while
  JoystickState_t currJoystickState = joystick.getState();
  if (currJoystickState == STATE_NORMAL 
      && joystick.getTimeSinceStateEnteredMs() > SLEEP_AFTER_IDLE_MS
      && timeSinceLastWatering * 1000 > SLEEP_AFTER_IDLE_MS)
  {
    // clean-up and print diagnostic information before sleeping
    prepareForSleep();     

    sleepEnteredOnPrevEventLoop = true;
    Serial.println("Going to sleep...");
    
    esp_deep_sleep_start();
  }
  else 
  {
    sleepEnteredOnPrevEventLoop = false;
  }

}

///////////////////////////////////////////////////////////////
/// prepareForSleep()
///
/// Housekeeping and useful print statements before going to sleep
///////////////////////////////////////////////////////////////
void prepareForSleep()
{

  // disable display timer interrupt 
  timerAlarmDisable(displayTimer);
  
  // clear the display
  segDisplay.clear();

  // save display settings in RTC memory
  rtcDataWateringFrequency = waterSetting.frequency;
  rtcDataWateringDuration  = waterSetting.duration;

  // latch the current value of the relay pin to low
  rtc_gpio_set_level(RELAY_SIGNAL_PIN, 0);
  rtc_gpio_hold_en(RELAY_SIGNAL_PIN);
  
  // get time since we last watered and use this to set 
  // the amount of time that we want to sleep
  struct tm currentTimeTmStruct = rtc.getTimeStruct();
  time_t currentTime = mktime(&currentTimeTmStruct);
  time_t timeSinceLastWatering = difftime(currentTime, timeOfLastWatering);
  time_t wateringFrequency = waterSetting.frequency; // * 24 * 60 * 60;   // (uncomment this when ready to wait days for watering)
  time_t timeUntilNextWatering = max(difftime(wateringFrequency, timeSinceLastWatering), 0.0);
  waterSetting.timeUntilNextWatering = timeUntilNextWatering;
  
  Serial.println("Preparing for sleep");
  Serial.print("timeSinceLastWater | timeUntilNextWater | waterEvery: ");
  Serial.print(timeSinceLastWatering);    Serial.print("s | ");
  Serial.print(timeUntilNextWatering);    Serial.print("s | ");
  Serial.print(wateringFrequency);        Serial.println("s");
  
  // adjust sleep time if next watering is going to happen soon
  if (timeUntilNextWatering < (time_t)SleepDuration.toSeconds())
  {
    uint64_t timeUntilNextWateringUSecs = timeUntilNextWatering * 1e6;

    Serial.print("Setting sleep time to ");
    Serial.print(timeUntilNextWateringUSecs);
    Serial.println(" usecs");
    if (esp_sleep_enable_timer_wakeup(timeUntilNextWateringUSecs) != ESP_OK)
    {
      Serial.println("Unable to set ESP32 sleep timer");
      halt();
    }
  }
  else
  {
    // otherwise just use the typical sleep duration
    const uint64_t sleepTimeUsecs = SleepDuration.toUsecs();
    Serial.print("Setting sleep time to ");
    Serial.print(sleepTimeUsecs);
    Serial.println(" usecs");
    if (esp_sleep_enable_timer_wakeup(sleepTimeUsecs) != ESP_OK)
    {
      Serial.println("Unable to set ESP32 sleep timer");
      halt();
    }
  }

  // flush the serial buffer so that everything gets printed out
  // before going to sleep
  Serial.flush();
}


///////////////////////////////////////////////////////////////
/// togglePump
///
/// Turns on pump for `duration` milliseconds, then turns it off.
///////////////////////////////////////////////////////////////
void togglePump(uint16_t duration)
{
  // hold last timestamp so we can check if millis() has wrapped around to 0
  static unsigned long prevMillis = 0;

  unsigned long start = millis();

  Serial.println("Turning pump on...");
  rtc_gpio_set_level(RELAY_SIGNAL_PIN, 1);

  // delay() seems to cause trouble, so use while-loop to spin instead
  while (true)
  {
    unsigned long timeNow = millis();

    // handle timer wrap-around
    if (timeNow < prevMillis)
    {
      prevMillis = timeNow;
      continue;
    }
    else if (timeNow - start > (unsigned long)duration * 1000)
    {
      break;
    }

    prevMillis = timeNow;

  }

  Serial.println("Turning pump off...");
  rtc_gpio_set_level(RELAY_SIGNAL_PIN, 0);
}

///////////////////////////////////////////////////////////////
/// print_wakeup_reason()
///
/// TODO: Delete this and just use getBootReasonMessage()
/// Query ESP32 to figure out why it woke up from sleep.
///////////////////////////////////////////////////////////////
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


///////////////////////////////////////////////////////////////
/// getBootReasonMessage()
///
/// Print reason for boot / wakeup from deep sleep.
///////////////////////////////////////////////////////////////
void getBootReasonMessage(char *buffer, int bufferlength)
{
  esp_reset_reason_t reset_reason = esp_reset_reason();

  bool resetReasonFound = true;
  switch (reset_reason)
  {
    case ESP_RST_UNKNOWN:
      snprintf(buffer, bufferlength, "Reset reason can not be determined");
      break;
    case ESP_RST_POWERON:
      snprintf(buffer, bufferlength, "Reset due to power-on event");
      break;
    case ESP_RST_EXT:
      snprintf(buffer, bufferlength, "Reset by external pin (not applicable for ESP32)");
      break;
    case ESP_RST_SW:
      snprintf(buffer, bufferlength, "Software reset via esp_restart");
      break;
    case ESP_RST_PANIC:
      snprintf(buffer, bufferlength, "Software reset due to exception/panic");
      break;
    case ESP_RST_INT_WDT:
      snprintf(buffer, bufferlength, "Reset (software or hardware) due to interrupt watchdog");
      break;
    case ESP_RST_TASK_WDT:
      snprintf(buffer, bufferlength, "Reset due to task watchdog");
      break;
    case ESP_RST_WDT:
      snprintf(buffer, bufferlength, "Reset due to other watchdogs");
      break;
    case ESP_RST_DEEPSLEEP:
      snprintf(buffer, bufferlength, "Reset after exiting deep sleep mode");
      break;
    case ESP_RST_BROWNOUT:
      snprintf(buffer, bufferlength, "Brownout reset (software or hardware)");
      break;
    case ESP_RST_SDIO:
      snprintf(buffer, bufferlength, "Reset over SDIO");
      break;
    default:
      reset_reason = ESP_RST_UNKNOWN;
      break;
  }


  if (reset_reason == ESP_RST_DEEPSLEEP)
  {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
      case ESP_SLEEP_WAKEUP_UNDEFINED:
        snprintf(buffer, bufferlength, "In case of deep sleep: reset was not caused by exit from deep sleep");
        break;
      case ESP_SLEEP_WAKEUP_ALL:
        snprintf(buffer, bufferlength, "Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source");
        break;
      case ESP_SLEEP_WAKEUP_EXT0:
        snprintf(buffer, bufferlength, "Wakeup caused by external signal using RTC_IO");
        break;
      case ESP_SLEEP_WAKEUP_EXT1:
        snprintf(buffer, bufferlength, "Wakeup caused by external signal using RTC_CNTL");
        break;
      case ESP_SLEEP_WAKEUP_TIMER:
        snprintf(buffer, bufferlength, "Wakeup caused by timer");
        break;
      case ESP_SLEEP_WAKEUP_TOUCHPAD:
        snprintf(buffer, bufferlength, "Wakeup caused by touchpad");
        break;
      case ESP_SLEEP_WAKEUP_ULP:
        snprintf(buffer, bufferlength, "Wakeup caused by ULP program");
        break;
      case ESP_SLEEP_WAKEUP_GPIO:
        snprintf(buffer, bufferlength, "Wakeup caused by GPIO (light sleep only)");
        break;
      case ESP_SLEEP_WAKEUP_UART:
        snprintf(buffer, bufferlength, "Wakeup caused by UART (light sleep only)");
        break;
    }
  }
}

///////////////////////////////////////////////////////////////
/// setRtc()
///
/// CALLED ONLY ONCE!!!
/// Sets real-time clock. Returns 0 on success.
///////////////////////////////////////////////////////////////
int setRtc()
{
  const char compile_date[] = __DATE__ " " __TIME__;
  Serial.print("Time at compilation: ");
  Serial.println(compile_date);

  // see https://cplusplus.com/reference/ctime/strftime/ 
  // for formatting information
  struct tm tm;
  if (strptime(compile_date, "%b %d %Y %H:%M:%S", &tm))
  {
    rtc.setTimeStruct(tm);
    timeOfLastWatering = mktime(&tm);
    rtcIsSet = true;
  }
  else
  {
    return -1;
  }

  return 0;
}

///////////////////////////////////////////////////////////////
/// halt()
///
/// Execution only reaches here if something bad happened
///////////////////////////////////////////////////////////////
void halt()
{
  Serial.printf("Halting...");
  while (true);
}
