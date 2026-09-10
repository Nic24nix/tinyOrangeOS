#include "timezone.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static Timezone current_timezone = TZ_UTC;

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int is_leap_year(int year)
{
    if ((year % 400) == 0)
        return 1;

    if ((year % 100) == 0)
        return 0;

    return (year % 4) == 0;
}

static int days_in_month(int year, int month)
{
    static const int days[] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 2 && is_leap_year(year))
        return 29;

    return days[month - 1];
}

static uint8_t bcd_to_bin(uint8_t value)
{
    return (value & 0x0F) +
           ((value >> 4) * 10);
}

static int last_sunday(int year, int month)
{
    int day;
    int weekday;

    /*
     * Zeller simplificado:
     * 0 = domingo
     */
    for (day = days_in_month(year, month);
         day >= 1;
         day--)
    {
        int y = year;
        int m = month;

        if (m < 3)
        {
            m += 12;
            y--;
        }

        weekday =
            (day +
             (13 * (m + 1)) / 5 +
             y +
             y / 4 -
             y / 100 +
             y / 400) % 7;

        /*
         * Fórmula acima:
         * 0 = sábado
         */
        if (weekday == 0)
            return day;
    }

    return 1;
}

static int second_sunday(int year, int month)
{
    int day;

    for (day = 1; day <= 14; day++)
    {
        int y = year;
        int m = month;
        int weekday;

        if (m < 3)
        {
            m += 12;
            y--;
        }

        weekday =
            (day +
             (13 * (m + 1)) / 5 +
             y +
             y / 4 -
             y / 100 +
             y / 400) % 7;

        if (weekday == 1)
            return day;
    }

    return 8;
}

static int first_sunday(int year, int month)
{
    int day;

    for (day = 1; day <= 7; day++)
    {
        int y = year;
        int m = month;
        int weekday;

        if (m < 3)
        {
            m += 12;
            y--;
        }

        weekday =
            (day +
             (13 * (m + 1)) / 5 +
             y +
             y / 4 -
             y / 100 +
             y / 400) % 7;

        if (weekday == 1)
            return day;
    }

    return 1;
}

static void add_minutes(DateTime *dt, int minutes)
{
    int total;

    total =
        dt->hour * 60 +
        dt->minute +
        minutes;

    while (total < 0)
    {
        total += 1440;

        dt->day--;

        if (dt->day < 1)
        {
            dt->month--;

            if (dt->month < 1)
            {
                dt->month = 12;
                dt->year--;
            }

            dt->day =
                days_in_month(
                    dt->year,
                    dt->month
                );
        }
    }

    while (total >= 1440)
    {
        total -= 1440;

        dt->day++;

        if (dt->day >
            days_in_month(
                dt->year,
                dt->month))
        {
            dt->day = 1;
            dt->month++;

            if (dt->month > 12)
            {
                dt->month = 1;
                dt->year++;
            }
        }
    }

    dt->hour = total / 60;
    dt->minute = total % 60;
}

static int european_dst(DateTime *dt)
{
    int start;
    int end;

    if (dt->month < 3 ||
        dt->month > 10)
        return 0;

    start =
        last_sunday(
            dt->year,
            3
        );

    end =
        last_sunday(
            dt->year,
            10
        );

    if (dt->month > 3 &&
        dt->month < 10)
        return 1;

    if (dt->month == 3 &&
        dt->day >= start)
        return 1;

    if (dt->month == 10 &&
        dt->day < end)
        return 1;

    return 0;
}

static int new_york_dst(DateTime *dt)
{
    int start;
    int end;

    if (dt->month < 3 ||
        dt->month > 11)
        return 0;

    start =
        second_sunday(
            dt->year,
            3
        );

    end =
        first_sunday(
            dt->year,
            11
        );

    if (dt->month > 3 &&
        dt->month < 11)
        return 1;

    if (dt->month == 3 &&
        dt->day >= start)
        return 1;

    if (dt->month == 11 &&
        dt->day < end)
        return 1;

    return 0;
}

void timezone_init(void)
{
    current_timezone = TZ_UTC;
}

void timezone_set(Timezone tz)
{
    if (tz <= TZ_SAO_PAULO)
        current_timezone = tz;
}

Timezone timezone_get(void)
{
    return current_timezone;
}

void timezone_get_datetime(DateTime *dt)
{
    uint8_t status_b;
    uint8_t hour;

    /*
     * Esperar o RTC terminar uma atualização.
     */
    while (cmos_read(0x0A) & 0x80)
    {
    }

    status_b = cmos_read(0x0B);

    dt->second = cmos_read(0x00);
    dt->minute = cmos_read(0x02);
    hour        = cmos_read(0x04);
    dt->weekday = cmos_read(0x06);
    dt->day     = cmos_read(0x07);
    dt->month   = cmos_read(0x08);

    dt->year =
        cmos_read(0x09);

    /*
     * O RTC de PC normalmente fornece
     * os dois últimos dígitos.
     */
    dt->year += 2000;

    /*
     * Converter BCD se necessário.
     */
    if (!(status_b & 0x04))
    {
        dt->second =
            bcd_to_bin(dt->second);

        dt->minute =
            bcd_to_bin(dt->minute);

        hour =
            bcd_to_bin(hour);

        dt->day =
            bcd_to_bin(dt->day);

        dt->month =
            bcd_to_bin(dt->month);
    }

    /*
     * Converter 12h -> 24h.
     */
    if (!(status_b & 0x02))
    {
        int pm = hour & 0x80;

        hour &= 0x7F;

        if (pm && hour < 12)
            hour += 12;

        if (!pm && hour == 12)
            hour = 0;
    }

    dt->hour = hour;

    /*
     * RTC assume UTC.
     */
    int offset = 0;

    switch (current_timezone)
    {
        case TZ_MADRID:
            offset = 60;
            break;

        case TZ_NEW_YORK:
            offset = -300;
            break;

        case TZ_SAO_PAULO:
            offset = -180;
            break;

        default:
            offset = 0;
            break;
    }

    if (current_timezone == TZ_LISBON ||
        current_timezone == TZ_LONDON)
    {
        if (european_dst(dt))
            offset += 60;
    }

    if (current_timezone == TZ_MADRID)
    {
        if (european_dst(dt))
            offset += 60;
    }

    if (current_timezone == TZ_NEW_YORK)
    {
        if (new_york_dst(dt))
            offset += 60;
    }

    add_minutes(dt, offset);
}

const char *timezone_get_name(void)
{
    switch (current_timezone)
    {
        case TZ_LISBON:
            return "Europe/Lisbon";

        case TZ_LONDON:
            return "Europe/London";

        case TZ_MADRID:
            return "Europe/Madrid";

        case TZ_NEW_YORK:
            return "America/New_York";

        case TZ_SAO_PAULO:
            return "America/Sao_Paulo";

        default:
            return "UTC";
    }
}

int timezone_get_offset_minutes(void)
{
    int offset = 0;

    switch (current_timezone)
    {
        case TZ_MADRID:
            offset = 60;
            break;

        case TZ_NEW_YORK:
            offset = -300;
            break;

        case TZ_SAO_PAULO:
            offset = -180;
            break;

        default:
            offset = 0;
            break;
    }

    return offset;
}

int timezone_is_dst(void)
{
    DateTime dt;

    timezone_get_datetime(&dt);

    if (current_timezone == TZ_LISBON ||
        current_timezone == TZ_LONDON ||
        current_timezone == TZ_MADRID)
    {
        return european_dst(&dt);
    }

    if (current_timezone == TZ_NEW_YORK)
        return new_york_dst(&dt);

    return 0;
}
