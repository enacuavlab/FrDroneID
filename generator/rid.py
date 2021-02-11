#!/usr/bin/python3
from enum import Enum


class Type(Enum):
    UAS_PROTOCOL_VERSION = 1
    UAS_ID_FR = 2
    UAS_ID_ANSI_UAS = 3
    UAS_LAT = 4
    UAS_LON = 5
    UAS_HMSL = 6
    UAS_HAGL = 7
    UAS_LAT_TO = 8
    UAS_LON_TO = 9
    UAS_H_SPEED = 10
    UAS_ROUTE = 11


FIELD_LENGTH = {
    Type.UAS_PROTOCOL_VERSION: 1,
    Type.UAS_ID_FR: 30,
    Type.UAS_ID_ANSI_UAS: 20,   # at most, but can be in the range [6..20]
    Type.UAS_LAT: 4,
    Type.UAS_LON: 4,
    Type.UAS_HMSL: 2,
    Type.UAS_HAGL: 2,
    Type.UAS_LAT_TO: 4,
    Type.UAS_LON_TO: 4,
    Type.UAS_H_SPEED: 1,
    Type.UAS_ROUTE: 2
}


class FrRID:
    def __init__(self):
        self.fr_id = ""
        self.lat = None
        self.lon = None
        self.hmsl = None
        self.hagl = None
        self.lat_to = None
        self.lon_to = None
        self.hspeed = None
        self.route = None
        self._data = b''    # type: bytes

    def make_data(self):
        assert len(self.fr_id) == 30
        assert self.lat is not None
        assert self.lon is not None
        assert self.lat_to is not None
        assert self.lon_to is not None
        assert self.hspeed is not None
        assert self.route is not None and 0 <= self.route <= 360
        assert (self.hmsl is not None) or (self.hagl is not None)

        # init with version protocol field
        self._data = bytes([1, 1, 1])

        self.put_field(Type.UAS_ID_FR, self.fr_id.encode())
        self.put_field(Type.UAS_LAT, int(self.lat * 1e5).to_bytes(4, 'big'))
        self.put_field(Type.UAS_LON, int(self.lon * 1e5).to_bytes(4, 'big'))
        self.put_field(Type.UAS_LAT_TO, int(self.lat_to * 1e5).to_bytes(4, 'big'))
        self.put_field(Type.UAS_LON_TO, int(self.lon_to * 1e5).to_bytes(4, 'big'))
        if self.hmsl is not None:
            self.put_field(Type.UAS_HMSL, int(self.hmsl).to_bytes(2, 'big'))
        if self.hagl is not None:
            self.put_field(Type.UAS_HAGL, int(self.hagl).to_bytes(2, 'big'))
        self.put_field(Type.UAS_ROUTE, int(self.route).to_bytes(2, 'big'))
        self.put_field(Type.UAS_H_SPEED, int(self.hspeed).to_bytes(1, 'big'))

    def put_field(self, field: Type, value: bytes):
        data = chr(field.value).encode() + chr(FIELD_LENGTH[field]).encode() + value
        self._data += data

    def compute_chk(self) -> bytes:
        data = chr(len(self._data)+4).encode() + self._data
        cka = ckb = 0
        for c in data:
            cka += c
            cka = cka & 0xFF
            ckb += cka
            ckb = ckb & 0xFF
        return bytes([cka, ckb])

    def make_message(self):
        self.make_data()
        msg = chr(0x99).encode() + chr(len(self._data)+4).encode() + self._data + self.compute_chk()
        return msg


if __name__ == '__main__':
    packet = FrRID()
    packet.fr_id = "TSTAAA000000000000000123456789"
    packet.lat_to = 43.25
    packet.lon_to = 1.2564
    packet.lat = 43.25
    packet.lon = 1.2564
    packet.hspeed = 20
    packet.hmsl = 230
    packet.route = 120
    print(packet.make_message())
