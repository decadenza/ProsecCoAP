/**
 * @file serverRpiPico.ino
 * @brief A CoAP server example specific for Raspberry Pi Pico.
 *
 * Tested on WIZnet W5100S-EVB-Pico. Other boards may require changes.
 *
 * Requirements:
 * - Install "https://github.com/earlephilhower/arduino-pico" (also through the Board Manager).
 * - Select "WIZnet W5100S-EVB-Pico" board.
 *
 * The current light status can be read using:
 * ```
 * coap-client-notls -m get coap://192.168.0.99/light
 * ```
 * and the light can be set ON or OFF, respectively, by using:
 * ```
 * coap-client-notls -m put coap://192.168.0.99/light -e "1"
 * coap-client-notls -m put coap://192.168.0.99/light -e "0"
 * ```
 * Note that on some boards, ON/OFF logic may be inverted.
 *
 */
#include <SPI.h>
#include <W5100lwIP.h>
#include <WiFiUdp.h> // This is the UDP provider for both WiFI and Ethernet.
#include <ProsecCoAP.h>

#define LEDP LED_BUILTIN

// Ethernet instance for W5100S-EVB-Pico board.
Wiznet5100lwIP eth(17, SPI, 21); // Note chip select is **17**.

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress deviceIp(192, 168, 0, 99); // Set your own.
IPAddress dns(192, 168, 0, 99);      // Set your own.
IPAddress gateway(192, 168, 0, 99);  // Set your own.
IPAddress subnet(255, 255, 255, 0);  // Set your own.

// CoAP server path callback.
void callbackLight(Coap::Message &packet, IPAddress ip, uint16_t port);

// CoAP response handler callback.
void callbackResponse(Coap::Message &packet, IPAddress ip, uint16_t port);

// UDP and CoAP instances.
WiFiUDP Udp;
Coap::Node coapNode(Udp);

bool LED_STATUS;

void setup()
{
  Serial.begin(115200);

  // Wait for serial before starting. Optional.
  while (!Serial)
  {
  }

  // Set SPI pins for your specific board.
  // Not needed for W5100S-EVB-Pico.
  // Edit to match your hardware.
  //   SPI.setRX(16);
  //   SPI.setCS(17);
  //   SPI.setSCK(18);
  //   SPI.setTX(19);
  //   SPI.begin();

  eth.config(deviceIp, gateway, subnet, dns);

  while (!eth.begin())
  {
    Serial.println("No wired Ethernet hardware detected. Check pinouts, wiring.");
    delay(1000);
  }

  // LED state.
  pinMode(LEDP, OUTPUT);
  digitalWrite(LEDP, HIGH);
  LED_STATUS = true;

  // Serve the light endpoint linked to callbackLight function.
  coapNode.serve("light", callbackLight);

  // Start coap node.
  coapNode.start();
  Serial.print("Coap node started on ");
  Serial.println(eth.localIP());
}

void loop()
{
  Serial.println(".");
  coapNode.loop();
  delay(1000);
}

// Light control callback.
// The expected payload input is one string character.
//
// A GET request will only return the current value.
// The response will be a string, either "1" or "0".
//
// A PUT request will set the light on if the payload is "1", off otherwise.
void callbackLight(Coap::Message &message, IPAddress ip, uint16_t port)
{
  if (message.getCode() == Coap::MessageCode::PUT)
  {
    Serial.println("Incoming PUT");
    const uint8_t *payload;
    size_t length;
    message.getPayload(payload, length); // You should check for any returned error.
    // Process incoming value, considering first byte only.
    LED_STATUS = (reinterpret_cast<const char *>(payload))[0] == '1' ? HIGH : LOW;
    digitalWrite(LEDP, LED_STATUS);
    // Send a response to a PUT requests.
    Coap::Message response;
    response.intoResponse(message, Coap::MessageCode::CHANGED);
    coapNode.sendMessage(response, ip, port);
  }
  else if (message.getCode() == Coap::MessageCode::GET)
  {
    // Send a piggybacked response to a GET requests.
    Coap::Message response;
    response.intoResponse(message, Coap::MessageCode::CONTENT);
    response.addPayload((const uint8_t *)(LED_STATUS ? "1" : "0"), 1, Coap::ContentFormat::TEXT_PLAIN);
    coapNode.sendMessage(response, ip, port);
  }

  // NOTE: If you cannot reply immediately, the protocol allows to:
  // 1. First, send an empty acknowledgement (to stop retransmission).
  // 2. When data is ready, send a separate response.

  Serial.print("[Light] ");
  Serial.println(LED_STATUS);
}
