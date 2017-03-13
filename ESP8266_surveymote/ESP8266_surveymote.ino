// Adapted by Tim Giles from code by:
// by Ray Burnette 20161013 compiled on Linux 16.3 using Arduino 1.6.12
//
// Intended Operation:
// This code is written to run using an Adafruit Feather ESP8266 HUZZAH and a Feather SD logging shield. 
// Upon power up, the code searches for one of the 3 wifi networks below to connect and get the current time 
// from and NTP time server via the internet.  Once the time is set the code set the wifi to a permiscuous mode.
// In one minute intervals, all the packets that it sees over wifi will be decoded to log the MAC, device type, 
// channel and RSSI of the packet.  The data is both stored to the SD card and printed out the USB-serial port.  
// Each unique MAC is only logged once until the end of the one minute interval. At the end of the interval 
// cache of "known" MACs is cleared, and the logging is repeated.

#include <ESP8266WiFi.h>
#include "./functions.h"
#include <WiFiUdp.h>
#include <TimeLib.h> 
#include <SPI.h>
#include <SD.h>
#define disable 0
#define enable  1
#define LED1 0 //RED, feather HUZZAH
#define LED2 2 //BLUE, feather HUZZAH

//define the chipSelect pin used for the SD card
const int chipSelect = 15;
unsigned int channel = 1;

//declare functions
time_t getNtpTime();
void digitalClockDisplay();
void sendNTPpacket(IPAddress &address);
void printDigits(int digits);
void SDLog(String LogString, boolean ForceLog);

//SSIDs and passwords
const char ssid[] = "ssid1";  //  your network SSID (name)
const char pass[] = "pass1";       // your network password
const char ssid2[] = "ssid2";  //  your network SSID (name)
const char pass2[] = "pass2";       // your network password
const char ssid3[] = "ssid3";  //  your network SSID (name)
const char pass3[] = "pass3";       // your network password

//global time variables
int prevminute = 0;
String digitalClockString = "";
String digitalDateString = "";
String nowString = "";

//global string SD card logging
String dataString = "";
boolean DontLog = true;

//NTP Servers, for getting time over the internet:
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
const int timeZone = -4; 
unsigned long ElapsedTime = millis();

//
void setup() {
  int WifiTry = 0;               

  //set pin direction for LED indicators
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  //turn off blue LED
  digitalWrite(LED1,0);
  //turn on red LED, booting
  digitalWrite(LED2,1);

  //intialize serial port, print debug info
  Serial.begin(115200);
  Serial.println(" ");
  Serial.print("SDK version: ");
  Serial.println(system_get_sdk_version());
  Serial.println("SOFWERX");
  Serial.println("01/31/2017");

  //start WIFI, connect to a known AP, get the time from an NTP server
  //if WIFI doesn't connect time will start at 1/1/1970, but accurate time is still kept
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  ElapsedTime = millis();//remember what time it is now
  while ((WiFi.status() != WL_CONNECTED) && WifiTry < 4) {//loop until one of the APs connects
    if ((millis() - ElapsedTime) > 15000 && ((WifiTry == 0) || (WifiTry == 3))){
      Serial.print(ssid);
      Serial.println(" failed to connect");
      Serial.print("Connecting to ");
      Serial.println(ssid2);
      WiFi.begin(ssid2, pass2);
      ElapsedTime = millis();//remember what time it is now
      WifiTry += 1;
    }
    if ((millis() - ElapsedTime) > 15000 && ((WifiTry == 1) || (WifiTry == 4))){
      Serial.print(ssid2);
      Serial.println(" failed to connect");
      Serial.print("Connecting to ");
      Serial.println(ssid3);
      WiFi.begin(ssid3, pass3);
      ElapsedTime = millis();//remember what time it is now
      WifiTry += 1;
    }
    if ((millis() - ElapsedTime) > 15000 && ((WifiTry == 2) || (WifiTry == 5))){
      Serial.print(ssid3);
      Serial.println(" failed to connect");
      Serial.print("Connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, pass);
      ElapsedTime = millis();//remember what time it is now
      WifiTry += 1;
    }
    //blink red LED while waiting to connect
    delay(250);
    digitalWrite(LED1,1);
    Serial.print(".");
    delay(250);
    digitalWrite(LED1,0);
  }
  //turn off LEDs, log IP information
  digitalWrite(LED1,0);
  digitalWrite(LED2,0);
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  
  Serial.print("Current Time is: ");
  digitalClockDisplay();
  Serial.println(digitalClockString);
  
  //initialize SD card
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  }
  else{
    Serial.println("card initialized.");
  }
  digitalWrite(LED1,1);
  digitalWrite(LED2,1);

  //write initial log header
  prevminute = minute();
  clients_known_count = 0;
  aps_known_count = 0;
   
  SDLog("-Time---,-UTCTIME--,-Type-,-----MAC-----,-----WiFi Access Point SSID-----,----MAC-----,-Chnl-,RSSI", true); //true forces logging even when "DontLog" is true
    
  digitalClockDisplay();
  SDLog(String("Time, " + digitalClockString + "," + digitalDateString), true); //true forces logging even when "DontLog" is true
  DontLog = false;
  
  //begin sniffing 
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb);   // Set up promiscuous callback
  wifi_promiscuous_enable(enable);
}

