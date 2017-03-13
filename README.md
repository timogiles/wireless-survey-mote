Wireless Survey Mote
======

The Wireless Survey Mote project is a small battery powered wifi device.  Based on the cheap and popular ESP8266, it operates in "permiscuous" mode to log WIFI device MAC addesses.  It doesn't attach to an network, it just listens for WIFI packets and strips the BSSID, SSID and MAC information from them.  Each minute all the unique information is stored to an SD card.  The buffers are then cleared and the cycle repeats.

At the very beginning of operation the ESP8266 will try to connect to a known wifi network to get the current time.  This is just so that the log is time accurate.  After the time is received it disconnects from the known network and starts listening.  If a known network connection is not possible then the time will start counting from zero.

Hardware
======

The Wireless Survey Mote was prototyped using an [Adafruit Feather HUZZAH](https://www.adafruit.com/product/2821) and an [Adalogger FeatherWing](https://www.adafruit.com/products/2922).  The RTC battery holder can be removed as it isn't needed.  Solder the two boards together using the included headers, upload the code, install an SD card.  The Feather board can be powered off of a 1S lithium battery or the USB connector.