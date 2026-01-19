/*
 * A CoAP server with Observe functionality.
 *
 * Author: Pasquale Lafiosca (2025)
 *
 * To test this example with the coap-client tool from libcoap:
 * ```
 * coap-client-notls -m get -s 60 coap://192.168.0.1/subscribe
 * ```
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
// Define the macros as empty (no-op) when debugging is disabled
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

// Using a sequential identifier. IP and MAC will be based on this.
#define DEVICE_ID 1

byte mac[] = {0xBE, 0xEF, 0xBE, 0xEF, 0x00, DEVICE_ID}; // Define the MAC address, this must be unique.
IPAddress ip(192, 168, 0, DEVICE_ID);                   // This device IP.

// Declarations.
void endpoint_subscribe(CoapPacket &packet, IPAddress ip, int port);

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

    SERIAL_PRINTLN("Setup echo endpoint");
    coap.server(endpoint_subscribe, "subscribe");

    // start coap server/client
    coap.start();

    SERIAL_PRINTLN("Server OK");
    SERIAL_PRINT("Server listening on ");
    SERIAL_PRINTLN(Ethernet.localIP());

    SERIAL_PRINTLN("Initialisation completed");
}

void endpoint_subscribe(CoapPacket &packet, IPAddress ip, int port)
{

    COAP_OBSERVE_VALUE observe_value;
    if (packet.getObserveValue(observe_value))
    {
        if (observe_value == COAP_OBSERVE_VALUE_REGISTER)
        {
            // Add a new observer in the table.
            Observer *observer = NULL;
            int rc = coap.addObserver(&observer, "subscribe", ip, port, packet.token, packet.tokenLength);
            if (rc != 0 || observer == NULL)
            {
                coap.sendResponse(ip, port, packet.messageId, "busy", strlen("busy"), COAP_SERVICE_UNAVAILABLE, COAP_TEXT_PLAIN, packet.token, packet.tokenLength);
                SERIAL_PRINTLN("Observer could not be added!");
                return;
            }
            else
            {
                // First confirm the subscription to the client, with no payload.
                coap.sendObserveRegisterConfirmation(observer, packet.messageId);
                SERIAL_PRINTLN("Subscribed!");
            }
        }
        else if (observe_value == COAP_OBSERVE_VALUE_CANCEL)
        {
            coap.removeObserver("subscribe", ip, port, packet.token, packet.tokenLength);
            coap.sendResponse(ip, port, packet.messageId, "unsubscribed", strlen("unsubscribed"), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenLength);
            SERIAL_PRINTLN("Unsubscribed!");
        }
        // Else ignore.
    }
}

void loop()
{
    coap.loop(); // Keeps connection alive.

    send_notification();
}

// Demo notification with gibberish data.
void send_notification()
{
    char payload[6]; // Max 5 digits for uint16_t + 1 for null terminator '\0'
    sprintf(payload, "%u", 42);
    int payload_len = strlen(payload);

    if (coap.notifyObservers("subscribe", payload, payload_len, COAP_APPLICATION_OCTET_STREAM) > 0)
    {
        SERIAL_PRINTLN("Notified!");
    }
}