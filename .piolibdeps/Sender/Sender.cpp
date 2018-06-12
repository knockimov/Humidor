/*
 * Original work made by Samuel Lang, universam1 https://github.com/universam1/iSpindel.
 * Modiefied by Pierre Tunander, knockimov https://github.com/knockimov/Humidor
*/

#include "Sender.h"

#define UBISERVER "things.ubidots.com"
#define THINGSERVER "api.thingspeak.com"
#define CONNTIMEOUT 2000

SenderClass::SenderClass()
{
    _jsonVariant = _jsonBuffer.createObject();
}
void SenderClass::add(String id, float value)
{
    _jsonVariant[id] = value;
}
void SenderClass::add(String id, String value)
{
    _jsonVariant[id] = value;
}
void SenderClass::add(String id, uint32_t value)
{
    _jsonVariant[id] = value;
}
void SenderClass::add(String id, int32_t value)
{
    _jsonVariant[id] = value;
}

bool SenderClass::sendThingspeak(String token)
{
    _jsonVariant.printTo(Serial);

    if (_client.connect(THINGSERVER, 80))
    {
        Serial.println(F("Sender: Thingspeak posting"));

        String msg = F("GET /update?key=");
        msg += token;

        //Build up the data for the Prometheus Pushgateway
        //A gauge is a metric that represents a single numerical value that can arbitrarily go up and down.
        for (const auto &kv : _jsonVariant.as<JsonObject>())
        {
            msg += "&";
            msg += kv.key;
            msg += "=";
            msg += kv.value.as<String>();
        }

        msg += F(" HTTP/1.1\r\nHost: api.thingspeak.com: ");
        msg += F("\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
        msg += _jsonVariant.measureLength();
        msg += "\r\n";

        _client.println(msg);
        Serial.println(msg);

        _jsonVariant.printTo(_client);
        _client.println();
        Serial.println(msg);
    }
    else
    {
        Serial.println(F("\nERROR Sender: couldnt connect"));
    }

    int timeout = 0;
    while (!_client.available() && timeout < CONNTIMEOUT)
    {
        timeout++;
        delay(1);
    }
    while (_client.available())
    {
        char c = _client.read();
        Serial.write(c);
    }
    // currentValue = 0;
    _client.stop();
    delay(100); // allow gracefull session close
    return true;
}

bool SenderClass::sendUbidots(String token, String name)
{
    _jsonVariant.printTo(Serial);

    if (_client.connect(UBISERVER, 80))
    {
        Serial.println(F("Sender: Ubidots posting"));

        String msg = F("POST /api/v1.6/devices/");
        msg += name;
        msg += "?token=";
        msg += token;
        msg += F(" HTTP/1.1\r\nHost: things.ubidots.com\r\nUser-Agent: ");
        msg += "ESP8266";
        msg += F("\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
        msg += _jsonVariant.measureLength();
        msg += "\r\n";

        _client.println(msg);
        Serial.println(msg);

        _jsonVariant.printTo(_client);
        _client.println();
        Serial.println(msg);
    }
    else
    {
        Serial.println(F("\nERROR Sender: couldnt connect"));
    }

    int timeout = 0;
    while (!_client.available() && timeout < CONNTIMEOUT)
    {
        timeout++;
        delay(1);
    }
    while (_client.available())
    {
        char c = _client.read();
        Serial.write(c);
    }
    // currentValue = 0;
    _client.stop();
    delay(100); // allow gracefull session close
    return true;
}