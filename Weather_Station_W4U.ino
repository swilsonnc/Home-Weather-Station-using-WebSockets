/* Home Weather Station
   Connect to a WiFi network and provide a web server on it so show Temperature, Humidity, Dew Point, Pressure, and Uptime as well as
   a forecast widget from weatherforyou.com.
   Refactored and added to for 4 readings by Scott Wilson 03-25-2017
   Arduino 1.6.12 on Windows 10 64-bit compiled: 03-25-2017 by Scott Wilson
   Sketch is running on a NodeMCU ESP-12E reading a BME280 sensor.
   Connect to http://ip:8890/update for webpage updater.
   Can be flashed OTA or http updater.
   Send all sensor readings to Thingspeak.com once every minute.
   History.html is hosted on another server.
   Webpage auto refreshes every 6 hours to update the forecast widget.  (websocket or ajax would be nice here as well)
   I am using this to display the page on a tablet 24/7.
   BME280 connection: VIN to 3v, GRD to grd, SCL to D1, SDA to D2. (not Adafruit BME280)
   Forward ports 8888 - 8890 to this ip in your router to access remotely.
   Can change look of webpage by modifying under HTTP START and WEBPAGE DESIGN
*/

#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <BME280I2C.h>
#include <FS.h>
#include <WebSocketsServer.h> // Version vom 20.05.2015 https://github.com/Links2004/arduinoWebSockets

// --- FUNCTION DECLARATION
char *getipstr(void);
void setupAP(void);

// --- CONSTANTS
const char* host = "esp8266-weather";
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "password";
const char *ssid = "-------your-ssid--------";
const char *password = "--------your-password------";

// Thingspeak stuff
char thingSpeakAddress[] = "api.thingspeak.com";
String APIKey = "YOUR-API-KEY";              //enter your channel's Write API Key
const int updateThingSpeakInterval = 60 * 1000;  // 1 minute interval at which to update ThingSpeak
// Variable Setup
long lastConnectionTime = 0;
boolean lastConnected = false;
WiFiClient client;
// End Thingspeak stuff

// BME280 stuff
BME280I2C bme;
bool metric = false;
String             temp_str;
String             humid_str;
String             pres_str;
String             dew_str;
// End BME280 stuff

ESP8266WebServer server (8888);

// HTTP Updater page
ESP8266WebServer httpServer(8890);
ESP8266HTTPUpdateServer httpUpdater;

WebSocketsServer webSocket = WebSocketsServer(8889);

uint32_t time_poll = 0;

//-------------------------------------------------HTTP START---------------------------------------------------------
const char HEAD_BEGIN[] PROGMEM = "\
<!DOCTYPE html>\r\n\
<html lang=\"en\">\r\n\
<head>\r\n\
<meta charset=\"utf-8\"/>\r\n\
<meta http-equiv=\"refresh\" content=\"21600\">\r\n\
";

