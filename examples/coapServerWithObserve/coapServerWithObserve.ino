/*
 * A CoAP server with Observe functionality.
 *
 * Author: Pasquale Lafiosca (2025)
 *
 * To test this example with the coap-client tool from libcoap:
 * ```
 * coap-client-notls -m get -s 5 coap://192.168.0.1/subscribe
 * ```
 * Note that at exit the unsubscribe call will be sent
 * automatically by coap-client-notls.
 */
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

// --- DEBUG MACROS ---
// Set to 1 to enable Serial output for debugging. Set to 0 to disable all Serial calls (no-ops).
#define ENABLE_SERIAL_DEBUG 1

#if ENABLE_SERIAL_DEBUG
#define SERIAL_BEGIN(baud) Serial.begin(baud)
#define SERIAL_WHILE_WAIT \
    while (!Serial)       \
        ;
#define SERIAL_PRINT(x) Serial.print(x)
#define SERIAL_PRINT_HEX(x) Serial.print(x, HEX)
#define SERIAL_PRINTLN(x) Serial.println(x)
#define SERIAL_WRITE(x) Serial.write(x)
#else
#define SERIAL_BEGIN(baud)
#define SERIAL_WHILE_WAIT
#define SERIAL_PRINT(x)
#define SERIAL_PRINT_HEX(x)
#define SERIAL_PRINTLN(x)
#define SERIAL_WRITE(x)
#endif

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// Using a sequential identifier. IP will be based on this.
#define DEVICE_ID 1

byte mac[] = {0xBE, 0xEF, 0xBE, 0xEF, 0x00, DEVICE_ID}; // Define the MAC address, this must be unique.
IPAddress ip(192, 168, 0, DEVICE_ID);                   // This device IP.

// Declarations.
void endpointSubscribe(CoapPacket &packet, IPAddress ip, uint16_t port);

void setup()
{

    // Initialize serial and wait for port to open.
    SERIAL_BEGIN(115200);
    SERIAL_WHILE_WAIT;
    SERIAL_PRINTLN("Booting...");
    SERIAL_PRINT("Configuring Ethernet...");
    Ethernet.begin(mac, ip);
    // Check for hardware issues
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        SERIAL_PRINTLN("Ethernet shield/hardware not found. Check connections!");
        return;
    }
    SERIAL_PRINTLN("OK");

    coap.server(endpointSubscribe, "subscribe");

    // Start coap server.
    coap.start();

    SERIAL_PRINT("Server listening on ");
    SERIAL_PRINTLN(Ethernet.localIP());

    SERIAL_PRINTLN("Initialisation completed");
}

void endpointSubscribe(CoapPacket &packet, IPAddress ip, uint16_t port)
{
    COAP_OBSERVE_VALUE observeValue = packet.getObserveValue();

    if (observeValue == COAP_OBSERVE_VALUE_REGISTER)
    {
        // Add a new observer in the table.
        CoapObserver *observer = NULL;
        int rc = coap.addObserver(&observer, "subscribe", ip, port, packet.token, packet.tokenLength);
        if (rc != 0 || observer == NULL)
        {
            coap.sendResponse(ip, port, packet, COAP_SERVICE_UNAVAILABLE, "busy", strlen("busy"), COAP_TEXT_PLAIN);
            SERIAL_PRINTLN("Observer could not be added!");
            return;
        }
        else
        {
            // The loop will return the current representation of the resource
            // (this also acts as confirmation, see https://datatracker.ietf.org/doc/html/rfc7641#section-4.1).
            SERIAL_PRINTLN("Subscribed!");
        }
    }
    else if (observeValue == COAP_OBSERVE_VALUE_DEREGISTER)
    {
        coap.removeObserver("subscribe", ip, port, packet.token, packet.tokenLength);
        coap.sendResponse(ip, port, packet, COAP_CONTENT, "unsubscribed", strlen("unsubscribed"), COAP_TEXT_PLAIN);
        SERIAL_PRINTLN("Unsubscribed!");
    }
    else
    {
        SERIAL_PRINT("Missing/invalid observe value: ");
        SERIAL_PRINTLN(observeValue);
    }
}

void loop()
{
    coap.loop(); // Process coap requests.

    sendNotification();
}

// Demo notification with gibberish data.
void sendNotification()
{
    char payload[] = "The answer is 42\n";
    size_t payloadLength = strlen(payload);

    if (coap.notifyObservers("subscribe", payload, payloadLength, COAP_TEXT_PLAIN) > 0)
    {
        SERIAL_PRINTLN("Notified!");
    }
}
