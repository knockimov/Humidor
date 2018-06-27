#include <FS.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h> //https://github.com/kentaylor/WiFiManager

#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <SSD1306.h>
#include <Sender.h>

// Pin configuration values
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.
const int TRIGGER_PIN = 12;
const int FANPIN = 16;
const int BUTTON = 14;
const int DHTPIN = 13;

// WiFi configuration values
char token[60] = "";
char name[60] = "Humidor001";
uint32_t update = 120;
unsigned int maxhum = 0;
unsigned int offset = 0;
bool ubidots = true;
bool thingspeak = true;

// Timers configuration values
unsigned long oledTimer = 0;            // will store last time the OLED was turned on
unsigned long previousMillisDHT = 0;    // will store last time the DHT was updated
unsigned long previousMillisUpdate = 0; // will store last time the API was updated
const long intervalDHT = 2000;          // interval at which to update DHT-sensor

// Misc configuration values
const char *CONFIG_FILE = "/config.json";
float HUMIDITY = 0, TEMPERATURE = 0;
#define FIRMWAREVERSION "1.2.1"
#define displayOnTime 15000 // ms, display ontime when button is pressed

// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;

bool readConfigFile();
bool writeConfigFile();

DHT dht(DHTPIN, DHT22, 60);
SSD1306 display(0x3C, 4, 5);

void formatSpiffs()
{
  Serial.println(F("\nNeed to format SPIFFS: "));
  SPIFFS.end();
  SPIFFS.begin();
  Serial.println(SPIFFS.format());
}

