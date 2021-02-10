#include <ESP8266WiFi.h>

#define MAX_PAYLOAD_LEN 80

#define PPRZ_STX 0x99

const uint8_t ID_TYPE = 2;

// EDIT the 2 following fields to match your manufacturer ID and the tracker model. (3 chars each)
// 000 is reserved for DIY beacons, so you can keep it.
const char tracker_manufacturer[4] = "000";
const char tracker_model[4] = "000";
char tracker_serial[25] = {0};

extern "C" {
  #include "user_interface.h"
}

enum normal_parser_states {
  SearchingPPRZ_STX,
  ParsingLength,
  ParsingPayload,
  CheckingCRCA,
  CheckingCRCB
};

struct normal_parser_t {
  enum normal_parser_states state;
  unsigned char length;
  int counter;
  unsigned char sender_id;
  unsigned char msg_id;
  unsigned char payload[256];
  unsigned char crc_a;
  unsigned char crc_b;
};

typedef struct {
    uint16_t version:2;
    uint16_t type:2;
    uint16_t subtype:4;
    uint16_t to_ds:1;
    uint16_t from_ds:1;
    uint16_t mf:1;
    uint16_t retry:1;
    uint16_t pwr:1;
    uint16_t more:1;
    uint16_t w:1;
    uint16_t o:1;
} wifi_ieee80211_frame_ctrl_t;

typedef struct {
    wifi_ieee80211_frame_ctrl_t frame_ctrl;
    uint16_t duration;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint16_t seq_ctrl;
} wifi_ieee80211_hdr_t;

/* linux sources */
typedef struct {
    uint8_t timestamp[8];
    uint16_t beacon_int;
    uint16_t capab_info;
    /* followed by some of SSID, Supported rates,
     * FH Params, DS Params, CF Params, IBSS Params, TIM */
    uint8_t variable[0];
} wifi_ieee80211_mgmt_beacon_t;

typedef struct {
    uint8_t element_id;
    uint8_t len;
    uint8_t variable[0];
} wifi_ieee80211_element_t;


