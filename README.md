# ProsecCoAP - A CoAP client and server library for Arduino.

<a href="http://coap.space/" target=_blank>CoAP</a> simple server/client library for Arduino IDE/PlatformIO, ESP32, ESP8266.

Documentation is available at [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).

## Details
This library is a *partial* implementation of CoAP protocol ([RFC-7252](https://datatracker.ietf.org/doc/html/rfc7252)) supporting:
- request/respose semantics,
- observe pattern.

This library is a fork of [CoAP-simple-library](https://github.com/hirotakaster/CoAP-simple-library) and aims at providing:
- Clear documentation and ease of use.
- A closer implementation of the ([RFC-7252](https://datatracker.ietf.org/doc/html/rfc7252)).

## How to install
### From Arduino IDE Library Manager
1. Open the *Sketch* menu in the IDE.
2. Navigate to *Include Library > Manage Libraries*.
3. Search for "ProsecCoAP" and install.

### Manual

1. Download this source code branch as a zip file.
2. In the Arduino IDE, navigate to *Sketch > Include Library > Add .ZIP Library*. At the top of the drop down list, select the option to "Add .ZIP Library".

## Getting started
Navigate to *File > Examples > ProsecCoAP* to get started with some basic examples.

### How to test
The [examples](./examples) need CoAP server libcoap or microcoap server to work. You can, alternatively:
- Use two devices and use serial monitor.
- Use one device and a [CoAP tool](https://coap.space/tools.html) on your computer for testing.
- Use [libcoap](https://github.com/obgm/libcoap) by compiling the examples yourself or use the example binaries available as Debian package [libcoap3-bin](https://packages.debian.org/stable/libs/libcoap3-bin). In this case, you can test with `coap-client-notls` and `coap-server-notls`.

## Documentation
Documentation is available at: [https://decadenza.github.io/ProsecCoAP/](https://decadenza.github.io/ProsecCoAP/).

### Building the documentation
To manually build documentation from the main folder, run:
```
doxygen
```
The documentation will accesible from `./html/index.html`.

## Particle Photon, Core compatible
Check <a href="https://github.com/hirotakaster/CoAP">this</a> version of the library for Particle Photon, Core compatibility.

## Release process
A new version is released following these steps:
1. Update [library.properties](library.properties) as required, using a new `vX.Y.Z` tag.
2. Create the corresponding `vX.Y.Z` tag and a new release (for GitHub and Arduino library manager).
3. Publish the new library to PlatformIO, running the following from the main project folder:
```
pio pkg publish .
```

## Credits
This library is a fork of [CoAP-simple-library](https://github.com/hirotakaster/CoAP-simple-library) by Hirotaka Niisato. Credits for all the orginal code go to the original contributors.

We are grateful to the original author for providing a solid, lightweight foundation for CoAP communication on embedded systems. This project maintains the original MIT License and continues the spirit of open-source IoT development.