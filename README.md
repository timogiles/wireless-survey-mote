Wireless Survey Mote
======

The Wireless Survey Mote project is a small battery powered wifi device.  Based on the cheap and popular ESP8266, it operates in "permiscuous" mode to log WIFI device MAC addesses.  It doesn't attach to an network, it just listens for WIFI packets and strips the BSSID, SSID and MAC information from them.  Each minute all the unique information is stored to an SD card.  The buffers are then cleared and the cycle repeats.

At the very beginning of operation the ESP8266 will try to connect to a known wifi network to get the current time.  This is just so that the log is time accurate.  After the time is received it disconnects from the known network and starts listening.  If a known network connection is not possible then the time will start counting from zero.