PRODUCT_ID(8702);
PRODUCT_VERSION(17);
SYSTEM_THREAD(ENABLED);
const char *version = "v1.0.1-17";
#include <Wire.h>
#include <math.h>
#include <SerLCD.h>
#include "SparkFun_SCD30_Arduino_Library.h" 
#include "PietteTech_DHT.h"
#include "constants.h"
#include "helpers.h"

const int MEASUREMENT_INTERVAL =30;
bool EXTERNAL_ANTENNAS_INSTALLED = true;
byte counter = 0;
byte contrast = 10;

SerLCD lcd; // Initialize the library with default I2C address 0x72
SCD30 airSensor;
// Use primary serial over USB interface for logging output
SerialLogHandler  logHandler(LOG_LEVEL_TRACE);

String deviceName;

//this is used to know when we have replaced the deviceId with the devicename we receive from the particle cloud
bool devicename_retrieved = false;
struct temp_humidity
{
  float humidity = 0;
  float temp = 0;
};
struct dht_values
{
  temp_humidity val;
  double dew = 0;
  double dewSlow = 0;
};
struct last_reading
{
  temp_humidity scd;
  dht_values dht;
  uint16_t co2 = 0;
} last_reading_struct;

//spark/device/diagnostics/update
//spark/device/last_reset
//spark/device/app-hash
//todo??
//spark/status
//spakr/flash/status
ApplicationWatchdog wd(60*1000,System.reset);
const char *connEvent = "connection";
//following format resonant / product name (rel)/description
time_t connection_watchdog;
PietteTech_DHT DHTB(D2, DHT22);

const char *notConnected = "Disconnected";
const char *connected = "Connected";
String deviceId;
struct eepromData_t {
  //version of the format of the eeprom data.
  //Increment this number if any change that is not backwards compatible occurs
  uint8_t version = 1;
  time_t calibration_started;
  bool first_calibration_completed = true;
  bool display_color_connection;
  //name of this device as reported on the publish
  //char name[20];
} eepromData;
extern bool deviceSpecificLoop();
//this will store how long has it been since we lost the wifi connection
unsigned long msSinceWifiLost = 0;

void handle_all_the_events(system_event_t event, int param)
{
    Log.info("got event %d with value %d", event, param);
}
void handle_wifi_events(system_event_t event, int param)
{
    if(param == network_status_disconnected 
    ||param == cloud_status_disconnected)
    {
      printInDisplay(kConnection, "Online: No",deviceName);
    }
    
    Log.info("wifi event %d with value %d", event, param);
}