void StartWifiConfig()
{
  if ((digitalRead(TRIGGER_PIN) == LOW) || (initialConfig))   // is configuration portal requested?
  {
    Serial.println("Configuration portal requested");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.

    // Extra parameters to be configured
    // After connecting, parameter.getValue() will get you the configured value
    // Format: <ID> <Placeholder text> <default value> <length> <custom HTML> <label placement>

    // API Key - this is a straight forward string parameter
    WiFiManagerParameter p_token("token", "API Key", token, 60);
    WiFiManagerParameter p_name("name", "Device Name", name, 60);
    WiFiManagerParameter p_update("update", "Update Intervall (s)", String(update).c_str(), 8, TYPE_NUMBER);

    // API - bool parameter visualized using checkbox, so couple of things to note
    // - value is always 'UB/TS' for true. When the HTML form is submitted this is the value that will be
    //   sent as a parameter. When unchecked, nothing will be sent by the HTML standard.
    // - customhtml must be 'type="checkbox"' for obvious reasons. When the default is checked
    //   append 'checked' too
    // - labelplacement parameter is WFM_LABEL_AFTER for checkboxes as label has to be placed after the input field

    char customhtml[24] = "type=\"checkbox\"";
    if (ubidots)
    {
      strcat(customhtml, " checked");
    }
    if (thingspeak)
    {
      strcat(customhtml, " checked");
    }
    WiFiManagerParameter p_ubidots("ubidots", "Ubidots", "UB", 3, customhtml, WFM_LABEL_AFTER);
    WiFiManagerParameter p_thingspeak("thingspeak", "Thingspeak", "TS", 3, customhtml, WFM_LABEL_AFTER);

    // maxhum and offset parameters are integers so we need to convert them to char array but
    // no other special considerations
    char convertedValue[3];
    sprintf(convertedValue, "%d", maxhum);
    WiFiManagerParameter p_maxhum("maxhum", "Maximum Humidity", convertedValue, 3);
    sprintf(convertedValue, "%d", offset);
    WiFiManagerParameter p_offset("offset", "Humidity Offset", convertedValue, 3);

    // Just a quick hint
    WiFiManagerParameter p_hint("<small>*Hint: if you want to reuse the currently active WiFi credentials, leave SSID and Password fields empty</small>");

    WiFiManager wifiManager; // Initialize WiFIManager
    //wifiManager.resetSettings();    //reset settings - for testing

    //add all parameters here
    wifiManager.addParameter(&p_hint);
    wifiManager.addParameter(&p_token);
    wifiManager.addParameter(&p_name);

    wifiManager.addParameter(&p_ubidots);
    wifiManager.addParameter(&p_thingspeak);

    wifiManager.addParameter(&p_update);
    wifiManager.addParameter(&p_maxhum);
    wifiManager.addParameter(&p_offset);

    // Sets timeout in seconds until configuration portal gets turned off.
    // If not specified device will remain in configuration mode until
    // switched off via webserver or device is restarted.
    wifiManager.setConfigPortalTimeout(300);

    if (!wifiManager.startConfigPortal("HumidorAP"))
    {
      //if (!wifiManager.autoConnect("HumidorAP","partagas")) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    }
    else
    {
      Serial.println("Connected...yeey :)");
    }

    // Getting posted form values and overriding local variables parameters
    // Config file is written regardless the connection state
    strcpy(token, p_token.getValue());
    strcpy(name, p_name.getValue());
    update = String(p_update.getValue()).toInt();
    ubidots = (strncmp(p_ubidots.getValue(), "UB", 1) == 0);
    thingspeak = (strncmp(p_thingspeak.getValue(), "TS", 1) == 0);
    maxhum = atoi(p_maxhum.getValue());
    offset = atoi(p_offset.getValue());

    writeConfigFile();           // Writing JSON config file to flash for next boot
    digitalWrite(PIN_LED, HIGH); // Turn LED off as we are not in configuration mode.
    ESP.reset();
    delay(5000);
  }
}

bool readConfigFile()
{
  File f = SPIFFS.open(CONFIG_FILE, "r"); // this opens the config file in read-mode

  if (!f)
  {
    Serial.println("Configuration file not found");
    return false;
  }
  else
  {
    size_t size = f.size();                               // we could open the file
    std::unique_ptr<char[]> buf(new char[size]);          // Allocate a buffer to store contents of the file.
    f.readBytes(buf.get(), size);                         // Read and store file contents in buf
    f.close();                                            // Closing file
    DynamicJsonBuffer jsonBuffer;                         // Using dynamic JSON buffer which is not the recommended memory model, but anyway
    JsonObject &json = jsonBuffer.parseObject(buf.get()); // Parse JSON string

    if (!json.success())
    { // Test if parsing succeeds.
      Serial.println("JSON parseObject() failed");
      return false;
    }
    json.printTo(Serial);

    // Parse all config file parameters, override local config variables with parsed values
    if (json.containsKey("token"))
    {
      strcpy(token, json["token"]);
    }
    if (json.containsKey("name"))
    {
      strcpy(name, json["name"]);
    }
    if (json.containsKey("ubidots"))
    {
      ubidots = json["ubidots"];
    }
    if (json.containsKey("thingspeak"))
    {
      thingspeak = json["thingspeak"];
    }
    if (json.containsKey("update"))
    {
      maxhum = json["update"];
    }
    if (json.containsKey("maxhum"))
    {
      maxhum = json["maxhum"];
    }
    if (json.containsKey("offset"))
    {
      offset = json["offset"];
    }
  }
  Serial.println("\nConfig file was successfully parsed");
  return true;
}

bool writeConfigFile()
{
  Serial.println("Saving config file");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  // JSONify local configuration parameters
  json["token"] = token;
  json["name"] = name;
  json["ubidots"] = ubidots;
  json["thingspeak"] = thingspeak;
  json["update"] = update;
  json["maxhum"] = maxhum;
  json["offset"] = offset;

  File f = SPIFFS.open(CONFIG_FILE, "w"); // Open file for writing
  if (!f)
  {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.prettyPrintTo(Serial);
  json.printTo(f); // Write data to file and close it
  f.close();

  Serial.println("\nConfig file was successfully saved");
  return true;
}

bool uploadData(uint8_t service)
{
  SenderClass sender;

  if (service == ubidots)
  {
    sender.add("humidity", HUMIDITY);
    sender.add("temperature", TEMPERATURE);
    sender.add("interval", update);
    sender.add("RSSI", WiFi.RSSI());
    Serial.println(F("\ncalling Ubidots"));
    return sender.sendUbidots(token, name);
  }

  if (service == thingspeak)
  {
    sender.add("field2", HUMIDITY);
    sender.add("field1", TEMPERATURE);
    Serial.println(F("\ncalling Thingspeak"));
    return sender.sendThingspeak(token);
  }
}

void updateDHT()
{
  unsigned long currentMillisDHT = millis();
  if (currentMillisDHT - previousMillisDHT >= intervalDHT)
  {
    previousMillisDHT = currentMillisDHT;
    TEMPERATURE = dht.readTemperature();
    HUMIDITY = dht.readHumidity() + offset;

    if (isnan(HUMIDITY) || isnan(TEMPERATURE))
    {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    Serial.print("Humidity: ");
    Serial.print(HUMIDITY);
    Serial.println(" %\t");

    Serial.print("Temperature: ");
    Serial.print(TEMPERATURE);
    Serial.println(" *C");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n Starting");

  //SPIFFS.format();    //clean FS, for testing

  pinMode(PIN_LED, OUTPUT);           // Initialize the LED digital pin as an output.
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Initialize trigger pin
  pinMode(FANPIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  digitalWrite(BUTTON, HIGH);
  dht.begin();
  display.init();

  bool result = SPIFFS.begin(); // Mount the filesystem
  Serial.println("SPIFFS opened: " + result);

  if (!readConfigFile())
  {
    Serial.println("Failed to read configuration file, using default values");
  }

  WiFi.printDiag(Serial); //Remove this line if you do not want to see WiFi password printed

  if (WiFi.SSID() == "")
  {
    Serial.println("We haven't got any access point credentials, so get them now");
    initialConfig = true;
  }
  else
  {
    digitalWrite(PIN_LED, HIGH); // Turn LED off as we are not in configuration mode.
    WiFi.mode(WIFI_STA);         // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    unsigned long startedAt = millis();
    Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis() - startedAt);
    Serial.print(waited / 1000);
    Serial.print(" secs in setup() connection result is ");
    Serial.println(connRes);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect, finishing setup anyway");
  }
  else
  {
    Serial.print("Local ip: ");
    Serial.println(WiFi.localIP());
  }
}

void loop()
{
  StartWifiConfig();

  updateDHT();

  unsigned long currentMillisUpdate = millis();
  if (currentMillisUpdate - previousMillisUpdate >= (update * 1000))
  {
    previousMillisUpdate = currentMillisUpdate;
    if (ubidots == 1)
    {
      uploadData(ubidots); // UPLOADING TO UBIDOTS API
      Serial.print("----------------- Updated ");
      Serial.print("Ubidots");
      Serial.println(" -----------------");
    }
    else if (thingspeak == 1)
    {
      uploadData(thingspeak); // UPLOADING TO THINGSPEAK API
      Serial.print("----------------- Updated ");
      Serial.print("Thingspeak");
      Serial.println(" -----------------");
    }
    else
    {
      Serial.println("FAILED TO UPLOAD TO API");
      Serial.println("Please run setup again");

      display.displayOn();
      oledTimer = millis();
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "UPLOAD DATA FAILED!!");
      display.drawString(0, 15, "Please run setup again");

      display.display();
    }
  }

  if (HUMIDITY > 0 && HUMIDITY < maxhum)
  {
    digitalWrite(FANPIN, HIGH);
  }
  else
  {
    digitalWrite(FANPIN, LOW);
  }

  if (digitalRead(BUTTON) == LOW)
  {
    char tempBuffer[8];
    char humBuffer[8];
    dtostrf(TEMPERATURE, 7, 2, tempBuffer);
    dtostrf(HUMIDITY, 7, 2, humBuffer);
    oledTimer = millis();
    Serial.println("Button Short Pressed");
    Serial.println("");
    Serial.print("Humidity buffer: ");
    Serial.println(humBuffer);
    Serial.print("Temperature buffer: ");
    Serial.println(tempBuffer);
    display.displayOn();
    display.clear();
    display.setFont(Dialog_plain_10);
    display.drawString(10, 0, "TEMP");
    display.drawString(80, 0, "FUKT");
    display.setFont(Dialog_plain_16);
    display.drawString(0, 15, tempBuffer);
    display.drawString(70, 15, humBuffer);
    display.setFont(Dialog_plain_10);
    display.drawString(10, 42, "Version: ");
    display.drawString(80, 42, FIRMWAREVERSION);
    display.display();
  }

  if (millis() - oledTimer >= displayOnTime)
  {
    display.displayOff();
  }
}