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

const char ssname[] = "seesaw soil";
uint8_t count  = 0;

struct {
  Adafruit_seesaw *ss; // instance pointer
  uint16_t capacitance;
  float    temperature = NAN;
  uint8_t  address;
} soil_sensors[SEESAW_SOIL_MAX_SENSORS];

/********************************************************************************************/

void SEESAW_SOILDetect(void) {
  for (uint8_t i = 0; i < SEESAW_SOIL_MAX_SENSORS; i++) {
     if (!I2cSetDevice(SEESAW_SOIL_START_ADDRESS + i)) { continue; }

    if (!soil_sensors[count].ss) // don't have an object,
      soil_sensors[count].ss = new Adafruit_seesaw(); // allocate one
    if (soil_sensors[count].ss->begin(SEESAW_SOIL_START_ADDRESS + i)) {
      soil_sensors[count].address = SEESAW_SOIL_START_ADDRESS + i;
      I2cSetActiveFound(soil_sensors[count].address, ssname);
      count++;
    }
  }
  if (soil_sensors[count].address == 0)
    delete soil_sensors[count].ss; // used object for detection, didn't find anything so we don't need this object
}

void SEESAW_SOILEverySecond(void) {
  for (uint32_t i = 0; i < count; i++) {
    soil_sensors[i].temperature = ConvertTemp(soil_sensors[i].ss->getTemp());
    soil_sensors[i].capacitance = soil_sensors[i].ss->touchRead(0);
  }
}

void SEESAW_SOILShow(bool json) {
  for (uint32_t i = 0; i < count; i++) {
    char temperature[33];
    dtostrfd(soil_sensors[i].temperature, Settings.flag2.temperature_resolution, temperature);

    char sensor_name[22];
    strlcpy(sensor_name, ssname, sizeof(sensor_name));
    if (count > 1) {
      snprintf_P(sensor_name, sizeof(sensor_name), PSTR("%s%c%02X"), sensor_name, IndexSeparator(), soil_sensors[i].address); // SEESAW_SOIL-18, SEESAW_SOIL-1A  etc.
    }

    if (json) {
      ResponseAppend_P(JSON_SNS_TEMP, sensor_name, temperature);
      ResponseAppend_P(PSTR(",\"" D_JSON_MOISTURE "\":%u"),soil_sensors[i].capacitance);
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
      WSContentSend_PD(HTTP_SNS_MOISTURE, ssname, soil_sensors[i].capacitance);
      WSContentSend_PD(HTTP_SNS_TEMP, sensor_name, temperature, TempUnit());
#endif  // USE_WEBSERVER
    }
  }
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
