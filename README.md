# ProsecCoAP: A CoAP client and server library for Arduino.

<a href="http://coap.space/" target=_blank>CoAP</a> simple server/client library for Arduino IDE/PlatformIO, ESP32, ESP8266.

## Details
This library is a partial implementation of CoAP protocol ([RFC-7252](https://datatracker.ietf.org/doc/html/rfc7252)) supporting:
- request/respose semantics,
- observe pattern.

## Documentation
Documentation is available at: **COMING SOON**.

To manually build documentation, from the main folder, run:
```
doxygen
```
The documentation will be placed in [./doc/html/index.html](./html/index.html).

## How to install
### From Arduino IDE
** COMING SOON **

### Manual
Download this source code branch zip file and extract it to the Arduino libraries directory.
You should be able the find the examples from the Arduino IDE menu.


## How to test
The [examples](./examples) need CoAP server libcoap or microcoap server to work. 

This is how to use the example with libcoap on Ubuntu Linux. You don't need to use CoAP server (request/response), simply follow these steps:

```bash
git clone https://github.com/obgm/libcoap 
cd libcoap/
./autogen.sh 
./configure
make
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.libs
gcc -o coap-server ./examples/coap-server.c -I./include -I. -L.libs -lcoap-1 -DWITH_POSIX
gcc -o coap-client ./examples/client.c ./examples/coap_list.c -I./include -I. -L.libs -lcoap-1 -DWITH_POSIX
./coap-server
```
Next, start Arduino and check the request/response in the serial monitor.

## Particle Photon, Core compatible
Check <a href="https://github.com/hirotakaster/CoAP">this</a> version of the library for Particle Photon, Core compatibility.

## Credits
This library is a fork of [CoAP-simple-library](https://github.com/hirotakaster/CoAP-simple-library) by Hirotaka Niisato. Credits for all the orginal code go to the original contributors.