#include <test.h>

#include <date_parser.h>

static void test_date(void)
{
    time_t t;

    assert(StringToTime("21 September 2009", &t) == 0);
    assert(t == 1253491200);
    assert(StringToTime("21 September", &t) == 0);
    assert(t == 1789948800);

    assert(StringToTime("21-September-2009", &t) == 0);
    assert(t == 1253491200);
    assert(StringToTime("21-September", &t) == 0);
    assert(t == 1789948800);
    assert(StringToTime("2009-09-21", &t) == 0);
    assert(t == 1253491200);

    assert(StringToTime("09/21", &t) == 0);
    assert(t == 1789948800);
    assert(StringToTime("09/21/2009", &t) == 0);
    assert(t == 1253491200);

    assert(StringToTime("21.09.2009", &t) == 0);
    assert(t == 1253491200);
    assert(StringToTime("21.09.", &t) == 0);

    assert(StringToTime("September 21", &t) == 0);
    assert(t == 1789948800);
    assert(StringToTime("September 21, 2009", &t) == 0);
    assert(t == 1253491200);
    assert(StringToTime("September 21 2009", &t) == 0);
    assert(t == 1253491200);

    assert(StringToTime("September 21 09", &t) == 0);
    assert(t == 1253491200);
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) \
    && !defined(__NetBSD__)
    assert(StringToTime("September 21 0009", &t) == 0);
    assert(t == -61860412800);
#endif
}

static void SetDate(struct tm *tm)
{
    tm->tm_year = 126;
    tm->tm_mon = 6;
    tm->tm_mday = 5;

    tm->tm_hour = 23;
    tm->tm_min = 19;
    tm->tm_sec = 0;

    tm->tm_wday = 0;
}

static void test_time(void)
{
    struct tm mydate;
    SetDate(&mydate);

    time_t t;
    assert(StringToTimeFrom("10:10", &t, &mydate) == 0);
    assert(t == 1783246200);
    assert(StringToTimeFrom("10:10:10", &t, &mydate) == 0);
    assert(t == 1783246210);
    assert(StringToTimeFrom("10:10:10.00001", &t, &mydate) == 0);
    assert(t == 1783246210);
    assert(StringToTimeFrom("10:10 am", &t, &mydate) == 0);
    assert(t == 1783246200);
    assert(StringToTimeFrom("10:10 p.m.", &t, &mydate) == 0);
    assert(t == 1783289400);
}

static void test_timezone(void)
{
    struct tm mydate;
    SetDate(&mydate);

    time_t t;
    assert(StringToTimeFrom("Z", &t, &mydate) == 0);
    assert(t == 1783209600);
    assert(StringToTimeFrom("+10:00", &t, &mydate) == 0);
    assert(t == 1783173600);
    assert(StringToTimeFrom("-10:10", &t, &mydate) == 0);
    assert(t == 1783246200);
    assert(StringToTimeFrom("UTC+10:10", &t, &mydate) == 0);
    assert(t == 1783173000);
    assert(StringToTimeFrom("UTC+10", &t, &mydate) == 0);
    assert(t == 1783173600);
    assert(StringToTimeFrom("+10", &t, &mydate) == 0);
    assert(t == 1783173600);
}

static void test_relative_time(void)
{
    struct tm mydate;
    SetDate(&mydate);

    time_t t;
    assert(StringToTimeFrom("year year year", &t, &mydate) == 0);
    assert(t == 1877987940);
    assert(StringToTimeFrom("this year", &t, &mydate) == 0);
    assert(t == 1783293540);
    assert(StringToTimeFrom("year", &t, &mydate) == 0);
    assert(t == 1814829540);
    assert(StringToTimeFrom("-10 years month 2 days ago second", &t, &mydate) == 0);
    assert(t == 1470266341);
    assert(StringToTimeFrom("today", &t, &mydate) == 0);
    assert(t == 1783293540);

}

static void test_weekday(void)
{
    struct tm mydate;
    SetDate(&mydate);

    time_t t;
    assert(StringToTimeFrom("third monday", &t, &mydate) == 0);
    assert(t == 1784505600);
    assert(StringToTimeFrom("last wednes,", &t, &mydate) == 0);
    assert(t == 1782864000);
    assert(StringToTimeFrom("this Sun,", &t, &mydate) == 0);
    assert(t == 1783209600);
}

static void test_everything_combined(void)
{
    struct tm mydate;
    SetDate(&mydate);

    time_t t;
    assert(StringToTimeFrom("next friday 14:37 UTC", &t, &mydate) == 0);
    assert(t == 1783694220);
    assert(StringToTimeFrom("3 weeks ago last monday 08:15", &t, &mydate) == 0);
    assert(t == 1780906500);
    assert(StringToTimeFrom("tomorrow 1 hour 15 minutes ago", &t, &mydate) == 0);
    assert(t == 1783382640);
    assert(StringToTimeFrom("next sunday +5 weeks 04:30 UTC", &t, &mydate) == 0);
    assert(t == 1786854600);
    assert(StringToTimeFrom("2024-02-29 12:00:00 +18 months", &t, &mydate) == 0);
    assert(t == 1756468800);
    assert(StringToTimeFrom("2026-07-15T13:45:30Z", &t, &mydate) == 0);
    assert(t == 1784123130);
    assert(StringToTimeFrom("2026-07-15T13:45:30+02:00", &t, &mydate) == 0);
    assert(t == 1784115930);
    assert(StringToTimeFrom("2026-07-15 13:45:30 UTC", &t, &mydate) == 0);
    assert(t == 1784123130);
    assert(StringToTimeFrom("2026-07-15T13:45:30Z +2 weeks", &t, &mydate) == 0);
    assert(t == 1785332730);
    assert(StringToTimeFrom("2025-12-31 23:59:59 UTC +1 second", &t, &mydate) == 0);
    assert(t == 1767225600);
    assert(StringToTimeFrom("next tuesday 13:45:27.123456 UTC", &t, &mydate) == 0);
    assert(t == 1783431927);
    assert(StringToTimeFrom("1970-01-01T00:00:00Z +2147483647 seconds", &t, &mydate) == 0);
    assert(t == 2147483647);
    assert(StringToTimeFrom("UTC +1 Monday", &t, &mydate) == 0);
    assert(t == 1783292400);
    assert(StringToTimeFrom("UTC 2 Monday", &t, &mydate) == 0);
    assert(t == 1783900800);
    assert(StringToTimeFrom("UTC Monday", &t, &mydate) == 0);
    assert(t == 1783296000);
    assert(StringToTimeFrom("July 13 10:15", &t, &mydate) == 0);
    assert(t == 1783937700);
    assert(StringToTimeFrom("21 September 10:10", &t,  &mydate) == 0);
    assert(t == 1789985400);
    assert(StringToTime("UTC 21 September 2026", &t) == 0);
    assert(t == 1789948800);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_date),
        unit_test(test_time),
        unit_test(test_timezone),
        unit_test(test_relative_time),
        unit_test(test_weekday),
        unit_test(test_everything_combined)
    };

    return run_tests(tests);
}
