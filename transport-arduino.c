#ifdef ARDUINO

#include <string.h>

#include "zhe-config.h"
#include "zhe-assert.h"
#include "transport-arduino.h"

#if defined __APPLE__ || defined __linux /* fake it if not a real Arduino */
static unsigned millis(void) { static unsigned m; return m++; }
static void serial_begin(int baud) { }
static void serial_write(uint8_t octet) { }
static void serial_println(void) { }
static uint8_t serial_read(void) { return 0; }
static int serial_available(void) { return 0; }
static struct {
    void (*begin)(int baud);
    void (*write)(uint8_t octet);
    void (*println)(void);
    uint8_t (*read)(void);
    int (*available)(void);
} Serial = {
    .begin = serial_begin,
    .write = serial_write,
    .println = serial_println,
    .read = serial_read,
    .available = serial_available
};
#endif

/* The arduino client code is not that important to me right now, so just take the FSM from the old code
   for draining the input on startup */
#define STATE_WAITINPUT    0
#define STATE_DRAININPUT   1
#define STATE_OPERATIONAL  2

struct zhe_transport *zhe_arduino_new(void)
{
    uint8_t state = STATE_WAITINPUT;
    zhe_time_t t_state_changed = millis();

    Serial.begin(115200);

    /* Perhaps shouldn't take this time here, but before one calls zhe_init(); for now however, it is safe to do it here */
    while (state != STATE_OPERATIONAL) {
        /* On startup, wait up to 5s for some input, and if some is received, drain
           the input until nothing is received for 1s.  For some reason, garbage
           seems to come in a few seconds after waking up.  */
        zhe_time_t tnow = millis();
        zhe_timediff_t timeout = (state == STATE_WAITINPUT) ? 5000 : 1000;
        if ((zhe_timediff_t)(tnow - t_state_changed) >= timeout) {
            state = STATE_OPERATIONAL;
            t_state_changed = tnow;
        } else if (Serial.available()) {
            (void)Serial.read();
            state = STATE_DRAININPUT;
            t_state_changed = tnow;
        }
    }

    return (struct zhe_transport *)&Serial;
}

static size_t arduino_addr2string(const struct zhe_transport *tp, char * restrict str, size_t size, const zhe_address_t * restrict addr)
{
    zhe_assert(size > 0);
    str[0] = 0;
    return 0;
}

int zhe_arduino_string2addr(const struct zhe_transport *tp, struct zhe_address * restrict addr, const char * restrict str)
{
    memset(addr, 0, sizeof(*addr));
    return 1;
}

static ssize_t arduino_send(struct zhe_transport * restrict tp, const void * restrict buf, size_t size, const zhe_address_t * restrict dst)
{
    size_t i;
    zhe_assert(size <= TRANSPORT_MTU);
#if TRANSPORT_MODE == TRANSPORT_PACKET
    Serial.write(0xff);
    Serial.write(0x55);
#if TRANSPORT_MTU > 255
#error "PACKET mode currently has TRANSPORT_MTU limited to 255 because it writes the length as a single byte"
#endif
    Serial.write((uint8_t)size);
#endif
    for (i = 0; i != size; i++) {
        Serial.write(((uint8_t *)buf)[i]);
    }
#if TRANSPORT_MODE == TRANSPORT_PACKET
    Serial.println();
#endif
    return (ssize_t)size;
}

#if TRANSPORT_MODE == TRANSPORT_PACKET
static int read_serial(void)
{
    static uint8_t serst = 0;
    if (Serial.available()) {
        uint8_t c = Serial.read();
        switch (serst) {
            case 0:
                serst = (c == 0xff) ? 255 : 0;
                break;
            case 255:
                serst = (c == 0x55) ? 254 : 0;
                break;
            case 254:
                if (c == 0 || c > MTU) {
                    serst = 0; /* ERROR + blinkenlights? */
                } else {
                    serst = c;
                    inp = 0;
                }
                break;
            default:
                inbuf[inp++] = c;
                if (--serst == 0) {
                    return 1;
                }
                break;
        }
    }
    return 0;
}
#endif

int zhe_arduino_recv(struct zhe_transport * restrict tp, void * restrict buf, size_t size, zhe_address_t * restrict src)
{
#if TRANSPORT_MODE == TRANSPORT_STREAM
    assert(size <= INT_MAX);
    size_t n = 0;
    while (n < size && Serial.available()) {
        ((uint8_t *) buf)[n++] = Serial.read();
    }
    return (int)n;
#else
#error "couldn't be bothered to integrate Arduino code fully"
#endif
}

static int arduino_addr_eq(const struct zhe_address *a, const struct zhe_address *b)
{
    return 1;
}

int zhe_arduino_wait(const struct zhe_transport * restrict tp, zhe_timediff_t timeout)
{
    return Serial.available();
}

zhe_transport_ops_t transport_arduino = {
    .addr2string = arduino_addr2string,
    .addr_eq = arduino_addr_eq,
    .send = arduino_send
};

#endif