const char WEBSOCKET_SCRIPT[] PROGMEM = "\
    <script language=\"javascript\" type=\"text/javascript\">\r\n\
        var connection = new WebSocket('ws://'+location.hostname+':8889/', ['arduino']);\r\n\\
        connection.onmessage = function (e) \{\r\n\\
          console.log('Server: ', e.data);\r\n\\
           if(e.data.slice(0,2)==\"1:\") document.getElementById('ActUptime').value = e.data.slice(2);\r\n\\
           if(e.data.slice(0,2)==\"2:\") document.getElementById('ActTemp').value = e.data.slice(2);\r\n\\
           if(e.data.slice(0,2)==\"3:\") document.getElementById('ActHum').value = e.data.slice(2);\r\n\\
           if(e.data.slice(0,2)==\"4:\") document.getElementById('ActDew').value = e.data.slice(2);\r\n\\
           if(e.data.slice(0,2)==\"5:\") document.getElementById('ActPres').value = e.data.slice(2);\r\n\\
        \};\r\n\\
        connection.onclose = function(){\r\n\\
          console.log('closed!');\r\n\\
          //reconnect now\r\n\\
          check();\r\n\\
        }\r\n\\
        function check(){\r\n\\
          if(!connection || connection.readyState == 3){;\r\n\\
            setInterval(check, 5000);\r\n\\
          }\r\n\\
        }\r\n\\
    </script>\r\n\\
    ";

const char STYLE[] PROGMEM = "\
        <style>\r\n\\
      body { background-color: #000000; font-family: Arial, Helvetica, Sans-Serif; Color: #ffffff; }\r\n\\
      a { text-decoration: none; color: #d3d3d3; }\r\n\\
    </style>\r\n\\
    ";

const char TITLE[] PROGMEM = "<title>[TITLE]</title>\r\n";
    
const char HEAD_END[] PROGMEM = "</head>\r\n";

//-----------------------------------------------------WEBPAGE DESIGN---------------------------------------------------
void http_root() {
  String sResponse;

  sResponse = FPSTR(HEAD_BEGIN);
  sResponse += FPSTR(WEBSOCKET_SCRIPT);
  sResponse.replace("[IP]", getipstr());
  sResponse += FPSTR(TITLE);
  sResponse.replace("[TITLE]", "Home Weather Station");
  sResponse += FPSTR(STYLE);
  sResponse += FPSTR(HEAD_END);
  sResponse += "\
  <body>\r\n\\
  <center>\r\n\\
  <table>\r\n\\
  <tbody>\r\n\\
  <tr>\r\n\\
  <td colspan=\"3\" align=\"center\">Temperature<br><font size=\"+5\"><output name=\"ActTemp\" id=\"ActTemp\"></output> &degF</font><p></td>\r\n\\
  </tr>\r\n\\
  <tr>\r\n\\
  <td align=\"left\">Humidity<br><font size=\"+2\"><output name=\"ActHum\" id=\"ActHum\"></output> &#37</font><p></td>\r\n\\
  <td align=\"center\">Dew Point<br><font size=\"+2\"><output name=\"ActDew\" id=\"ActDew\"></output> &degF</font><p></td>\r\n\\
  <td align=\"right\">Pressure<br><font size=\"+2\"><output name=\"ActPres\" id=\"ActPres\"></output> in</font><p></td>\r\n\\
  </tr>\r\n\\
  <tr>\r\n\\
  <td colspan=\"3\" align=\"center\">Uptime: <output name=\"ActUptime\" id=\"ActUptime\"></output><p></td>\r\n\\
  </tr>\r\n\\
  <tr width=\"250\"><td colspan=\"3\"><div style='width: 300px; overflow: auto;'>\r\n\\
  //------------------------Weatherforyou.com---Replace between this line and nect with your custom code--------------------------
  <a href=\"https://www.weatherforyou.com/weather/your+state/yourcity.html\" target=\"_new\">\r\n\\
  <img src=\"https://www.weatherforyou.net/fcgi-bin/hw3/hw3.cgi?config=png&forecast=zone&alt=hwizone7day5&place=city&state=state&country=us&hwvbg=black&hwvtc=white&hwvdisplay=&daysonly=2&maxdays=7\" width=\"500\" height=\"200\" border=\"0\" alt=\"City, State, weather forecast\"></a>\r\n\\
  //------------------------Stop paste here---------------------------------------------------------------------------------------
  </div></td>\r\n\\
  </tr>\r\n\\
  </tbody>\r\n\\
  </table>\r\n\\
  </body>\r\n\\
  </html>\r\n";

  server.send ( 200, "text/html", sResponse );

  Serial.println("Client disonnected");
}

//--------------------------------------------------------Websocket Event----------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      webSocket.disconnect(num);
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        //ForceSendValues = 1;
        webSocket.sendTXT(num, "Connected");
      }
      break;

    case WStype_TEXT:
      Serial.printf("[%u] got Text: %s\n", num, payload);
      break;

    case WStype_BIN:
      Serial.printf("[%u] got binary length: %u\n", num, lenght);
      hexdump(payload, lenght);
      break;

    default:
      Serial.println("webSocketEvent else");
  }

}

//#######################################################
//#######################################################
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send( 404, "text/plain", message );
}

//--------------------------------------------------------BME280-------------------------------------------------------
void bmeSample() {
  float temp(NAN), hum(NAN), pres(NAN);
  float dewPoint = bme.dew(metric);
  uint8_t pressureUnit(2);                                           // unit: B000 = Pa, B001 = hPa, B010 = Hg, B011 = atm, B100 = bar, B101 = torr, B110 = N/m^2, B111 = psi
  bme.read(pres, temp, hum, metric, pressureUnit);                   // Parameters: (float& pressure, float& temp, float& humidity, bool celsius = false, uint8_t pressureUnit = 0x0)
  /* Alternatives to ReadData():
    float temp(bool celsius = false);
    float pres(uint8_t unit = 0);
    float hum();

    Keep in mind the temperature is used for humidity and
    pressure calculations. So it is more effcient to read
    temperature, humidity and pressure all together.
  */
  temp_str = (temp);
  humid_str = (hum);
  pres_str = (pres);
  dew_str = (dewPoint);
  Serial.print("Temperature");Serial.println(": " + temp_str + "F");
  Serial.print("Humidity");Serial.println(": " + humid_str + "%");
  Serial.print("Dew Point");Serial.println(": " + dew_str + "F");
  Serial.print("Pressure");Serial.println(": " + pres_str + "in");
}

//-------------------------------------------------------Thingspeak----------------------------------------------
void updateThingSpeak(String tsData) {
  if (client.connect(thingSpeakAddress, 80)) {
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + APIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");
    client.print(tsData);
    lastConnectionTime = millis();

    if (client.connected()) {
      Serial.println("Connecting to ThingSpeak...");
      Serial.println();
    }
  }
}

void thingSpeak() {
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }
  // Disconnect from ThingSpeak
  if (!client.connected() && lastConnected) {
    Serial.println("...disconnected");
    Serial.println();
    client.stop();
  }
  // Update ThingSpeak
  if (!client.connected() && (millis() - lastConnectionTime > updateThingSpeakInterval)) {
    updateThingSpeak("field1=" + temp_str + "&field2=" + humid_str + "&field3=" + dew_str + "&field4=" + pres_str);
  }
  lastConnected = client.connected();
}

//------------------------------------------------------- SETUP -------------------------------------------------------
void setup ( void ) {
  Serial.begin(115200);

  Serial.println("Trying wifi config");
  WiFi.begin(ssid, password);
  uint8_t i=0;
  while ( WiFi.status() != WL_CONNECTED && i<10) {
    delay (500);
    Serial.print (".");
    i++;
  }

  Serial.println( "" );
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Own MAC: ");
  Serial.println(WiFi.macAddress());

  //START WEBSERVER UPGRADE
  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 8890);
  //Print the IP Address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.print(":8890");
  Serial.println("/update");

  // ------------------------------------------------------Arduino OTA------------------------------------------------------
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  SPIFFS.begin();
  // SPIFFS.format(); // only uncomment if you want to format SPIFFS.  After flashing run once and then comment back.
  
  //-------------------------------------------- SERVER HANDLES -------------------------------------------------------
  server.on("/", http_root);
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Socket server started");

  Serial.println(F("BME280 test"));

    if (!bme.begin()) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      delay(1000);
    } 
}

//------------------------------------------------------- LOOP -------------------------------------------------------
void loop ( void ) {
  server.handleClient();
  ArduinoOTA.handle();
  httpServer.handleClient();
  webSocket.loop();
  thingSpeak();
  bmeSample();
  
  if (time_poll <= millis()) {
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;
    int day = hr / 24;
    char Uptime[15];
    sprintf(Uptime, "1:%02dd:%02dh:%02dm:%02ds", day, hr % 24, min % 60, sec % 60);
    webSocket.broadcastTXT(Uptime);
    webSocket.broadcastTXT("2\:" + temp_str);
    webSocket.broadcastTXT("3\:" + humid_str);
    webSocket.broadcastTXT("4\:" + dew_str);
    webSocket.broadcastTXT("5\:" + pres_str);
    
    time_poll = millis() + 1000;
  }
}

char *getipstr(void){
  static char cOut[20];
  IPAddress ip = WiFi.localIP();                  // the IP address of your esp
  sprintf(cOut, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //also pseudocode.
  return cOut;
}

