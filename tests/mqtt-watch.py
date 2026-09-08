#!/usr/bin/env python3
"""
Watch the canary's MQTT topics. Speaks just enough MQTT 3.1.1 to subscribe,
so there is nothing to install -- handy on a box without paho or
mosquitto-clients.

  ./tests/mqtt-watch.py <broker-host> [topic]

Host may also come from the CANARY_BROKER environment variable. Topic
defaults to the tree the firmware publishes to.
"""
import os, socket, struct, sys, datetime

HOST  = (sys.argv[1] if len(sys.argv) > 1
         else os.environ.get("CANARY_BROKER", ""))
if not HOST:
    sys.exit("usage: mqtt-watch.py <broker-host> [topic]   "
             "(or set CANARY_BROKER)")
TOPIC = sys.argv[2] if len(sys.argv) > 2 else "canary/glasses/#"
PORT  = 1883

def enc_str(s):
    b = s.encode()
    return struct.pack("!H", len(b)) + b

def enc_remaining(n):
    out = b""
    while True:
        d = n % 128; n //= 128
        if n: d |= 0x80
        out += bytes([d])
        if not n: return out

def read_remaining(sock):
    mult, val = 1, 0
    while True:
        b = sock.recv(1)
        if not b: raise ConnectionError("broker closed the connection")
        val += (b[0] & 127) * mult
        if not (b[0] & 128): return val
        mult *= 128

def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        c = sock.recv(n - len(buf))
        if not c: raise ConnectionError("broker closed the connection")
        buf += c
    return buf

s = socket.create_connection((HOST, PORT), timeout=10)

vh = enc_str("MQTT") + bytes([0x04, 0x02]) + struct.pack("!H", 60)
body = vh + enc_str("canary-watch")
s.sendall(bytes([0x10]) + enc_remaining(len(body)) + body)
if recv_exact(s, 4)[3] != 0:
    sys.exit("broker refused the connection")

body = struct.pack("!H", 1) + enc_str(TOPIC) + bytes([0])   # QoS 0
s.sendall(bytes([0x82]) + enc_remaining(len(body)) + body)
recv_exact(s, 1); recv_exact(s, read_remaining(s))          # SUBACK

print(f"watching {TOPIC} on {HOST}:{PORT} -- Ctrl-C to stop\n", flush=True)
s.settimeout(None)
try:
    while True:
        hdr = recv_exact(s, 1)[0]
        payload = recv_exact(s, read_remaining(s))
        if hdr >> 4 != 3:                                   # only PUBLISH
            continue
        tlen = struct.unpack("!H", payload[:2])[0]
        topic = payload[2:2+tlen].decode(errors="replace")
        msg = payload[2+tlen:].decode(errors="replace")
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        retained = " (retained)" if hdr & 0x01 else ""
        print(f"{ts}  {topic}{retained}\n          {msg}", flush=True)
except KeyboardInterrupt:
    pass
finally:
    s.close()
