#include <stdint.h>
#include <string.h>

#include <Wire.h>

#include "raat.hpp"
#include "raat-buffer.hpp"

#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"

#include "http-get-server.hpp"

static const uint8_t DIMMER_I2C_ADDRESS = 0x27;
static const uint8_t CHANNEL_1_REGISTER = 0x80;

static HTTPGetServer s_server(true);

static uint8_t s_currentValues[4];
static raat_params_struct * s_pParams;

static void setChannel(uint8_t channel, uint8_t value)
{
    if (value <= 100)
    {
        Wire.beginTransmission(DIMMER_I2C_ADDRESS);
        Wire.write(channel + CHANNEL_1_REGISTER);
        Wire.write(value);
        Wire.endTransmission();
    }
}

static void dimmer_task_fn(RAATOneShotTask& pThisTask, void * pTaskData)
{
    (void)pThisTask;
    (void)pTaskData;
    
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        setChannel(channel, s_currentValues[channel]);
    }
}
static RAATOneShotTask s_dimmer_task(100, dimmer_task_fn, NULL);

static void send_standard_erm_response()
{
    s_server.set_response_code_P(PSTR("200 OK"));
    s_server.set_header_P(PSTR("Access-Control-Allow-Origin"), PSTR("*"));
    s_server.finish_headers();
}

static void set_dimmer(char const * const url, uint8_t channel)
{
    char * pValue = url[9];
    int32_t parsed;
    if (raat_parse_single_numeric(pValue, parsed, NULL))
    {
        if ((parsed >= 0) && (parsed <= 100))
        {
            s_currentValues[channel] = parsed;
        }
    }
    send_standard_erm_response();  
}

static void set_dimmer1(char const * const url)
{
    set_dimmer(url, 0);
}

static void set_dimmer2(char const * const url)
{
    set_dimmer(url, 1);
}

static void set_dimmer3(char const * const url)
{
    set_dimmer(url, 2);
}

static void set_dimmer4(char const * const url)
{
    set_dimmer(url, 3);
}

static void save_values(char const * const url)
{
    (void)url;
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        s_pParams->pDimmer[channel]->set(s_currentValues[channel]);
    }
}

static void restore_values(char const * const url)
{
    (void)url;
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        s_currentValues[channel] = s_pParams->pDimmer[channel]->get();
    }
}

static http_get_handler s_handlers[] = 
{
    {"/dimmer1", set_dimmer1},
    {"/dimmer2", set_dimmer2},
    {"/dimmer3", set_dimmer3},
    {"/dimmer4", set_dimmer4},
    {"/save", save_values},
    {"/restore", restore_values},
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
    s_pParams = &params;
    restore_values(NULL);
}

void raat_custom_loop(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)devices; (void)params;
    s_dimmer_task.run();
}