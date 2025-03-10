#include <stdint.h>
#include <string.h>

#include <Wire.h>

#include "raat.hpp"
#include "raat-buffer.hpp"

#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"
#include "raat-task.hpp"

#include "http-get-server.hpp"

#include "flicker.hpp"

static const uint8_t DIMMER_I2C_ADDRESS = 0x27;
static const uint8_t CHANNEL_1_REGISTER = 0x80;

static HTTPGetServer s_server(NULL);

static uint8_t s_currentValues[4];
static bool s_updateFlags[4];

static const raat_params_struct * s_pParams;

static const uint8_t s_flicker_values[64] = {100};

static void etherDelay(unsigned long msdelay)
{
    unsigned long now = millis();

    while ((millis() - now) < msdelay)
    {
        raat_handle_any_pending_commands();
        raat_service_timer();
    }
}

void setChannel(const uint8_t channel, const uint8_t value, bool bLog)
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

static void set_all_dimmers(char const * const url, char const * const additional)
{
    set_dimmer_from_string(additional, 0);
    set_dimmer_from_string(additional, 1);
    set_dimmer_from_string(additional, 2);
    set_dimmer_from_string(additional, 3);
    send_standard_erm_response();
}

static void set_dimmer1(char const * const url, char const * const additional)
{
    set_dimmer_from_string(additional, 0);
    send_standard_erm_response();
}

static void set_dimmer2(char const * const url, char const * const additional)
{
    set_dimmer_from_string(additional, 1);
    send_standard_erm_response();
}

static void set_dimmer3(char const * const url, char const * const additional)
{
    set_dimmer_from_string(additional, 2);
    send_standard_erm_response();
}

static void set_dimmer4(char const * const url, char const * const additional)
{
    set_dimmer_from_string(additional, 3);
    send_standard_erm_response();
}

static void save_values(char const * const url, char const * const additional)
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

static void restore_values(char const * const url, char const * const additional)
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

static void flicker_leds(char const * const url, char const * const additional)
{
    uint8_t channels[4] = {NO_CHANNEL, NO_CHANNEL, NO_CHANNEL, NO_CHANNEL};

    for (uint8_t i=0; i<4; i++)
    {
        char this_channel = additional[0];

        if (inrange<char>(this_channel, '1', '4'))
        {
            channels[i] = this_channel - '1';
        }
        else
        {
            break;
        }
    }

    raat_logln_P(LOG_APP, PSTR("Flickering channels, %u, %u,  %u,  %u"),
        channels[0], channels[1], channels[2], channels[3]);
    
    if (url)
    {
        send_standard_erm_response();
    }

    flicker_start(channels);
}

static void get_dimmer_value(char const * const url, char const * const additional)
{
    char dimmer_value[4];
    if (url)
    {
        s_server.set_response_code_P(PSTR("200 OK"));
        s_server.finish_headers();

        if ((additional[0] >= '0') && additional[0] <= '3')
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

    flicker_setup(params.pFlickerSettings);

    restore_values(NULL, NULL);
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
    flicker_run();
}