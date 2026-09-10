#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <stdint.h>

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} DateTime;

typedef enum
{
    TZ_UTC = 0,
    TZ_LISBON,
    TZ_LONDON,
    TZ_MADRID,
    TZ_NEW_YORK,
    TZ_SAO_PAULO
} Timezone;

void timezone_init(void);
void timezone_set(Timezone tz);
Timezone timezone_get(void);

void timezone_get_datetime(DateTime *dt);

const char *timezone_get_name(void);
int timezone_get_offset_minutes(void);
int timezone_is_dst(void);

#endif
