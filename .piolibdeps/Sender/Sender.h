/*
 * Original work made by Samuel Lang, universam1 https://github.com/universam1/iSpindel.
 * Modiefied by Pierre Tunander, knockimov https://github.com/knockimov/Humidor
*/

#ifndef _SENDER_H_
#define _SENDER_H_

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

class SenderClass
{
public:
  SenderClass();
  bool sendThingspeak(String token);
  bool sendUbidots(String token, String name);
  void add(String id, float value);
  void add(String id, String value);
  void add(String id, int32_t value);
  void add(String id, uint32_t value);
  // ~SenderClass();

private:
  WiFiClient _client;
  // StaticJsonBuffer<200> _jsonBuffer;
  DynamicJsonBuffer _jsonBuffer;
  // JsonObject data;
  JsonVariant _jsonVariant;
};

#endif
