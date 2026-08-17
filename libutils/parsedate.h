#ifndef PARSEDATE_H
#define PARSEDATE_H

#include <time.h>

/**
 * Parses a date/time string into a time_t epoch timestamp.
 * 
 * @param p    The string buffer containing the date/time to parse.
 * @param now  Optional pointer to a reference time (pass NULL for current time).
 * @param zone Optional pointer to a timezone offset in minutes (pass NULL for local time).
 * @return     Epoch timestamp (time_t), or (time_t)-1 on error.
 */
time_t ParseDate(const char *p, const time_t *now, const int *zone);

#endif /* PARSEDATE_H */