void loop() {
  if(prevminute != minute()){
    //suspend logging during the reset peroid
    DontLog = true;
    prevminute = minute();
    clients_known_count = 0;
    aps_known_count = 0;
   
    SDLog("-Time---,-UTCTIME--,-Type-,-----MAC-----,-----WiFi Access Point SSID-----,----MAC-----,-Chnl-,RSSI", true); //true forces logging even when "DontLog" is true
    
    digitalClockDisplay();
    SDLog(String("Time, " + digitalClockString + "," + digitalDateString), true); //true forces logging even when "DontLog" is true

    DontLog = false;
  }else{
    channel = 1;
    wifi_set_channel(channel);
    while (true) {
      nothing_new++;                          // Array is not finite, check bounds and adjust if required
      if (nothing_new > 200) {
        nothing_new = 0;
        channel++;
        if (channel == 15) break;             // Only scan channels 1 to 14
        wifi_set_channel(channel);
      }
      delay(1);  // critical processing timeslice for NONOS SDK! No delay(0) yield()
      // Press keyboard ENTER in console with NL active to repaint the screen
      if ((Serial.available() > 0) && (Serial.read() == '\n')) {
        Serial.println("\n-------------------------------------------------------------------------------------\n");
        for (int u = 0; u < clients_known_count; u++){
          print_client(clients_known[u]);
        }
        for (int u = 0; u < aps_known_count; u++){          
          print_beacon(aps_known[u]);
        }
        Serial.println("\n-------------------------------------------------------------------------------------\n");
      }
    }
  }
}

void SDLog(){
  SDLog(dataString, false);
}
void SDLog(String LogString, boolean ForceLog){
  dataString = LogString;
  if ((DontLog == false) || (ForceLog == true)){
    digitalWrite(LED2,0);//LED is blue while SD card is being accessed
    File dataFile = SD.open("CSVout.csv", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      Serial.println(dataString);
      dataString = "";
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.print(dataString);
      Serial.println(",SDERROR - error opening datalog.txt");
    }  
    digitalWrite(LED2,1);
  }else{
    //clear data string if logging is suspended
    dataString = "";
  }
}

void digitalClockDisplay(){
  // convert time and date to strings that are conveniently printable
  nowString = now();
  digitalClockString = hour();
  printDigits(minute());
  printDigits(second());
  digitalDateString = String(String(day()) + "." + String(month()) + "." + String(year()));
}
void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  digitalClockString += ":";
  if(digits < 10){
    digitalClockString += "0";
  }
  digitalClockString += digits;
}


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

//**TimeNTP over Wifi
