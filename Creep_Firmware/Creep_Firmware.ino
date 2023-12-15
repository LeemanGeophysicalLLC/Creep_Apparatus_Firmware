/*
 * Creep Apparatus
 * Leeman Geophysical LLC
 * 
 * Firmware to produce readings from the ADC on the creep apparatus and allow
 * users to set the calibration, baud rate, and timed output.
 */

#include <Wire.h>
#include <EEPROM.h>
#include "Adafruit_ADS1X15.h"
#include "Cmd.h"

// EEPROM
const uint8_t EEPROM_BASE_ADDRESS = 0;

// Firmware Version
const uint8_t FIRMWARE_MAJOR_VERSION = 1;
const uint8_t FIRMWARE_MINOR_VERSION = 1;

// Structure to hold semi-permanent settings
struct PersistentData
{
  uint8_t system_first_boot;
  float calibration;
  uint32_t baud_rate;
  uint8_t firmware_major_version;
  uint8_t firmware_minor_version;
  uint32_t timed_output;
};

// ADC object
Adafruit_ADS1115 ads;

// Create insance to hold settings
PersistentData device_settings;


void SetDefaults()
{
  /*
   * Set the default values in the EEPROM
   */
  PersistentData defaults = {
    45,
    1,
    115200,
    FIRMWARE_MAJOR_VERSION,
    FIRMWARE_MINOR_VERSION,
    1000
  };
  EEPROM.put(EEPROM_BASE_ADDRESS, defaults);
  EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);
  Serial.println(F("Default settings restored."));
}


uint8_t CheckFirstBoot()
{
  /*
   * Checks if this is the devices first boot of if the EEPROM settings
   * appear to be incorrect. Checks for the first boot value and the 
   * firmware version to see if they make sense.
   */
  char first_boot = 0;
  if ((device_settings.system_first_boot != 45) ||
      (device_settings.firmware_major_version != FIRMWARE_MAJOR_VERSION) ||
      (device_settings.firmware_minor_version != FIRMWARE_MINOR_VERSION)
     )
  {
    first_boot = 1;
  }
 return first_boot;
}


void SetCalibration(int arg_cnt, char **args)
{
  /*
   * Set the calibration of the device to a provided engineering unit/counts.
   */
   if (arg_cnt > 0 )
  {
    float cal = cmdStr2Num(args[1], 10);
    cal += cmdStr2Num(args[2], 10) / 10;
    device_settings.calibration = atof(args[1]);
    EEPROM.put(EEPROM_BASE_ADDRESS, device_settings);
    EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);
  }
 EEPROM.put(EEPROM_BASE_ADDRESS, device_settings);
 EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);
}


void SetTimed(int arg_cnt, char **args)
{
  /*
   * Set the timed output mode in ms.
   */
  if (arg_cnt > 0)
  {
    device_settings.timed_output = cmdStr2Num(args[1], 10);
    EEPROM.put(EEPROM_BASE_ADDRESS, device_settings);
    EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);
  }
}


void SetBaud(int arg_cnt, char **args)
{
  /*
   * Set the baud rate.
   */
  if (arg_cnt > 0 )
  {
    uint32_t proposed_rate = cmdStr2Num(args[1], 10);
  
    // Make sure the baud rate is valid
    if ((proposed_rate == 1200) || (proposed_rate == 2400) ||
        (proposed_rate == 4800) || (proposed_rate == 9600) ||
        (proposed_rate == 19200) || (proposed_rate == 38400) ||
        (proposed_rate == 57600) || (proposed_rate == 74880) ||
        (proposed_rate == 115200))
        {
    
          // Set the device baud rate
          device_settings.baud_rate = proposed_rate;
          EEPROM.put(EEPROM_BASE_ADDRESS, device_settings);
          EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);
          Serial.begin(device_settings.baud_rate);  // Restart with new rate
        }
    else
    {
      Serial.println(F("Invalid Baud Rate!"));
    }
  }
}


float GetCalibration()
{
  /*
   * Get the calibration in mm/mV
   */
  return device_settings.calibration;
}


void PrintCalibration()
{
  /*
   * Show the calibration via serial
   */
  Serial.println(GetCalibration(), 6);
}

uint32_t GetBaud()
{
  /*
   * Get the baud rate
   */
  return device_settings.baud_rate;
}


void PrintBaud()
{
  /*
   * Show the baud rate via serial
   */
  Serial.println(GetBaud());
}


void Help(int arg_cnt, char **args)
{
  /*
   * Show some basic help information
   */
  Serial.println(F("Enter commands followed by newline character"));
  Serial.println(F("SETCAL XX.XX - Set the calibration in mm/mV"));
  Serial.println(F("SETBAUD XXXXX - Set the baud rate to any standard rate between 1200-115200"));
  Serial.println(F("TIMED XXXX - Send data automatically every XXXX milliseconds"));
  Serial.println(F("HELP - Show this screen"));
  Serial.println(F("SHOW - Show current settings and information"));
  Serial.println(F("DEFAULTS - Reset to factory defaults"));
}


void Show(int arg_cnt, char **args)
{
  /*
   * Show the current settings
   */
  Serial.println(F("Creep Apparatus Settings"));
  Serial.println(F("--------------------------"));
  Serial.print(F("System Boot Value: "));
  Serial.println(device_settings.system_first_boot);
  Serial.print(F("Calibration mm/mV: "));
  PrintCalibration();
  Serial.print(F("Baud Rate: "));
  PrintBaud();
  Serial.print(F("Firmware Version: "));
  Serial.print(device_settings.firmware_major_version);
  Serial.print(F("."));
  Serial.println(device_settings.firmware_minor_version);
  Serial.print(F("Timed output (ms): "));
  Serial.println(device_settings.timed_output);
}

void setup()
{
  // Reads the settings from the EEPROM into our structure
  EEPROM.get(EEPROM_BASE_ADDRESS, device_settings);

  // Check and see if it's valid or if we need to restore defaults
  if (CheckFirstBoot() == 1)
  {
    Serial.println(F("Restoring Defaults!"));
    SetDefaults();
  }

  // Setup the command structure
  cmdInit(&Serial);
  cmdAdd("SETCAL", SetCalibration);
  cmdAdd("SETBAUD", SetBaud);
  cmdAdd("HELP", Help);
  cmdAdd("SHOW", Show);
  cmdAdd("DEFAULTS", SetDefaults);
  cmdAdd("TIMED", SetTimed);
  
  Serial.begin(device_settings.baud_rate);
  
  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();
}

void loop()
{  
  static uint32_t last_serial_time_ms = 0;

  if ((millis() - last_serial_time_ms + 1) > device_settings.timed_output)
  {
    last_serial_time_ms = millis();
    int16_t voltage = ads.readADC_Differential_0_1() * 0.1875F;
    float displacement = voltage * GetCalibration();
    Serial.print(last_serial_time_ms);
    Serial.print(",");
    Serial.println(displacement);
  }
  
  // Check for serial commands and handle them
  cmdPoll();
}
