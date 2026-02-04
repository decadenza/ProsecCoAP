/**
 * Examples of advanced message building.
 */
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress deviceIp(192, 168, 0, 99);                   // Set your own.
IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.

// UDP and CoAP class
EthernetUDP Udp;
// Coap coap(Udp);

void setup()
{
  Serial.begin(115200);

  Ethernet.begin(mac, deviceIp);
  Serial.print("IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();
}

void loop()
{
  Serial.println("[Building new message]");
  // Build a GET request message to transmit.
  Coap::Message msg(Coap::MessageType::Con, Coap::MessageCode::Get); // Initialise a new CoAP confirmable message, as GET request.

  // Optionally, add a token of 4 bytes. Use getToken() to retrieve it later.
  msg.addToken(4);
  Serial.print("Added token of length: ");
  Serial.println(msg.getTokenLength());

  // You can add a payload.
  Coap::ErrorCode err = msg.addPayload((const uint8_t *)(&"PAYLOAD_DATA"), 12, Coap::ContentFormat::TextPlain);
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding payload: ");
    Serial.println((int8_t)err);
  }

  err = msg.addHost(destinationIp); // Uri-Host is optional (often not necessary, if destination host is the same IP).
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  err = msg.addPort(COAP_DEFAULT_PORT); // Optional. If not present will default to the default port anyway.
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  // Add Uri-Path options using the helper method.
  err = msg.addPath("sensors/temp");
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  // You may add advanced CoAP options using the constructor...
  char value[] = "this is 15 long";
  Coap::Option newOption(Coap::OptionNumber::LocationPath, (const uint8_t *)value, strlen(value));
  err = msg.addOption(newOption);
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }
  // ...or passing the values directly.
  err = msg.addOption(Coap::OptionNumber::LocationPath, (const uint8_t *)value, strlen(value));
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  /**
   * PRO-TIP: The library supports building a message in any order.
   * However, given the structure of the CoAP message, some memmove calls
   * will be necessary.
   *
   * It is slightly more efficient to perform operations in this order:
   * 1. Instantiate the message with type and code.
   * 2. Add a token.
   * 3. Add options from the option with lower OptionNumber to the higher one.
   * 4. Lastly, add a payload.
   */

  // Read the current options from the message using an iterator.
  Coap::OptionIterator optIterator = msg.getOptionIterator();
  Coap::Option opt;
  while ((err = optIterator.next(opt)) == Coap::ErrorCode::None)
  {
    Serial.print("Found option: ");
    Serial.print((int8_t)opt.number);
    Serial.print(", length: ");
    Serial.println((int8_t)opt.length);
  }
  if (err != Coap::ErrorCode::NotFound)
  {
    Serial.print("Error while reading options: ");
    Serial.println((int8_t)err);
  }

  // Read the payload from the message.
  const uint8_t *payload;
  size_t payloadLength;
  err = msg.getPayload(payload, payloadLength);
  if (err != Coap::ErrorCode::None)
  {
    Serial.print("Error while reading payload: ");
    Serial.println((int8_t)err);
  }
  else
  {
    const char *payloadStr = reinterpret_cast<const char *>(payload);
    Serial.print("Payload found: ");
    Serial.println(payloadStr);
  }
  delay(1000);
}
