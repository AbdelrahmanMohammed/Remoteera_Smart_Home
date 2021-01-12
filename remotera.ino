/*
static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;
static const uint8_t D9   = 3;
static const uint8_t D10  = 1;
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

//network data
WiFiServer server(80);
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

//system data
const unsigned short relayCount = 6; //on board relays
unsigned int relayStates[relayCount] = { 0,0,0,0,0,0 };
unsigned short relayPin[relayCount] = { D1, D2, D8, D7, D6, D5 };

const unsigned short buttonCount = 4; //switches
bool buttonStates[buttonCount] = { 0,0,0,0 };
unsigned short buttonPin[buttonCount] = { D3, D4, D9, D10 };//should set the two jumper to s

unsigned short timerCount = 0;
unsigned short minCount = 0;
volatile bool secondTick = false;

void ICACHE_RAM_ATTR onTimerISR(){ 
  secondTick = true;
  timer1_write(312499);
}

void setup() {
  system_boot();  
  system_init();
}

void loop(){
  while(1)
  {
    WiFiClient client = server.available();
    MDNS.update();
    if (client) {
          unsigned int relay,delayTime;
          String c,ssid,password;
          String req = client.readStringUntil('\r');
          if(!parseCommand(req,c,ssid,password,relay,delayTime))
            {
              //Serial.println("not remotera command");
              continue;
            }
          //Serial.println("flush buffer");
          client.flush(); 
          commandRoutine(client,c,ssid,password,relay,delayTime);
          client.stop();
    }
    buttonRoutine();
    TickRoutine();
  }
}
///////////////////////////////////////////////////////////////////
void buttonRoutine()
{
  for(int i=0; i< buttonCount; i++){
    bool newState = digitalRead(buttonPin[i]);
    if(newState != buttonStates[i])   
    {
      buttonStates[i] = newState;
      //toggle
      if(relayStates[i])
        {
          relayStates[i] = 0;
          digitalWrite(relayPin[i] ,LOW);
        }
        else
        {
          relayStates[i] = 1;
          digitalWrite(relayPin[i] ,HIGH);
        }
    } 
  }
}
/////////////////////////////////////////////////////////////////
void commandRoutine(WiFiClient &client,
                        String &c, 
                        String &ssid,
                        String &password,
                        unsigned int &relay,
                        unsigned int &delayTime)
{
  if(c.equals("s"))
            {
              String clientMessage="";
              //send states
              for(int i=0; i<relayCount; i++){
                clientMessage += String(relayStates[i]);
                if(i < relayCount-1)
                  clientMessage += ".";
              }
              sendResponse(client,clientMessage);
            }    
          else if(c.equals("r"))
            {
              if(delayTime)digitalWrite(relayPin[relay] ,HIGH);
              else digitalWrite(relayPin[relay] ,LOW);
                relayStates[relay] = delayTime;
                sendResponse(client,"rec"); 
            }
          else if(c.equals("t"))
            {
              //toggel all
              for(int i=0; i< relayCount; i++){
                if(delayTime)digitalWrite(relayPin[i], HIGH );
                else digitalWrite(relayPin[i], LOW );
                relayStates[i] = delayTime;
                delay(50);
              }
              sendResponse(client,"rec"); 
            }
            else if(c.equals("n"))
            {
              //setting ssid and password
              writeStringToEEPROM((relayCount*2), ssid);
              writeStringToEEPROM((relayCount*2)+ssid.length()+1, password);
              sendResponse(client,"rec");
            }  
            else if(c.equals("o"))
            {
              //restart
              delay(2000);
              ESP.reset();
            }
}
///////////////////////////////////////////////////////////////
void TickRoutine()
{
  if(secondTick)
  {
    secondTick = false;
    //decrement & save states every min
    if(timerCount>58)
    {
      for(int i=0; i< relayCount; i+=2){
        if(relayStates[i]>2){
          relayStates[i]--;
        }else if(relayStates[i]==2){
          relayStates[i]=0;
        }
      }
      if(minCount>19){
        for(int i=0; i< relayCount; i++){
          EEPROM.write((i*2),relayStates[i]>>8);
          EEPROM.write((i*2)+1,relayStates[i]);
          }
          EEPROM.commit();
          minCount = 0;
      }
      else
        minCount++;
      timerCount=0;
    }
    else
      timerCount++;
  }
}
/////////////////////////////////////////////////////////////////
void sendResponse(WiFiClient &client,String r)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
  client.println();
  

  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  // output the response
  client.println("<body>");
  client.println("<h1>");
  client.print(r);
  client.print("</h1>" );
  client.print("</body>" );
  client.println("</html>");
}
//////////////////////////////////////////////////////////////
void system_boot()
{
  //init WiFi
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect();  //Prevent connecting to wifi based on previous configuration
   
  //init serial
  //Serial.begin(115200);  
  
  //init eeprom
  EEPROM.begin(512);
  delay(1);
}
/////////////////////////////////////////////////////////////
void system_init()
{
  //configure all I/O & restore states
  for(int i=0; i< buttonCount; i++){
    pinMode(buttonPin[i], INPUT);// define a pin as input for Switch
  }
  for(int i=0; i< relayCount; i++){
    pinMode(relayPin[i], OUTPUT);// define a pin as output for relay
  }
  for(int i=0; i< relayCount; i++){
    relayStates[i] = EEPROM.read(i*2);
    relayStates[i] = relayStates[i]<<8;
    relayStates[i] = EEPROM.read((i*2)+1);
    if(relayStates[i])digitalWrite(relayPin[i], HIGH );
    else digitalWrite(relayPin[i], LOW );
    delay(200);
  }
  //Serial.println("I/O configured and states restored");
  
  //auto connect
  auto_connect();
  //start server
  server.begin();
  //Serial.println("HTTP server started");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  //start MDNS
  if (MDNS.begin("remoteera",WiFi.localIP())) {
    //Serial.println("MDNS responder started");
    //Serial.println("access via http://remotera");
  }
  //Initialize Timer every 1s
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP);
  timer1_write(312499);//from timer equation
}
//////////////////////////////////////////////////////////////
int parseCommand(String req,
                  String &c,String &ssid,String &password,unsigned int &relay,unsigned int &delayTime)
{
  //Serial.println("parsing");
  //example : "GET /r:1,500 HTTP/1.1"
  //trim req --> r:1,500
  req = req.substring(req.indexOf(" /")+2, req.indexOf("HTTP")-1); 
  //check if its not a command 
  //send 0
  c = req.substring(0, req.indexOf(":"));
  if(!c.equals("s") && !c.equals("r") && !c.equals("o") && !c.equals("t") && !c.equals("n"))
    return 0;        
  if(c.equals("r"))
    {
      relay = (req.substring(req.indexOf(":")+1, req.indexOf(","))).toInt();
      delayTime = (req.substring(req.indexOf(",")+1, req.length())).toInt();
    }
  else if(c.equals("t"))
    {
      delayTime = (req.substring(req.indexOf(":")+1, req.length())).toInt();
    }
  else if(c.equals("n"))
    {
      ssid = req.substring(req.indexOf(':')+1, req.indexOf(','));
      password = req.substring(req.indexOf(',')+1, req.length());
    }   
  if(delayTime>65000)
    delayTime=65000;    
  return 1;
}
///////////////////////////////////////////////////////////
String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
  data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
    return String(data);
}
////////////////////////////////////////////////////////////
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();
}
///////////////////////////////////////////////////////
void auto_connect()
{
  //Serial.println(" ");
  //Serial.println("starting auto_connect");
  String ssid = "";
  String password = ""; 
  ssid = readStringFromEEPROM(relayCount*2); // EEPROM contains relay states, ssid, password
  password = readStringFromEEPROM((relayCount*2)+ssid.length()+1);
  //Serial.println("connecting to : "+ssid+" with password : "+password);  
  delay(1);
  //set WIFI mode to station mode and try to connect
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  delay(1);
  WiFi.begin(ssid, password);
  delay(55);
  int time_out = 0; //in sec
  while (WiFi.status() != WL_CONNECTED && time_out < 20) {
    delay(1000);
    time_out++;
    //Serial.print(".");
  }
  if(WiFi.status() == WL_CONNECTED)
  {
    //Serial.println("WiFi connected.");
    return;
  }
  //Serial.println("WiFi not connected.");
  WiFi.disconnect(true);
  delay(1);
  //set WIFI to soft AP 
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(1);
  WiFi.softAP("remoteera");
  delay(10);
  return;
}
