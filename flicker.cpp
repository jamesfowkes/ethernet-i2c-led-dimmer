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
    bool more_values_follow;
    if (raat_parse_single_numeric(s_next_value, parsed, &pEndOfThisValue))
    {
        if (inrange<int32_t>(parsed, 0, 100))
        {
            for (uint8_t channel_index=0; channel_index<4; channel_index++)
            {
                if (s_channels[channel_index] != NO_CHANNEL)
                {
                    setChannel(s_channels[channel_index], (uint8_t)parsed);
                }
                else
                {
                    return;
                }
            }
        }
        
        more_values_follow = (*pEndOfThisValue == ',');
        if (more_values_follow)
        {
            s_next_value++;
        }
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
    if (s_bFlickering) { return; }
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
