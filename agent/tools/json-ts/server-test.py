#!/usr/bin/python

from jsonts import *
import socket

NUM_SENDS = 500
HOST = "localhost"
PORT = 9090

hello_msg = { "info" : { "hostName" : "localhost" } }

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

for i in range(NUM_SENDS):
    msg = createCommandMsg("hello", hello_msg)
    sock.send(serializeMsg(msg))