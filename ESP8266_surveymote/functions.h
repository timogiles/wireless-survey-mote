// This-->tab == "functions.h"

// Expose Espressif SDK functionality
extern "C" {
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int  wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int  wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

extern String digitalClockString;
extern String nowString;
extern String dataString;
extern void SDLog();

#include <ESP8266WiFi.h>
#include "./structures.h"

#define MAX_APS_TRACKED 100
#define MAX_CLIENTS_TRACKED 200

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
int nothing_new = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

char *tempdataString = (char*)malloc(100 * sizeof(char));


int register_beacon(beaconinfo beacon)
{
  int known = 0;   // Clear known flag
  for (int u = 0; u < aps_known_count; u++)
  {
    if (! memcmp(aps_known[u].bssid, beacon.bssid, ETH_MAC_LEN)) {
      known = 1;
      break;
    }   // AP known => Set known flag
  }
  if (! known)  // AP is NEW, copy MAC to array and return it
  {
    memcpy(&aps_known[aps_known_count], &beacon, sizeof(beacon));
    aps_known_count++;

    if ((unsigned int) aps_known_count >=
        sizeof (aps_known) / sizeof (aps_known[0]) ) {
      Serial.printf("exceeded max aps_known\n");
      aps_known_count = 0;
    }
  }
  return known;
}

int register_client(clientinfo ci)
{
  int known = 0;   // Clear known flag
  for (int u = 0; u < clients_known_count; u++)
  {
    if (! memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN)) {
      known = 1;
      break;
    }
  }
  if (! known)
  {
    memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));
    clients_known_count++;

    if ((unsigned int) clients_known_count >=
        sizeof (clients_known) / sizeof (clients_known[0]) ) {
      Serial.printf("exceeded max clients_known\n");
      clients_known_count = 0;
    }
  }
  return known;
}

void print_beacon(beaconinfo beacon)
{
  if (beacon.err != 0) {
    //Serial.printf("BEACON ERR: (%d)  ", beacon.err);
  } else {
    dataString += String(digitalClockString + "," + nowString + ",");
    sprintf(tempdataString, "BEACON,             ,%32s,", beacon.ssid);
    dataString += String(tempdataString);
    for (int i = 0; i < 6; i++){ 
      sprintf(tempdataString,"%02x", beacon.bssid[i]);
      dataString += String(tempdataString);
    }
    //Serial.printf(",  %2d", beacon.channel);
    sprintf(tempdataString,",  %2d", beacon.channel);
    dataString += String(tempdataString);
    //Serial.printf(",  %4d\r\n", beacon.rssi);
    sprintf(tempdataString,",  %4d", beacon.rssi);
    dataString += String(tempdataString);
    SDLog();
  }
}

void print_client(clientinfo ci)
{
  int u = 0;
  int known = 0;   // Clear known flag
  if (ci.err != 0) {
    // nothing
  } else {
    dataString = String(digitalClockString + "," + nowString + ",");
    dataString += "DEVICE, ";
    for (int i = 0; i < 6; i++){
      sprintf(tempdataString,"%02x", ci.station[i]);
      dataString += String(tempdataString);
    }
    dataString += ",";

    for (u = 0; u < aps_known_count; u++)
    {
      if (! memcmp(aps_known[u].bssid, ci.bssid, ETH_MAC_LEN)) {
        sprintf(tempdataString,"%32s,", aps_known[u].ssid);
        dataString += String(tempdataString);
        known = 1;     // AP known => Set known flag
        break;
      }
    }
    if (! known)  {
      sprintf(tempdataString,"   Unknown/Malformed packet \r\n");
      dataString += String(tempdataString);
    } else {
      for (int i = 0; i < 6; i++){
        sprintf(tempdataString,"%02x", ci.ap[i]);
        dataString += String(tempdataString);
      }
      sprintf(tempdataString,", %3d", aps_known[u].channel);
      dataString += String(tempdataString);
      sprintf(tempdataString,",  %4d", ci.rssi);
      dataString += String(tempdataString);
      SDLog();
    }
  }
}

void promisc_cb(uint8_t *buf, uint16_t len)
{
  int i = 0;
  uint16_t seq_n_new = 0;
  if (len == 12) {
    struct RxControl *sniffer = (struct RxControl*) buf;
  } else if (len == 128) {
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
    if (register_beacon(beacon) == 0) {
      print_beacon(beacon);
      nothing_new = 0;
    }
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    //Is data or QOS?
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
        if (register_client(ci) == 0) {
          print_client(ci);
          nothing_new = 0;
        }
      }
    }
    //if (sniffer->buf[0] == 0x40) {//probe request
    //  
    //}
  }
}

