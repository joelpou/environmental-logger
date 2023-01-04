#ifndef RA_CONSTANTS_H_
#define RA_CONSTANTS_H_
#include "application.h"
#define SENSOR_READING_EVENT_KEY "resonant/rel/measurement-2"
#define DHT22_SENSOR_READING_EVENT_KEY "resonant/rel/measurement-dht"
#define SENSOR_EVENT "resonant/rel/sensor/calibration"
#define SIGNAL_EVENT "resonant/rel/wifi/signal"
#define DEVICE_STATUS "resonant/rel/device-status"

#define PARTICLE_DEVICE_EVENT_KEY "particle/device/name"

enum DisplayMessageType
{
  kHumidity =0,
  kTemperature,
  
  kConnection,
  kSignal,
  kTwoLine,
  kOther //for everything that does not has a header inside the print function
};
#define NOT_READY_STRING "No sensor data"
void printInDisplay(DisplayMessageType event, String value);
void printInDisplay(DisplayMessageType event, String value,String key) ;

void setupExternalAntenna();

#endif