size_t
create_beacon_packet(char *ssid, uint8_t id, uint8_t *buff, uint8_t *uas_payload, uint8_t uas_len, size_t buffsize) {
    if (buff == NULL || buffsize < (60 + strlen(ssid))) {
        return (-1);
    }

    size_t len = 0;

    /* header */
    wifi_ieee80211_hdr_t *hdr = (wifi_ieee80211_hdr_t *) buff;
    memset(hdr, 0, sizeof(wifi_ieee80211_hdr_t));
    hdr->frame_ctrl.subtype = 0x8;
    hdr->duration = 0;
    uint8_t da[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t sa[] = {0xba, 0xde, 0xaf, 0xfe, 0x00, id};
    uint8_t bssid[] = {0xba, 0xde, 0xaf, 0xfe, 0x00, id};
    memcpy(hdr->da, da, 6);
    memcpy(hdr->sa, sa, 6);
    memcpy(hdr->bssid, bssid, 6);

    len += sizeof(wifi_ieee80211_hdr_t);

    /* beacon */
    wifi_ieee80211_mgmt_beacon_t *beacon = (wifi_ieee80211_mgmt_beacon_t *) (buff + len);
    memset(beacon, 0, sizeof(wifi_ieee80211_mgmt_beacon_t));
    beacon->beacon_int = 100; // 100TU = 102.4 milliseconds
    beacon->capab_info = 0b00110001 << 8 | 0b00000100; // capabilities subfields
    len += sizeof(wifi_ieee80211_mgmt_beacon_t);

    /* SSID */
    wifi_ieee80211_element_t *element = (wifi_ieee80211_element_t *) (buff + len);
    element->element_id = 0x00; // id of SSID element
    element->len = strlen(ssid);
    memcpy(element->variable, ssid, element->len);
    len += sizeof(wifi_ieee80211_element_t) + element->len;

    /* supported rates */
    element = (wifi_ieee80211_element_t *) (buff + len);
    element->element_id = 0x01; // id of Supported Rates
    element->len = 8;
    memcpy(element->variable, (const uint8_t[]) {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24}, element->len);
    len += sizeof(wifi_ieee80211_element_t) + element->len;

    /* DS parameters */
    uint8_t channel = 6;
    element = (wifi_ieee80211_element_t *) (buff + len);
    element->element_id = 0x03; // id of DS params
    element->len = 1;
    memcpy(element->variable, &channel, element->len);
    len += sizeof(wifi_ieee80211_element_t) + element->len;

    /* traffic indication map */
    element = (wifi_ieee80211_element_t *) (buff + len);
    element->element_id = 0x05; // id of Traffic Indication Map
    element->len = 4;
    memcpy(element->variable, (const uint8_t[]) {0x01, 0x02, 0x00, 0x00}, element->len);
    len += sizeof(wifi_ieee80211_element_t) + element->len;


    /* Vendor specific : UAS stuff !!!!! */
    
    element = (wifi_ieee80211_element_t *) (buff + len);
    element->element_id = 0xDD; // id of Vendor Specific stuff
    element->len = uas_len + 4 + 30 + 2; // [OUI/CID(3)], [VS Type(1)], [trackerId: type(1), len(1), ID(30)] , [uas_payload(uas_len)]
    memcpy(element->variable, (const uint8_t[]) {0x6A, 0x5C, 0x35}, 3);     //CID: company ID : 6A-5C-35
    memcpy(element->variable + 3, (const uint8_t[]) {0x01}, 1);             //VS Type: protocol number, fixed to 0x01.
    
    // Add field "tracker ID".
    element->variable[4] = ID_TYPE;
    element->variable[5] = 30;
    memcpy(element->variable + 6, tracker_manufacturer, 3);
    memcpy(element->variable + 9, tracker_model, 3);
    memcpy(element->variable + 12, tracker_serial, 24);

    memcpy(element->variable + 36, uas_payload, uas_len);  //uas payload
    len += sizeof(wifi_ieee80211_element_t) + element->len;

    return len;
}

void
emit_beacon(char *ssid, uint8_t id, uint8_t *uas_payload, uint8_t uas_len) {
    size_t len;
    //uint8_t buff[128];
    uint8_t buff[100 + MAX_PAYLOAD_LEN];

    len = create_beacon_packet(ssid, id, buff, uas_payload, uas_len, sizeof(buff));
    //Serial.println(len);
    wifi_send_pkt_freedom(buff, len, 1);
}

char ssid[30];// = "pprz_beacon_123";
struct normal_parser_t parser;
unsigned char last_payload[256];
int msg_len = 0;

void setup() {
  delay(500);
  Serial.begin(115200);
  uint32_t chip_id = system_get_chip_id();
  sprintf(ssid, "pprz_beacon_%010X", chip_id);
  sprintf(tracker_serial, "%024X", chip_id);

  //wifi_set_opmode(STATION_MODE);
  //wifi_promiscuous_enable(1); 

  WiFi.mode(WIFI_OFF);
  
  // set default AP settings
  WiFi.softAP(ssid, nullptr, 6, false, 0); // ssid, pwd, channel, hidden, max_cnx, 
  WiFi.setOutputPower(20.5); // max 20.5dBm
  
  softap_config current_config;
  wifi_softap_get_config(&current_config);

  current_config.beacon_interval = 1000;
  wifi_softap_set_config(&current_config);
  
}




void loop() {

  while(Serial.available() > 0) {
    unsigned char inbyte = Serial.read();
    if (parse_single_byte(inbyte)) { // if parser.payload message detected
      memcpy(last_payload, parser.payload, parser.length - 4);
      msg_len = parser.length - 4;
      Serial.println("MSG RCV");
      //emit beacon frame as soon as the message is received
      emit_beacon(ssid, 1, last_payload, msg_len);
    }
  }
  
}

/*
 * PPRZ-message: ABCxxxxxxxDE
    A PPRZ_STX (0x99)
    B LENGTH (A->E)
    C PAYLOAD
    D PPRZ_CHECKSUM_A (sum[B->C])
    E PPRZ_CHECKSUM_B (sum[ck_a])
    Returns 0 if not ready, return 1 if complete message was detected
*/
uint8_t parse_single_byte(unsigned char in_byte)
{
  switch (parser.state) {

    case SearchingPPRZ_STX:
      if (in_byte == PPRZ_STX) {
        parser.crc_a = 0;
        parser.crc_b = 0;
        parser.counter = 1;
        parser.state = ParsingLength;
      }
      break;

    case ParsingLength:
      parser.length = in_byte;
      parser.crc_a += in_byte;
      parser.crc_b += parser.crc_a;
      parser.counter++;
      parser.state = ParsingPayload;
      break;

    case ParsingPayload:
      parser.payload[parser.counter-2] = in_byte;
      parser.crc_a += in_byte;
      parser.crc_b += parser.crc_a;
      parser.counter++;
      if (parser.counter == parser.length - 2) {
        parser.state = CheckingCRCA;
      }
      break;

    case CheckingCRCA:
      //printf("CRCA: %d vs %d\n", in_byte, parser.crc_a);
      if (in_byte == parser.crc_a) {
        parser.state = CheckingCRCB;
      }
      else {
        parser.state = SearchingPPRZ_STX;
      }
      break;

    case CheckingCRCB:
      //printf("CRCB: %d vs %d\n", in_byte, parser.crc_b);
      if (in_byte == parser.crc_b) {
        parser.state = SearchingPPRZ_STX;
        return 1;
      }
      parser.state = SearchingPPRZ_STX;
      break;

    default:
      /* Should never get here */
      break;
  }
  
  return 0;
}
