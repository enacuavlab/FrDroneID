#!/usr/bin/python3
from serial import Serial
from rid import FrRID
import time
from math import sin, cos, degrees, pi
import sys

if __name__ == '__main__':
    lat0 = 43.25
    lon0 = 1.25
    r = 0.02
    w = 0.5

    packet = FrRID()
    packet.fr_id = "TSTAAA000000000000000123456789"
    packet.lat_to = 43.25
    packet.lon_to = 1.2564
    packet.hspeed = 20
    packet.hmsl = 230
    packet.route = 120

    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = "/dev/ttyUSB1"

    serial = Serial(port, 115200)

    start = time.time()
    while True:
        t = time.time() - start
        theta = (w*t) % (2*pi)
        dlat = r * sin(theta)
        dlon = r * cos(theta)
        route = 360 - degrees(theta)
        packet.route = route
        packet.lat = lat0 + dlat
        packet.lon = lon0 + dlon

        msg = packet.make_message()
        print(msg)
        serial.write(msg)
        time.sleep(0.2)
