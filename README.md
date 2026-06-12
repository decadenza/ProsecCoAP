# ProsecCoAP 🥂 - CoAP client/server library for Arduino

<a href="http://coap.space/" target=_blank>Constrained Application Protocol (CoAP)</a> server/client library for Arduino IDE/PlatformIO, ESP32, ESP8266.

![Build with Arduino](https://github.com/decadenza/ProsecCoAP/actions/workflows/build-arduino.yml/badge.svg) ![Build with PlatformIO](https://github.com/decadenza/ProsecCoAP/actions/workflows/build-platformio.yml/badge.svg)

Documentation is available at [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).

## Details
This library is an implementation of CoAP protocol ([RFC-7252](https://datatracker.ietf.org/doc/html/rfc7252)).
The aim is to provide all the compulsory functionalities, maintaining the execution lightweight and clearly documenting its API. 

Please open an issue to request missing functionalities or report bugs.

## How to install
### Install from Arduino IDE Library Manager
1. Open the *Sketch* menu in the IDE.
2. Navigate to *Include Library > Manage Libraries*.
3. Search for "ProsecCoAP" and install.

### Install from PlatformIO CLI
```
pio pkg install --library ProsecCoAP
```

### Manual install
1. Download this source code branch as a zip file.
2. In the Arduino IDE, navigate to *Sketch > Include Library > Add .ZIP Library*. At the top of the drop down list, select the option to "Add .ZIP Library".

## Getting started
A simple server can be started as:
```cpp
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

// Initialise a node.
EthernetUDP Udp;
Coap::Node coapNode(Udp);

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
IPAddress deviceIp(192, 168, 0, 99);

// Your custom callback declaration.
void myCallback(Coap::Message &message, IPAddress ip, uint16_t port);

void setup()
{
  Ethernet.begin(mac, deviceIp);
  coapNode.serve("my-endpoint", myCallback);
  coapNode.start();
}

void loop() {
  coapNode.loop();
}


// Send "42" as a response to a GET request.
void myCallback(Coap::Message &message, IPAddress ip, uint16_t port)
{
  if (message.getCode() == Coap::MessageCode::GET)
  {
    Coap::Message response;
    response.intoResponse(message, Coap::MessageCode::CONTENT);
    response.addPayload((const uint8_t *)("42"), 2, Coap::ContentFormat::TEXT_PLAIN);
    coapNode.sendMessage(response, ip, port);
  }
}
```
Navigate to *File > Examples > ProsecCoAP* in Arduino IDE or check the [example folder](./examples/) for more examples.

### Run the examples
For the examples to compile and work correctly, please ensure to have all the necessary boards installed.
The examples need standard libraries, like:
- `Ethernet` by Arduino
- `WiFi` by Arduino

Refer to the comments in each example for details.

### How to test
#### Verify compile errors and warnings for multiple boards
To quickly verify a successful build process for multiple boards: 
1. Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/).
2. Ensure the core for the supported boards are installed:
```
arduino-cli core install arduino:avr
arduino-cli core install esp32:esp32
arduino-cli core install esp8266:esp8266 --additional-urls http://arduino.esp8266.com/stable/package_esp8266com_index.json
```
3. Run `make`.

#### Functional tests through the examples
The [examples](https://github.com/decadenza/ProsecCoAP/tree/main/examples) need another CoAP node to be executed. You can, alternatively:
- Use two devices and check serial monitor of each.
- Use one device and a [CoAP tool](https://coap.space/tools.html) on your computer for testing.
  For example, [libcoap](https://github.com/obgm/libcoap) by compiling it yourself or use the example binaries available as Debian package [libcoap3-bin](https://packages.debian.org/stable/libs/libcoap3-bin). In this case, you can test with `coap-client-notls` and `coap-server-notls`.

## Documentation
Documentation is available at: [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).
It points to the `main` branch.

### Building the documentation
To manually build documentation in your current working directory, run:
```
doxygen
```
The documentation will be accessible from `./html/index.html`.

## Credits
This library was inspired by [CoAP-simple-library](https://github.com/hirotakaster/CoAP-simple-library). Credits for the original code go to the original contributors.
This project maintains the original MIT License and continues the spirit of open-source IoT development.
