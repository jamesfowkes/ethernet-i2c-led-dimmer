#include <stdint.h>
#include <string.h>

#include <Wire.h>

#include "raat.hpp"
#include "raat-buffer.hpp"

#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"
#include "raat-task.hpp"

#include "http-get-server.hpp"

static const uint8_t DIMMER_I2C_ADDRESS = 0x27;
static const uint8_t CHANNEL_1_REGISTER = 0x80;

static HTTPGetServer s_server(true);

static uint8_t s_currentValues[4];
static bool s_updateFlags[4];

static const raat_params_struct * s_pParams;

static const uint8_t flickerValues[64] = {100};

static void etherDelay(unsigned long msdelay)
{
    unsigned long now = millis();

    while ((millis() - now) < msdelay)
    {
        raat_handle_any_pending_commands();
        raat_service_timer();
    }
}

static void setChannel(const uint8_t channel, const uint8_t value, bool bLog=true)
{
    if (value <= 100)
    {
        s_currentValues[channel] = value;
        if (bLog) { raat_logln_P(LOG_APP, PSTR("Setting channel %u to %u"), channel, s_currentValues[channel]); }
        Wire.beginTransmission(DIMMER_I2C_ADDRESS);
        Wire.write(channel + CHANNEL_1_REGISTER);
        Wire.write(100-value);
        Wire.endTransmission();
    }
    else
    {
        if (bLog) { raat_logln_P(LOG_APP, PSTR("Value %u out of range"), value); }
    }
}

static void dimmer_task_fn(RAATTask& task, void * pTaskData)
{
    (void)task;
    (void)pTaskData;
    
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        if (s_updateFlags[channel])
        {
            s_updateFlags[channel] = false;
            setChannel(channel, s_currentValues[channel]);
        }
    }
}
static RAATTask s_dimmer_task(100, dimmer_task_fn, NULL);

static void send_standard_erm_response()
{
    s_server.set_response_code_P(PSTR("200 OK"));
    s_server.set_header_P(PSTR("Access-Control-Allow-Origin"), PSTR("*"));
    s_server.finish_headers();
}

static void set_dimmer_from_string(char const * const pValue, const uint8_t channel)
{
    int32_t parsed;
    if (raat_parse_single_numeric(pValue, parsed, NULL))
    {
        if ((parsed >= 0) && (parsed <= 100))
        {
            if (s_currentValues[channel] != parsed)
            {
                raat_logln_P(LOG_APP, PSTR("Setting dimmer %u to %d"), channel, parsed);
                s_currentValues[channel] = parsed;
                s_updateFlags[channel] = true;
            }
            else
            {
                raat_logln_P(LOG_APP, PSTR("Dimmer %u already at %d"), channel, parsed);
            }
        }
        else
        {
            raat_logln_P(LOG_APP, PSTR("Value %d out of range"), parsed);
        }
    }
    else
    {
        raat_logln_P(LOG_APP, PSTR("Could not parse %s"), pValue);
    }
}

static void set_all_dimmers(char const * const url)
{
    set_dimmer_from_string(&url[5], 0);
    set_dimmer_from_string(&url[5], 1);
    set_dimmer_from_string(&url[5], 2);
    set_dimmer_from_string(&url[5], 3);
    send_standard_erm_response();
}

static void set_dimmer1(char const * const url)
{
    set_dimmer_from_string(&url[9], 0);
    send_standard_erm_response();
}

static void set_dimmer2(char const * const url)
{
    set_dimmer_from_string(&url[9], 1);
    send_standard_erm_response();
}

static void set_dimmer3(char const * const url)
{
    set_dimmer_from_string(&url[9], 2);
    send_standard_erm_response();
}

static void set_dimmer4(char const * const url)
{
    set_dimmer_from_string(&url[9], 3);
    send_standard_erm_response();
}

static void save_values(char const * const url)
{
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        s_pParams->pDimmer[channel]->set(s_currentValues[channel]);
    }
    if (url)
    {
        send_standard_erm_response();
    }
}

static void restore_values(char const * const url)
{
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        s_currentValues[channel] = s_pParams->pDimmer[channel]->get();
        s_updateFlags[channel] = true;
    }
    if (url)
    {
        send_standard_erm_response();
    }
}

static void flicker_leds(char const * const url)
{
    
    if (url)
    {
        send_standard_erm_response();
    }

    uint8_t i = 0;
    while (flickerValues[i] != 100)
    {
        setChannel(0, flickerValues[i]);
        etherDelay(80+random(0, 70));
        i++;
    }
    setChannel(0, 100);
}

static void get_dimmer_value(char const * const url)
{
    char dimmer_value[4];
    if (url)
    {
        s_server.set_response_code_P(PSTR("200 OK"));
        s_server.finish_headers();

        if ((url[5] >= '0') && url[5] <= '3')
        {
            sprintf(dimmer_value, "%u", s_currentValues[0]);
            s_server.add_body(dimmer_value);
        }
    }
}

static const char DIMMER1_URL[] PROGMEM = "/dimmer1";
static const char DIMMER2_URL[] PROGMEM = "/dimmer2";
static const char DIMMER3_URL[] PROGMEM = "/dimmer3";
static const char DIMMER4_URL[] PROGMEM = "/dimmer4";
static const char ALL_DIMMERS_URL[] PROGMEM = "/all";
static const char SAVE_URL[] PROGMEM = "/save";
static const char RESTORE_URL[] PROGMEM = "/restore";
static const char FLICKER_URL[] PROGMEM = "/flicker";
static const char QUERY_URL[] PROGMEM = "/get";

static http_get_handler s_handlers[] = 
{
    {DIMMER1_URL, set_dimmer1},
    {DIMMER2_URL, set_dimmer2},
    {DIMMER3_URL, set_dimmer3},
    {DIMMER4_URL, set_dimmer4},
    {ALL_DIMMERS_URL, set_all_dimmers},
    {SAVE_URL, save_values},
    {RESTORE_URL, restore_values},
    {FLICKER_URL, flicker_leds},
    {QUERY_URL, get_dimmer_value},
    {"", NULL}
};

void ethernet_packet_handler(char * req)
{
    s_server.handle_req(s_handlers, req);
}

char * ethernet_response_provider()
{
    return s_server.get_response();
}

void raat_custom_setup(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)devices;

    randomSeed(analogRead(A0));

    Wire.begin();

    s_pParams = &params;
    restore_values(NULL);
    raat_logln_P(LOG_APP, PSTR("Restore values: %d,%d,%d,%d"),
        s_currentValues[0], s_currentValues[1],
        s_currentValues[2], s_currentValues[3]
    );

    for (uint8_t channel = 0; channel < 4; channel++)
    {
        s_updateFlags[channel] = true;
    }
}

void raat_custom_loop(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)devices; (void)params;

    if (devices.pToggle_Input->check_low_and_clear())
    {
        if (s_currentValues[0] || s_currentValues[1] || s_currentValues[2] || s_currentValues[3])
        {
            s_currentValues[0] = 0;
            s_updateFlags[0] = true;
            s_currentValues[1] = 0;
            s_updateFlags[1] = true;
            s_currentValues[2] = 0;
            s_updateFlags[2] = true;
            s_currentValues[3] = 0;
            s_updateFlags[3] = true;
        }
        else
        {
            s_currentValues[0] = 100;
            s_updateFlags[0] = true;
            s_currentValues[1] = 100;
            s_updateFlags[1] = true;
            s_currentValues[2] = 100;
            s_updateFlags[2] = true;
            s_currentValues[3] = 100;
            s_updateFlags[3] = true;
        }
    }
    s_dimmer_task.run();
}