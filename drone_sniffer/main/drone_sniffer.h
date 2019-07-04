
#ifndef DRONE_SNIFFER_H
#define DRONE_SNIFFER_H


//little or big endian ???
struct uas_raw_payload {
  char id_fr[31];    //manufacturer trigram on 3 bytes, aircraft or beacon model on 3 bytes, aircraft or beacon serial number on 24 bytes (with 0 padding if necessary).
  int32_t lat;      // [-90; 90] * 10e5
  int32_t lon;      // ]180; 180]  * 10e5
  int16_t hmsl; //either hmsl or hagl !
  int16_t hagl; //in m
  int32_t lat_to;   // [-90; 90] * 10e5
  int32_t lon_to;   // ]180; 180]  * 10e5
  uint8_t h_speed;  // in m/s
  uint16_t route;   // [0; 359]

  uint16_t types; // For now, types are in [1; 11]. Use bitshift to flag existing fields.
};

enum uas_type {
  // 0 reserved for future use
  UAS_PROTOCOL_VERSION = 1,
  UAS_ID_FR = 2,
  UAS_ID_ANSI_UAS =3,
  UAS_LAT = 4,
  UAS_LON =5,
  UAS_HMSL = 6,
  UAS_HAGL =7,
  UAS_LAT_TO = 8,
  UAS_LON_TO = 9,
  UAS_H_SPEED = 10,
  UAS_ROUTE = 11
  // 12 to 200 are reserved for future use
};



#endif