//nifty function System.enterSafeMode();
void checkAutoCalibrationTime(bool forced)
{
  //dont start auto calibration if we already did it after 10 days
  if(Time.now() -  eepromData.calibration_started  > 60*60*24*10
  || forced)
  {
    //Set calibration true
    airSensor.setAutoSelfCalibration(true);
    eepromData.calibration_started = Time.now();
    saveEEPROM();
    String data = String::format("{\"calibrationStatus\": \"calibrating\"}");
    Particle.publish(SENSOR_EVENT,data,60,PRIVATE);
  }else
  {
    airSensor.setAutoSelfCalibration(false);
    String data = String::format("{ \"tags\" : {\"id\": \"%s\"},\"values\": {\"calibrationStatus\": \"notCalibrating\"}}", deviceId.c_str());
    Particle.publish(SENSOR_EVENT,data,60,PRIVATE);
  }
}
void saveEEPROM()
{
  EEPROM.put(0x0,eepromData);
}
int setFieldCal(String extra)
{
  if (extra == "")
  {
    return 1;
  }
  int factor = extra.toInt();
  boolean ret = airSensor.setForcedRecalibrationFactor(factor);
  Log.info("Forced Calibration ret: %d",ret);
  return !ret;
}
int setConfig(String extra)
{
  Log.info("Received setConfig value:%s",extra.c_str());
  if(extra.length() <= 0 
  || !(extra.equalsIgnoreCase("true") || extra.equalsIgnoreCase("false"))
  )
  {
    return 1;
  }
  bool isColorEnabled = extra.equalsIgnoreCase( "true");
  
  eepromData.display_color_connection  = isColorEnabled;
  saveEEPROM();
  return 0;
}
int setCal(String extra)
{
  if( !(extra.equalsIgnoreCase("true") || extra.equalsIgnoreCase("false")))
  {
    return  1;
  }
  eepromData.first_calibration_completed = (extra == "true");
  saveEEPROM();
  
  return 0;
}
void publishDeviceName()
{
  if(devicename_retrieved == true)
  {
    return;
  }
  bool tmp = Particle.publish(PARTICLE_DEVICE_EVENT_KEY);
  if(!tmp)
  {
    Log.error("publish devicename failed");
  }
}
time_t lastSensorReading = 0;
time_t lastScdReading = 0;
void setup()
{
  bool tmp = false;
  Wire.begin();
  deviceInit();
  Serial.begin(9600);
  Serial1.begin(9600);
  //This must be near the device init / network init area otherwise they won't work.
  bool success = Particle.function("setFieldCal",setFieldCal);
  Log.info("register setFieldCal %d",success);
  success = Particle.function("setCal",setCal);
  Log.info("register setCal %d",success);
  success = Particle.function("setConfig",setConfig);
  Log.info("register setConfig %d",success);
  deviceId = Particle.deviceID();
  //setup device id to devicename while we get the device event key published
  deviceName = deviceId;
  tmp = Particle.subscribe(PARTICLE_DEVICE_EVENT_KEY, handler);
  if(!tmp){Log.error("subscribe devicename failed");}
  publishDeviceName();
  Log.info("Resonant Environmental Logger Firmware:%s", version);
  Log.info("System version: %s", System.version().c_str());
 
  lcd.begin(Serial1); //Set up the LCD for Serial communication at 9600bps
  DHTB.begin();
  airSensor.begin(); 
  
  lcd.setBacklight(0, 255, 0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resonant Aero");
  lcd.setCursor(0,1);
  lcd.print(version);
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enviromental");
  lcd.setCursor(0,1);
  lcd.print("Logger");
  delay(3000);
  
  //!!read eeprom before using the data structure
  EEPROM.get(0x0,eepromData);
  eepromData.first_calibration_completed = true;
  //force calibration if we have never completed a calibration
  checkAutoCalibrationTime(!eepromData.first_calibration_completed);
  //Change number of seconds between measurements: 2 to 1800 (30 minutes)
  airSensor.setMeasurementInterval( 2);
  //Set altitude of the sensor in m
  airSensor.setAltitudeCompensation(564);
  //Current ambient pressure in mBar: 700 to 1200     
  airSensor.setAmbientPressure(1019.303);
  connection_watchdog = Time.now();
  lastSensorReading =  lastSensorReading = connection_watchdog;
  System.on(network_status+cloud_status, handle_wifi_events);
  System.on(all_events, handle_all_the_events);
}
int _sensorA_error_count = 0;
void printSensorData(class PietteTech_DHT *_d) 
{
  int result = _d->getStatus();

  switch (result) 
  {
  case DHTLIB_OK:
    Log.info("read DHT");
    break;
  case DHTLIB_ERROR_CHECKSUM:
    Log.error("Error\n\r\tChecksum error");
    break;
  case DHTLIB_ERROR_ISR_TIMEOUT:
    Log.error("Error\n\r\tISR time out error");
    break;
  case DHTLIB_ERROR_RESPONSE_TIMEOUT:
    Log.error("Error\n\r\tResponse time out error");
    break;
  case DHTLIB_ERROR_DATA_TIMEOUT:
    Log.error("Error\n\r\tData time out error");
    break;
  case DHTLIB_ERROR_ACQUIRING:
    Log.error("Error\n\r\tAcquiring");
    break;
  case DHTLIB_ERROR_DELTA:
    Log.error("Error\n\r\tDelta time to small");
    break;
  case DHTLIB_ERROR_NOTSTARTED:
    Log.error("Error\n\r\tNot started");
    break;
  default:
    Log.error("Unknown DHT error");
    break;
  }
  if(result == DHTLIB_OK)
  {
    last_reading_struct.dht.val.humidity = _d->getHumidity();
    last_reading_struct.dht.val.temp = _d->getFahrenheit();
    last_reading_struct.dht.dew = _d->getDewPoint();
    last_reading_struct.dht.dewSlow = _d->getDewPointSlow();
    
    Log.info("\tHumidity (%%): "+ String(last_reading_struct.dht.val.humidity)
    + "\tTemp F: "
    + String(last_reading_struct.dht.val.temp) 
    + "\tDew Point (oC): "
    + String(last_reading_struct.dht.dew)
    +"\tDew Point Slow (oC): "
    + String(last_reading_struct.dht.dewSlow));


      String data = String::format("{\"temp\": %f," \
                                      "\"humidity\": %f," \
                                      "\"dew\": %d," \
                                      "\"dewslow\": %d" \
                                      "}",
                                      last_reading_struct.dht.val.temp, 
                                      last_reading_struct.dht.val.humidity,
                                      last_reading_struct.dht.dew,
                                      last_reading_struct.dht.dewSlow);
      Particle.publish(DHT22_SENSOR_READING_EVENT_KEY, data,60*60,PRIVATE);
  }else
  {
      _sensorA_error_count++;
      Log.error("dht22 error %d", _sensorA_error_count);
      String data = String::format("{\"device-status\": \"dhterror\", \"errType\": %d,\"errCount\": %u}",
                              result,
                              _sensorA_error_count);
      Particle.publish(DEVICE_STATUS,data ,60*60,PRIVATE);
  }
}
 //variable to store truncated float to 2 decimal places
String tempString,humidityString;

bool dht_requested = false;

void loop()
{
    //TODO: this function should not stop sensor data from being displayed to client on lcd
    // if(deviceSpecificLoop())
    // {
    //   return;
    // }
    time_t current = Time.now();
    time_t time_since_dht = current - lastSensorReading;
    Log.info("last dht secs ago: %lu",  time_since_dht);
    //DHT time
    if(time_since_dht  > 30)
    { 
      if(!dht_requested)
      {
        int dht_error = DHTB.acquire();
        Log.warn("DHT Acquire: %d",dht_error);
        dht_requested = true;
      }
      //block while reading data
     // while(DHTB.acquiring()){};
      if (!DHTB.acquiring()) 
      {
        printSensorData(&DHTB);
        dht_requested = false;
        lastSensorReading = current;
      }
    }
    //scd
    time_t time_since_last_scd = current - lastScdReading;
    Log.info("time since reading: %lu",  time_since_last_scd);
    if( time_since_last_scd > MEASUREMENT_INTERVAL)
    { 
        if (airSensor.dataAvailable())
        {
            lastScdReading = current;
            //In Farenheit
            last_reading_struct.scd.temp = airSensor.getTemperature()*9/5 + 32;
            last_reading_struct.co2 = airSensor.getCO2();
            last_reading_struct.scd.humidity = airSensor.getHumidity();
            
            Log.info("scd temp(F): "+ String(last_reading_struct.scd.temp,2));
            Log.info("scd co2(ppm): %u", last_reading_struct.co2);   
            Log.info("scd humidity(%%): "+ String(last_reading_struct.scd.humidity,2));

            String data = String::format("{\"temp\": %f, \"co2\": %u, \"humidity\": %f}",
                                        last_reading_struct.scd.temp, 
                                        last_reading_struct.co2,
                                        last_reading_struct.scd.humidity);

            Particle.publish(SENSOR_READING_EVENT_KEY, data,60*60,PRIVATE);
        }
        else
        {
          if(time_since_last_scd> MEASUREMENT_INTERVAL)
          {
            Log.error("Sensor data not available");
            Particle.publish(DEVICE_STATUS, "{\"device-status\": \"no-sensor-data-available\"}",60,PRIVATE);
            printInDisplay(kOther, NOT_READY_STRING);
          }
        }
    }
    printInDisplay(kTemperature,String(last_reading_struct.dht.val.temp,2));
    printInDisplay(kTwoLine, 
                  "CO2(ppm):" + String(last_reading_struct.co2),
                  "Humidity: "+String(last_reading_struct.dht.val.humidity,2));
    /// Check connection status
    if(!isConnected()) 
    {
      wd.checkin();
      time_t time_offline = current - connection_watchdog ;
      if(time_offline > 60) 
      {
        Log.error("Resetting Due to connection timeout");
        delay(200);
        System.reset();
      }
      Log.warn("Offline for:%lu",time_offline);
      if(eepromData.display_color_connection)
      {
       lcd.setBacklight(255,0,0);
      }
      printInDisplay(kConnection, "Online: No",deviceName);
    }
    else 
    {
      connection_watchdog = current;
      lcd.setBacklight(0,255,0);
      publishDeviceName();
      printInDisplay(kConnection, "Online: Yes",deviceName);
    }

    //print in display and device specific print already have their delays internally
    #if (PLATFORM_ID == PLATFORM_ARGON) 
        printInDisplay(kTwoLine,WiFi.SSID() ,WiFi.localIP().toString());
    #endif
    deviceSpecificPrint();
  
}
void handler(const char *topic, const char *data) 
{  
  Log.info("handler called with topic:%s",topic);
  String dataStr = String(data);
  Log.info("received " + String(topic) + ": " + dataStr);
  if (dataStr.length() != 0) 
  {
    deviceName = dataStr;
    devicename_retrieved = true;
  }
}
