#include <Arduino.h>

#include <stdint.h>
#include <string.h>

#include "raat.hpp"
#include "string-param.hpp"
#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"

#include "flicker.hpp"

static StringParam * s_pValues;
static bool s_bFlickering = false;

static char const * s_next_value;
static uint8_t s_channels[4];

static bool set_next_value(void)
{
    char * pEndOfThisValue;
    int32_t parsed;
    bool more_values_follow = false;
    if (raat_parse_single_numeric(s_next_value, parsed, &pEndOfThisValue))
    {
        if (inrange<int32_t>(parsed, 0, 100))
        {
            for (uint8_t channel_index=0; channel_index<4; channel_index++)
            {
                if (s_channels[channel_index] != NO_CHANNEL)
                {
                    setChannel(s_channels[channel_index], (uint8_t)parsed, false);
                }
            }
        }
        
        more_values_follow = (*pEndOfThisValue == ',');
        if (more_values_follow)
        {
            s_next_value = pEndOfThisValue+1;
        }
        else
        {
            raat_logln_P(LOG_APP, PSTR("Flicker sequence complete!"));
        }
    }
    else
    {
        uint8_t index = s_next_value - s_pValues->get();
        raat_logln_P(LOG_APP, PSTR("Could not parse value at index %u"), index);
    }
    return more_values_follow;
}

static void flicker_task_fn(RAATOneShotTask& ThisTask, void * pTaskData)
{
    (void)pTaskData;

    bool continue_task = set_next_value();

    if (continue_task)
    {
        ThisTask.start(80+random(0, 70));
    }
    else
    {
        s_bFlickering = false;
    }
}
static RAATOneShotTask s_flicker_task(80, flicker_task_fn, NULL);

void flicker_setup(StringParam * pValues)
{
    s_pValues = pValues;
}

void flicker_start(uint8_t * channels)
{
    if (s_bFlickering)
    {
        raat_logln_P(LOG_APP, PSTR("Flicker already active!"));        
        return;
    }
    if (!channels) { return; }

    s_bFlickering = true;
    s_next_value = s_pValues->get();
    memcpy(s_channels, channels, 4);

    set_next_value();
    s_flicker_task.start(80+random(0, 70));
}

void flicker_run(void)
{
    s_flicker_task.run();
}
