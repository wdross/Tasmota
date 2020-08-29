/*
  xsns_76_seesaw_soil - I2C Capacitance & temperature sensor support for Tasmota

  Copyright (C) 2020  Wayne Ross and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef USE_I2C
#ifdef USE_SEESAW_SOIL
/*********************************************************************************************\
 * SEESAW_SOIL - Capacitance & Temperature Sensor
 *
 * I2C Address: 0x36, 0x37, 0x38, 0x39
 *
\*********************************************************************************************/

#define XSNS_76              76
#define XI2C_53              53  // See I2CDEVICES.md

#include "Adafruit_seesaw.h"

#define SEESAW_SOIL_MAX_SENSORS    4
#define SEESAW_SOIL_START_ADDRESS  0x36

const char ssname[] = "seesaw_soil"; // spaces not allowed for Homeassistant integration/mqtt topics
uint8_t count  = 0;

struct {
  Adafruit_seesaw *ss; // instance pointer
  float   moisture;
  float   temperature;
  uint8_t address;
} soil_sensors[SEESAW_SOIL_MAX_SENSORS];

// Used to convert capacitance into a moisture.
// From observation, an un-touched, try reading is in the low 330's
// Appears to be a 10-bit device, readings close to 1020
// So let's make a scale that converts those (aparent) facts into a percentage
#define MAX_CAPACITANCE 1023.0f
#define MIN_CAPACITANCE 330
#define CAP_TO_MOIST(c) ((max((int)(c),MIN_CAPACITANCE)-MIN_CAPACITANCE)/(MAX_CAPACITANCE-MIN_CAPACITANCE))

/********************************************************************************************/

void SEESAW_SOILDetect(void) {
  Adafruit_seesaw *SSptr=0;

  for (uint8_t i = 0; i < SEESAW_SOIL_MAX_SENSORS; i++) {
    int addr = SEESAW_SOIL_START_ADDRESS + i;
    if (!I2cSetDevice(addr)) { continue; }

    if (!SSptr) { // don't have an object,
      SSptr = new Adafruit_seesaw(); // allocate one
    }
    if (SSptr->begin(addr)) {
      soil_sensors[count].ss = SSptr; // save copy of pointer
      SSptr = 0; // mark that we took it
      soil_sensors[count].address = addr;
      soil_sensors[count].temperature = NAN;
      soil_sensors[count].moisture = NAN;
      I2cSetActiveFound(soil_sensors[count].address, ssname);
      count++;
    }
  }
  if (SSptr) {
    delete SSptr; // used object for detection, didn't find anything so we don't need this object
  }
}

void SEESAW_SOILEverySecond(void) {
  for (uint32_t i = 0; i < count; i++) {
    soil_sensors[i].temperature = ConvertTemp(soil_sensors[i].ss->getTemp());
    soil_sensors[i].moisture = CAP_TO_MOIST(soil_sensors[i].ss->touchRead(0)); // convert 10-bit value to percentage
  }
}

void SEESAW_SOILShow(bool json) {
  for (uint32_t i = 0; i < count; i++) {
    char temperature[33];
    dtostrfd(soil_sensors[i].temperature, Settings.flag2.temperature_resolution, temperature);

    char sensor_name[22];
    strlcpy(sensor_name, ssname, sizeof(sensor_name));
    if (count > 1) {
      snprintf_P(sensor_name, sizeof(sensor_name), PSTR("%s%c%2X"), sensor_name, IndexSeparator(), soil_sensors[i].address); // SEESAW_SOIL-18, SEESAW_SOIL-1A  etc.
    }

    if (json) {
      ResponseAppend_P(JSON_SNS_TEMP, sensor_name, temperature);
      ResponseAppend_P(JSON_SNS_MOISTURE, sensor_name, uint16_t(soil_sensors[i].moisture*100));
      if ((0 == tele_period) && (0 == i)) {
#ifdef USE_DOMOTICZ
        DomoticzSensor(DZ_TEMP, temperature);
#endif  // USE_DOMOTICZ
#ifdef USE_KNX
        KnxSensor(KNX_TEMPERATURE, soil_sensors[i].temperature);
#endif // USE_KNX
      }
#ifdef USE_WEBSERVER
    } else {
      WSContentSend_PD(HTTP_SNS_MOISTURE, sensor_name, uint16_t(soil_sensors[i].moisture*100)); // web page formats as integer (%d) percent
      WSContentSend_PD(HTTP_SNS_TEMP, sensor_name, temperature, TempUnit());
#endif  // USE_WEBSERVER
    }
  } // for each sensor connected
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns76(uint8_t function)
{
  if (!I2cEnabled(XI2C_53)) { return false; }
  bool result = false;

  if (FUNC_INIT == function) {
    SEESAW_SOILDetect();
  }
  else if (count){
    switch (function) {
      case FUNC_EVERY_SECOND:
        SEESAW_SOILEverySecond();
        break;
      case FUNC_JSON_APPEND:
        SEESAW_SOILShow(1);
        break;
  #ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        SEESAW_SOILShow(0);
        break;
  #endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_SEESAW_SOIL
#endif  // USE_I2C
