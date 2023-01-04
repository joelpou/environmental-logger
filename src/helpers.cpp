#include "helpers.h"
#include "constants.h"
#include <SerLCD.h>
#include "application.h"
extern SerLCD lcd;
extern String deviceName;
void printInDisplay(DisplayMessageType event, String value,String key) 
{
  Log.info("called printInDisplay: secondline:%s firstline:%s",value.c_str(),key.c_str());
  lcd.clear();
  lcd.setCursor(0, 0);

  if (event == kTemperature) {
    lcd.print("Temperature(F");
    lcd.print((char)223);
    lcd.print("):");
  }
  else if (event == kHumidity) {
    lcd.print("Humidity(%):");
  }
  else if (event == kConnection 
  || event == kSignal 
  || event == kTwoLine) 
  {
    lcd.print(key);
  }
  else {
    lcd.print(NOT_READY_STRING);
  }

  lcd.setCursor(0, 1);
  lcd.print(value);
  delay(2500);
}

void printInDisplay(DisplayMessageType event, String value)
{
  printInDisplay(event,value,"");
}