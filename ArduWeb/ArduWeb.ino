#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "Arducam_Mega.h"

WebServer server(80);
DNSServer dnsServer;
const char* ssid = "robsrob";
const char* password = "";
WiFiClient client;

#define READ_IMAGE_LENGTH           255
const int CS = 10;
Arducam_Mega myCAM(CS);
bool streamStart=false;
ArducamCamera* cameraInstance;

/*
Camera pin    Development board pins
VCC           3V3
GND           GND
SCK           12
MISO          13
MOSI          11
CS            10
    gpio.12 SCK Defined as SPI0_SCK
    gpio.11 MOSI Defined as SPI0_MOSI
    gpio.13 MISO Defined as SPI0_MISO
    gpio.10 CSO Defined as SPI_CS0
    */


/*
typedef enum {
    CAM_IMAGE_MODE_QQVGA  = 0x00,  //<160x120
    CAM_IMAGE_MODE_QVGA   = 0x01,  //<320x240
    CAM_IMAGE_MODE_VGA    = 0x02,  //<640x480
    CAM_IMAGE_MODE_SVGA   = 0x03,  //<800x600
    CAM_IMAGE_MODE_HD     = 0x04,  //<1280x720
    CAM_IMAGE_MODE_SXGAM  = 0x05,  //<1280x960
    CAM_IMAGE_MODE_UXGA   = 0x06,  //<1600x1200
    CAM_IMAGE_MODE_FHD    = 0x07,  //<1920x1080
    CAM_IMAGE_MODE_QXGA   = 0x08,  //<2048x1536
    CAM_IMAGE_MODE_WQXGA2 = 0x09,  //<2592x1944
    CAM_IMAGE_MODE_96X96  = 0x0a,  //<96x96
    CAM_IMAGE_MODE_128X128 = 0x0b, //<128x128
    CAM_IMAGE_MODE_320X320 = 0x0c, //<320x320
} CAM_IMAGE_MODE;
*/
CAM_IMAGE_MODE CameraMode=CAM_IMAGE_MODE_320X320;
unsigned long startMillis = 0;



const char* htmlPage = R"rawliteral(
  <script>
  function buttonClick()
  {
    fetch('/buttonstopstream')
    .then(response =>  response.text())
  }
  </script>

  <html><body>
  <h1>ESP8266 Camera Stream</h1>
  <img src='/stream' />
  <button onclick = "buttonClick()">Stop Stream</button>
  </body></html>

)rawliteral";

void loginInspecifictWLan(char* WLan_Name, char* WLan_Password) 
{
  if (WiFi.status() == WL_CONNECTED) 
  {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLan_Name, WLan_Password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter == 30) 
    {
      Serial.println("WiFi not connected!!!");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    //wir geben die IP adresse die uns das Wlan gegeben hat aus
    Serial.println(WiFi.localIP());
  }
}

void hotspot() 
{
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.softAP(ssid, password);

  dnsServer.start(53, "*", WiFi.softAPIP());

  Serial.println("WiFi created");
  Serial.println("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void handleRoot() 
{ 
  server.send(200, "text/html", htmlPage);
}

void handleNotFound() 
{
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleStream() 
{
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  Serial.println("Stream start");
  client = server.client();
  
  client.print("--frame\r\n");
  client.print("Content-Type: image/jpg\r\n\r\n");

  myCAM.takePicture(CameraMode,CAM_IMAGE_PIX_FMT_JPG);
  streamStart=true;
  startMillis=millis();
  cameraInstance = myCAM.getCameraInstance();
}

void readPicture()
{
  if(streamStart==false)
  {
    return;
  }

  uint32_t check =myCAM.getReceivedLength();
  uint8_t buff[READ_IMAGE_LENGTH];
  uint8_t rtLength = readBuff(cameraInstance, buff, READ_IMAGE_LENGTH);

  if(!client.connected())
  {    
    streamStart=false;
    Serial.println("Bild down");
    client.flush();
    client.print("\r\n");
    client.stop();
    return;
  }

  client.write(&buff[0], rtLength);
  client.flush();

  if (check == 0) 
  {
    //counter++;
    //Serial.print(counter);
    //ende und neu start
    
    client.flush();
    client.print("\r\n");

    Serial.print("ende Bild ");
    long elapsedMillis = millis() - startMillis;
    Serial.print(elapsedMillis);
    Serial.println("   neu start");
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpg\r\n\r\n");
    myCAM.takePicture(CameraMode,CAM_IMAGE_PIX_FMT_JPG);
    startMillis=millis();
  }
}

void setup() 
{
  Serial.begin(115200);
  Serial.println("Start");

  //loginInspecifictWLan("Vodafone-BE2C", "q49adKnc4bPka7bp");
  loginInspecifictWLan("Com2u.de.WLAN2","SOMMERREGEN05");
  hotspot();

  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  //server.on("/buttonstopstream", handleButtonStopStream);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("Camera start");
  myCAM.begin();
  Serial.println("Camera nun offen");

  myCAM.reset();
  Serial.println("Reset Camera");
}

void loop() 
{
  dnsServer.processNextRequest();
  server.handleClient();
  readPicture();
  delay(10);
}
