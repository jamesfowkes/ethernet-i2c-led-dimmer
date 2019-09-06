#ifndef _FLICKER_H_
#define _FLICKER_H_

#define NO_CHANNEL (0xff)

void setChannel(const uint8_t channel, const uint8_t value, bool bLog=true);

void flicker_setup(StringParam * pValues);
void flicker_start(uint8_t * channels);
void flicker_run(void);

#endif
