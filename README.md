# ProsecCoAP 🥂 - CoAP client/server library for Arduino.

<a href="http://coap.space/" target=_blank>Constrained Application Protocol (CoAP)</a> server/client library for Arduino IDE/PlatformIO, ESP32, ESP8266.

Documentation is available at [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).

## Details
This library is an implementation of CoAP protocol ([RFC-7252](https://datatracker.ietf.org/doc/html/rfc7252)).
It aims at implementing all the compulsory functionalities of the protocol, maintaining the execution lightweight and clearly documenting its API. 

This library is a **work in progress**. Although CoAP request/response pattern and observe pattern are implemented, specific functionalities may be delivered in future releases. Please open an issue to request missing functionalities or report bugs.

## How to install
### Pre-requirements and dependencies
For the examples to compile and work correctly, please ensure to have all the necessary boards installed.

In addition, these libraries are needed:
- `Ethernet` by Arduino
- `WiFi` by Arduino

### Install from Arduino IDE Library Manager
1. Open the *Sketch* menu in the IDE.
2. Navigate to *Include Library > Manage Libraries*.
3. Search for "ProsecCoAP" and install.

### Manual install
1. Download this source code branch as a zip file.
2. In the Arduino IDE, navigate to *Sketch > Include Library > Add .ZIP Library*. At the top of the drop down list, select the option to "Add .ZIP Library".

## Getting started
Navigate to *File > Examples > ProsecCoAP* to get started with some basic examples.

### How to test
#### Verify compile errors and warnings
To quickly verify a successfull build process for multiple boards: 
1. Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/).
2. Ensure the core for the supported boards are installed:
```
arduino-cli core install arduino:avr
arduino-cli core install esp32:esp32
arduino-cli core install esp8266:esp8266 --additional-urls http://arduino.esp8266.com/stable/package_esp8266com_index.json
```
3. Run `make`.

#### Functional tests
The [examples](https://github.com/decadenza/ProsecCoAP/tree/main/examples) need CoAP server libcoap or microcoap server to work. You can, alternatively:
- Use two devices and check serial monitor of each.
- Use one device and a [CoAP tool](https://coap.space/tools.html) on your computer for testing.
  For example, [libcoap](https://github.com/obgm/libcoap) by compiling it yourself or use the example binaries available as Debian package [libcoap3-bin](https://packages.debian.org/stable/libs/libcoap3-bin). In this case, you can test with `coap-client-notls` and `coap-server-notls`.

## Documentation
Documentation is available at: [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).

### Building the documentation
To manually build documentation from the main folder, run:
```
doxygen
```
The documentation will be accesible from `./html/index.html`.

## Contributing
Contributions are welcome. You may contribute by:
- Opening issues to report bugs or ask for missing features.
- Make pull requests.

Code must be documented using [Doxygen](https://www.doxygen.nl/manual/docblocks.html).
Please write code following the [Arduino Style Guide](https://docs.arduino.cc/learn/contributions/arduino-library-style-guide/) as much as possible.

## Release process memo
A new version is released following these steps:
1. Update [library.properties](https://github.com/decadenza/ProsecCoAP/blob/main/library.properties) as required, using a new `vX.Y.Z` tag.
2. Create the corresponding `vX.Y.Z` tag and a new release (for GitHub and Arduino library manager). The logs for the Arduino library manager bot can be checked [here](https://downloads.arduino.cc/libraries/logs/github.com/decadenza/ProsecCoAP/).
3. Publish the new library to PlatformIO, running the following from the main project folder:
```
pio pkg publish .
```

## Credits
This library is a fork of [CoAP-simple-library](https://github.com/hirotakaster/CoAP-simple-library) by Hirotaka Niisato. Credits for all the orginal code go to the original contributors.

We are grateful to the original author for providing a solid, lightweight foundation for CoAP communication on embedded systems. This project maintains the original MIT License and continues the spirit of open-source IoT development.