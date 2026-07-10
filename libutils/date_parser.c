/*
  Copyright 2026 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <time.h>
#include <sequence.h>
#include <stdbool.h>
#include <string_lib.h>
#include <ctype.h>
#include <alloc.h>
#include <errno.h>

/*
 * https://www.gnu.org/software/coreutils/manual/html_node/Date-input-formats.html
 */

typedef enum {
    DAY_SUNDAY = 0,
    DAY_MONDAY,
    DAY_TUESDAY,
    DAY_WEDNESDAY,
    DAY_THURSDAY,
    DAY_FRIDAY,
    DAY_SATURDAY,
} Weekday;


typedef enum {
    MONTH_JANUARY = 0,
    MONTH_FEBRUARY,
    MONTH_MARCH,
    MONTH_APRIL,
    MONTH_MAY,
    MONTH_JUNE,
    MONTH_JULY,
    MONTH_AUGUST,
    MONTH_SEPTEMBER,
    MONTH_OCTOBER,
    MONTH_NOVEMBER,
    MONTH_DECEMBER
} Month;


typedef enum {
    MERIDIAN_AM,
    MERIDIAN_PM
} Meridian;


typedef enum {
    ORD_THIS = 0,
    ORD_FIRST = 1,
    ORD_THIRD = 3,
    ORD_FOURTH,
    ORD_FIFTH,
    ORD_SIXTH,
    ORD_SEVENTH,
    ORD_EIGHTH,
    ORD_NINTH,
    ORD_TENTH,
    ORD_ELEVENTH,
    ORD_TWELFTH,
    ORD_LAST
} Ordinal;


typedef enum {
    RELATIVE_YEAR,
    RELATIVE_MONTH,
    RELATIVE_FORTNIGHT,
    RELATIVE_WEEK,
    RELATIVE_DAY,
    RELATIVE_HOUR,
    RELATIVE_MINUTE,
    RELATIVE_SECOND
} Relative;

typedef enum {
    TIMEZONE_Z,
    TIMEZONE_UTC
} Timezone;


typedef enum {
    DATE_TOKEN_AT,
    DATE_TOKEN_COMMA,
    DATE_TOKEN_DOT,
    DATE_TOKEN_SLASH,
    DATE_TOKEN_PLUS,
    DATE_TOKEN_DASH,
    DATE_TOKEN_COLON,
    DATE_TOKEN_T,
    DATE_TOKEN_MONTH,
    DATE_TOKEN_MERIDIAN,
    DATE_TOKEN_ZONE,
    DATE_TOKEN_WEEKDAY,
    DATE_TOKEN_RELATIVE,
    DATE_TOKEN_ORDINAL,
    DATE_TOKEN_TIMEZONE,
    DATE_TOKEN_AGO,
    DATE_TOKEN_DEIXIS,
    DATE_TOKEN_INTEGER
} DateTokenType;


typedef struct
{
    DateTokenType type;
    union {
        struct { const char *ptr; size_t len; } str;
        Weekday day;
        Month month;
        Meridian meridian;
        Ordinal ordinal;
        Relative relative;
        Timezone tz;
        int offset;
    };
} DateToken;


typedef struct {
    const char *name;
    DateTokenType type;
    union {
        Weekday day;
        Month month;
        Meridian meridian;
        Ordinal ordinal;
        Relative relative;
        Timezone tz;
        int offset;
    };
} DateKeyword;


// TODO: make smarter keyword matching: "thursday", "thu", "thur", "thurs"
static const DateKeyword keywords[] = {
    {.name = "january", .type = DATE_TOKEN_MONTH, .month = MONTH_JANUARY},
    {.name = "february", .type = DATE_TOKEN_MONTH, .month = MONTH_FEBRUARY},
    {.name = "march", .type = DATE_TOKEN_MONTH, .month = MONTH_MARCH},
    {.name = "april", .type = DATE_TOKEN_MONTH, .month = MONTH_APRIL},
    {.name = "may", .type = DATE_TOKEN_MONTH, .month = MONTH_MAY},
    {.name = "june", .type = DATE_TOKEN_MONTH, .month = MONTH_JUNE},
    {.name = "july", .type = DATE_TOKEN_MONTH, .month = MONTH_JULY},
    {.name = "august", .type = DATE_TOKEN_MONTH, .month = MONTH_AUGUST},
    {.name = "september", .type = DATE_TOKEN_MONTH, .month = MONTH_SEPTEMBER},
    {.name = "october", .type = DATE_TOKEN_MONTH, .month = MONTH_OCTOBER},
    {.name = "november", .type = DATE_TOKEN_MONTH, .month = MONTH_NOVEMBER},
    {.name = "december", .type = DATE_TOKEN_MONTH, .month = MONTH_DECEMBER},
    {.name = "jan", .type = DATE_TOKEN_MONTH, .month = MONTH_JANUARY},
    {.name = "feb", .type = DATE_TOKEN_MONTH, .month = MONTH_FEBRUARY},
    {.name = "mar", .type = DATE_TOKEN_MONTH, .month = MONTH_MARCH},
    {.name = "apr", .type = DATE_TOKEN_MONTH, .month = MONTH_APRIL},
    {.name = "jun", .type = DATE_TOKEN_MONTH, .month = MONTH_JUNE},
    {.name = "jul", .type = DATE_TOKEN_MONTH, .month = MONTH_JULY},
    {.name = "aug", .type = DATE_TOKEN_MONTH, .month = MONTH_AUGUST},
    {.name = "sep", .type = DATE_TOKEN_MONTH, .month = MONTH_SEPTEMBER},
    {.name = "sept", .type = DATE_TOKEN_MONTH, .month = MONTH_SEPTEMBER},
    {.name = "oct", .type = DATE_TOKEN_MONTH, .month = MONTH_OCTOBER},
    {.name = "nov", .type = DATE_TOKEN_MONTH, .month = MONTH_NOVEMBER},
    {.name = "dec", .type = DATE_TOKEN_MONTH, .month = MONTH_DECEMBER},

    {.name = "monday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_MONDAY},
    {.name = "tuesday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_TUESDAY},
    {.name = "wednesday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_WEDNESDAY},
    {.name = "thursday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_THURSDAY},
    {.name = "friday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_FRIDAY},
    {.name = "saturday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_SATURDAY},
    {.name = "sunday", .type = DATE_TOKEN_WEEKDAY, .day = DAY_SUNDAY},
    {.name = "mon", .type = DATE_TOKEN_WEEKDAY, .day = DAY_MONDAY},
    {.name = "tue", .type = DATE_TOKEN_WEEKDAY, .day = DAY_TUESDAY},
    {.name = "tues", .type = DATE_TOKEN_WEEKDAY, .day = DAY_TUESDAY},
    {.name = "wed", .type = DATE_TOKEN_WEEKDAY, .day = DAY_WEDNESDAY},
    {.name = "wednes", .type = DATE_TOKEN_WEEKDAY, .day = DAY_WEDNESDAY},
    {.name = "thu", .type = DATE_TOKEN_WEEKDAY, .day = DAY_THURSDAY},
    {.name = "thur", .type = DATE_TOKEN_WEEKDAY, .day = DAY_THURSDAY},
    {.name = "thurs", .type = DATE_TOKEN_WEEKDAY, .day = DAY_THURSDAY},
    {.name = "fri", .type = DATE_TOKEN_WEEKDAY, .day = DAY_FRIDAY},
    {.name = "sat", .type = DATE_TOKEN_WEEKDAY, .day = DAY_SATURDAY},
    {.name = "sun", .type = DATE_TOKEN_WEEKDAY, .day = DAY_SUNDAY},

    {.name = "am", .type = DATE_TOKEN_MERIDIAN, .meridian = MERIDIAN_AM},
    {.name = "a.m.", .type = DATE_TOKEN_MERIDIAN, .meridian = MERIDIAN_AM},
    {.name = "pm", .type = DATE_TOKEN_MERIDIAN, .meridian = MERIDIAN_PM},
    {.name = "p.m.", .type = DATE_TOKEN_MERIDIAN, .meridian = MERIDIAN_PM},

    {.name = "this", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_THIS},
    {.name = "first", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_FIRST},
    {.name = "next", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_FIRST},
    /* 'second' as ordinal is missing because it would get mixed up with 'second' as relative time */
    {.name = "third", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_THIRD},
    {.name = "fourth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_FOURTH},
    {.name = "fifth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_FIFTH},
    {.name = "sixth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_SIXTH},
    {.name = "seventh", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_SEVENTH},
    {.name = "eighth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_EIGHTH},
    {.name = "ninth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_NINTH},
    {.name = "tenth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_TENTH},
    {.name = "eleventh", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_ELEVENTH},
    {.name = "twelfth", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_TWELFTH},
    {.name = "last", .type = DATE_TOKEN_ORDINAL, .ordinal = ORD_LAST},

    {.name = "year", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_YEAR},
    {.name = "month", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MONTH},
    {.name = "fortnight", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_FORTNIGHT},
    {.name = "week", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_WEEK},
    {.name = "day", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_DAY},
    {.name = "hour", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_HOUR},
    {.name = "minute", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MINUTE},
    {.name = "second", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_SECOND},
    {.name = "min", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MINUTE},
    {.name = "sec", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_SECOND},
    {.name = "years", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_YEAR},
    {.name = "months", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MONTH},
    {.name = "fortnights", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_FORTNIGHT},
    {.name = "weeks", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_WEEK},
    {.name = "days", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_DAY},
    {.name = "hours", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_HOUR},
    {.name = "minutes", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MINUTE},
    {.name = "seconds", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_SECOND},
    {.name = "mins", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_MINUTE},
    {.name = "secs", .type = DATE_TOKEN_RELATIVE, .relative = RELATIVE_SECOND},

    {.name = "Z", .type = DATE_TOKEN_TIMEZONE, .tz = TIMEZONE_Z},
    {.name = "UTC", .type = DATE_TOKEN_TIMEZONE, .tz = TIMEZONE_UTC},

    {.name = "tomorrow", .type = DATE_TOKEN_DEIXIS, .offset = 1},
    {.name = "today", .type = DATE_TOKEN_DEIXIS, .offset = 0},
    {.name = "now", .type = DATE_TOKEN_DEIXIS, .offset = 0},
    {.name = "yesterday", .type = DATE_TOKEN_DEIXIS, .offset = -1},
    {.name = "ago", .type = DATE_TOKEN_AGO, .offset = -1}
};


#define NUM_KEYWORDS (sizeof(keywords) / sizeof(keywords[0]))


/* =========================== */


static DateToken *DateTokenCopy(DateToken *in)
{
    DateToken *out = xmalloc(sizeof(DateToken));
    memcpy(out, in, sizeof(DateToken));

    return out;
}


static void TokenizeInteger(const char **curr, const char *token_start, DateToken *token)
{
    while (isdigit(**curr) && **curr != '\0')
    {
        (*curr)++;
    }

    token->type = DATE_TOKEN_INTEGER;
    token->str.ptr = token_start;
    token->str.len = *curr - token_start;
}


static bool TokenizeId(const char **curr, const char *token_start, DateToken *token)
{
    while ((isalpha(**curr) || **curr == '.') && **curr != '\0')
    {
        (*curr)++;
    }
    size_t length = *curr - token_start;

    if (length == 1 && *token_start == 'T')
    {
        token->type = DATE_TOKEN_T;
        return true;
    }

    for (size_t i = 0; i < NUM_KEYWORDS; i++)
    {
        if (length == strlen(keywords[i].name)
            && StringSafeCompareN_IgnoreCase(token_start, keywords[i].name, length) == 0)
        {
            token->type = keywords[i].type;
            switch (token->type) {
                case DATE_TOKEN_MONTH:
                    token->month = keywords[i].month;
                    break;
                case DATE_TOKEN_WEEKDAY:
                    token->day = keywords[i].day;
                    break;
                case DATE_TOKEN_MERIDIAN: 
                    token->meridian = keywords[i].meridian;
                    break;
                case DATE_TOKEN_ORDINAL: 
                    token->ordinal = keywords[i].ordinal;
                    break;
                case DATE_TOKEN_RELATIVE:
                    token->relative = keywords[i].relative;
                    break;
                case DATE_TOKEN_TIMEZONE:
                    token->tz = keywords[i].tz;
                    break;
                case DATE_TOKEN_DEIXIS:
                    token->offset = keywords[i].offset;
                    break;
                case DATE_TOKEN_AGO:
                    token->offset = keywords[i].offset;
                    break;
                default: 
                    assert(false && "Cannot reach here");
                    break;
            }
            return true;
        }
    }
    return false;
}


static Seq *Tokenize(const char *data)
{
    Seq *tokens = SeqNew(10, free);

    DateToken token = {0};

    const char *token_start = data;
    const char *curr = data;
    while (*curr != '\0')
    {
        token_start = curr;
    
        switch (*curr)
        {
        case '\n':
        case ' ':
        case '\t':
        case '\r':
            curr++;
            continue;
        case '@':
            token.type = DATE_TOKEN_AT;
            curr++;
            break;
        case ',':
            token.type = DATE_TOKEN_COMMA;
            curr++;
            break;
        case '.':
            token.type = DATE_TOKEN_DOT;
            curr++;
            break;
        case '/':
            token.type = DATE_TOKEN_SLASH;
            curr++;
            break;
        case '+':
            token.type = DATE_TOKEN_PLUS;
            curr++;
            break;
        case '-':
            token.type = DATE_TOKEN_DASH;
            curr++;
            break;
        case ':':
            token.type = DATE_TOKEN_COLON;
            curr++;
            break;
        default:
            if (isdigit(*curr))
            {
                TokenizeInteger(&curr, token_start, &token);
            }
            else if (isalpha(*curr))
            {
                if (!TokenizeId(&curr, token_start, &token))
                {
                    SeqDestroy(tokens);
                    return NULL;
                }
            }
            else {
                SeqDestroy(tokens);
                return NULL;
            }
        }
        SeqAppend(tokens, DateTokenCopy(&token));
    }
    return tokens;
}


/* =========================== */


typedef struct {
    Seq *tokens;
    size_t idx;
    bool error;

    int mask;

    struct tm *tm;
    struct {
        int year, month, day;
        int hour, minute, second;
    } relative;
    struct {
        int wday;
        int multiple;
    } weekday;

    int tz_offset;

} ParserState;


static inline bool IsEnd(ParserState *state)
{
    return state->idx >= SeqLength(state->tokens) || state->error;
}


static DateToken *PeekToken(ParserState *state)
{
    if (IsEnd(state))
    {
        return NULL;
    }
    return (DateToken *) SeqAt(state->tokens, state->idx);
}


static DateToken *PeekTokenAhead(ParserState *state, size_t ahead)
{
    size_t target = state->idx + ahead;
    if (state->error || target >= SeqLength(state->tokens))
    {
        return NULL;
    }
    return (DateToken *) SeqAt(state->tokens, target);
}


static DateToken *NextToken(ParserState *state)
{
    if (state->error || state->idx + 1 >= SeqLength(state->tokens))
    {
        state->idx = SeqLength(state->tokens);
        return NULL;
    }
    return (DateToken *) SeqAt(state->tokens, ++state->idx);

}

static int64_t ToYear(int64_t year, size_t ndigit)
{
    if (ndigit >= 4) /* 0090 is year 90 */
    {
        return year - 1900;
    }

    if (year >= 69 && year <= 99)
    {
        year += 1900;
    }
    if (year >= 0 && year <= 68)
    {
        year += 2000;
    }
    return year - 1900;
}

/* =========================== */


static bool ParseInt(DateToken *token, int64_t *out)
{
    assert(token->type == DATE_TOKEN_INTEGER);

    errno = 0;
    char *endptr = NULL;
    *out = (int64_t) strtoll(token->str.ptr, &endptr, 10);

    if (token->str.ptr + token->str.len != endptr)
    {
        return false;
    }

    if (errno == ERANGE)
    {
        return false;
    }

    return true;
}

/*
 * 13 July 2026
 * 13 July
 */
static bool ParseDDMonthYY(ParserState *state, int64_t day, int64_t month)
{
    state->tm->tm_mday = day;
    state->tm->tm_mon = month;
    DateToken *token = NextToken(state);
    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return true;
    }

    DateToken *lookahead = PeekTokenAhead(state, 1);
    if (lookahead != NULL && lookahead->type == DATE_TOKEN_COLON)
    {
        return true;
    }


    int64_t year;
    size_t ndigit = token->str.len;
    if (!ParseInt(token, &year))
    {
        state->error = true;
        return false;
    }
    NextToken(state);
    state->tm->tm_year = ToYear(year, ndigit);

    return true;
}

/*
 * 13-July-2026
 * 13-July
 * 2026-07-13
 */
static bool ParseDashDate(ParserState *state, int64_t leading)
{
    DateToken *token = NextToken(state);

    if (token == NULL)
    {
        return true;
    }

    if (token->type == DATE_TOKEN_MONTH)
    {
        int64_t month = token->month;
        int64_t year;
        token = NextToken(state);

        if (token == NULL || token->type != DATE_TOKEN_DASH)
        {
            state->tm->tm_mday = leading;
            state->tm->tm_mon = month;
            return true;
        }
        token = NextToken(state);

        if (token == NULL || token->type != DATE_TOKEN_INTEGER)
        {
            return false;
        }
        size_t ndigit = token->str.len;
        if (!ParseInt(token, &year))
        {
            state->error = true;
            return false;
        }
        NextToken(state);
        state->tm->tm_year = ToYear(year, ndigit);
        state->tm->tm_mon = month;
        state->tm->tm_mday = leading;

        return true;
    }
    else if (token->type == DATE_TOKEN_INTEGER)
    {
        int64_t month;
        int64_t day;
        if (!ParseInt(token, &month))
        {
            state->error = true;
            return false;
        }
        token = NextToken(state);

        if (token == NULL || token->type != DATE_TOKEN_DASH)
        {
            return false;
        }
        token = NextToken(state);

        if (token == NULL || token->type != DATE_TOKEN_INTEGER)
        {
            return false;
        }
        size_t ndigit = token->str.len;
        if (!ParseInt(token, &day))
        {
            state->error = true;
            return false;
        }
        NextToken(state);
        state->tm->tm_year = ToYear(leading, ndigit);
        state->tm->tm_mon = month - 1;
        state->tm->tm_mday = day;

        return true;
    }
    return false;
}

/**
 * 07/13
 * 07/13/2026
 */
static bool ParseSlashDate(ParserState *state, int64_t leading)
{
    DateToken *token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }

    int64_t day;
    if (!ParseInt(token, &day))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_SLASH)
    {
        state->tm->tm_mon = leading - 1;
        state->tm->tm_mday = day;
        return true;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }

    int64_t year;
    size_t ndigit = token->str.len;
    if (!ParseInt(token, &year))
    {
        state->error = true;
        return false;
    }
    state->tm->tm_year = ToYear(year, ndigit);
    state->tm->tm_mon = leading - 1;
    state->tm->tm_mday = day;

    NextToken(state);

    return true;
}

/**
 * 13.07.2026
 * 13.07.
 */
static bool ParseDotDate(ParserState *state, int64_t leading)
{
    DateToken *token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }

    int64_t month;
    if (!ParseInt(token, &month))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_DOT)
    {
        return false;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        state->tm->tm_mon = month - 1;
        state->tm->tm_mday = leading;
        return true;
    }

    int64_t year;
    size_t ndigit = token->str.len;
    if (!ParseInt(token, &year))
    {
        state->error = true;
        return false;
    }
    state->tm->tm_year = ToYear(year, ndigit);
    state->tm->tm_mon = month - 1;
    state->tm->tm_mday = leading;
    NextToken(state);

    return true;
}

/**
 * July 13
 * July 13, 2026
 * July 13 2026
 */
static bool ParseMonthDDYY(ParserState *state, int64_t leading)
{
    DateToken *token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }

    int64_t day;
    if (!ParseInt(token, &day))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL || (token->type != DATE_TOKEN_INTEGER && token->type != DATE_TOKEN_COMMA))
    {
        state->tm->tm_mon = leading;
        state->tm->tm_mday = day;
        return true;
    }

    if (token->type == DATE_TOKEN_INTEGER)
    {
        DateToken *lookahead = PeekTokenAhead(state, 1);
        if (lookahead != NULL && lookahead->type == DATE_TOKEN_COLON)
        {
            state->tm->tm_mon = leading;
            state->tm->tm_mday = day;
            return true;
        }
    }


    if (token->type == DATE_TOKEN_COMMA)
    {
        token = NextToken(state);
        if (token == NULL || token->type != DATE_TOKEN_INTEGER)
        {
            return false;
        }
    }
    int64_t year;
    size_t ndigit = token->str.len;
    if (!ParseInt(token, &year))
    {
        state->error = true;
        return false;
    }
    state->tm->tm_year = ToYear(year, ndigit);
    state->tm->tm_mon = leading;
    state->tm->tm_mday = day;
    NextToken(state);
    
    return true;
}

/* day, month, year */
static bool ParseDate(ParserState *state)
{
    DateToken *token = PeekToken(state);
    if (token == NULL)
    {
        return false;
    }

    if (token->type == DATE_TOKEN_INTEGER)
    {
        int64_t leading;
        if (!ParseInt(token, &leading))
        {
            state->error = true;
            return false;
        }
        token = NextToken(state);

        if (token == NULL)
        {
            return false;
        }

        switch (token->type)
        {
        case DATE_TOKEN_MONTH:
            return ParseDDMonthYY(state, leading, token->month);
        case DATE_TOKEN_DASH:
            return ParseDashDate(state, leading);
        case DATE_TOKEN_SLASH:
            return ParseSlashDate(state, leading);
        case DATE_TOKEN_DOT:
            return ParseDotDate(state, leading);
        default:
            return false;
        }
    }
    else if (token->type == DATE_TOKEN_MONTH)
    {
        return ParseMonthDDYY(state, token->month);
    }
    return false;
}

/* 10:10:01.0001 am */
static bool ParseTime(ParserState *state)
{
    DateToken *token = PeekToken(state);
    if (token != NULL && token->type == DATE_TOKEN_T)
    {
        token = NextToken(state);
    }

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }
    int64_t hour;
    if (!ParseInt(token, &hour))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_COLON)
    {
        return false;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }
    int64_t minute;
    if (!ParseInt(token, &minute))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL)
    {
        /* h:m */
        state->tm->tm_hour = hour;
        state->tm->tm_min = minute;
        state->tm->tm_sec = 0;
        return true;
    }

    int64_t second = 0;
    if (token->type == DATE_TOKEN_COLON)
    {
        token = NextToken(state);
        if (token == NULL || token->type != DATE_TOKEN_INTEGER)
        {
            return false;
        }

        if (!ParseInt(token, &second))
        {
            state->error = true;
            return false;
        }
        token = NextToken(state);

        if (token != NULL && (token->type == DATE_TOKEN_COMMA || token->type == DATE_TOKEN_DOT))
        {
            token = NextToken(state);
            if (token == NULL || token->type != DATE_TOKEN_INTEGER)
            {
                return false;
            }
            token = NextToken(state); /* TODO: parse .0001 to double */
        }
    }

    state->tm->tm_hour = hour;
    state->tm->tm_min = minute;
    state->tm->tm_sec = second;

    /* am | pm */
    if (token != NULL && token->type == DATE_TOKEN_MERIDIAN)
    {
        if (state->tm->tm_hour < 1 || state->tm->tm_hour > 12)
        {
            state->error = true;
            return false;
        }
        if (token->meridian == MERIDIAN_PM && state->tm->tm_hour != 12)
        {
            state->tm->tm_hour += 12;
        }
        else if (token->meridian == MERIDIAN_AM && state->tm->tm_hour == 12)
        {
            state->tm->tm_hour = 0;
        }
        token = NextToken(state);
    }

    return true;
}

/* UTC+10:10 */
static bool ParseTimezone(ParserState *state)
{
    DateToken *token = PeekToken(state);

    if (token == NULL)
    {
        return false;
    }

    bool matched_something = false;

    if (token->type == DATE_TOKEN_TIMEZONE)
    {
        token = NextToken(state);
        matched_something = true;
    }

    if (token == NULL)
    {
        /* Z | UTC*/
        state->tz_offset = 0;
        return true;
    }

    int sign = 1;
    size_t offset_start = state->idx;
    bool has_sign = false;

    if (token->type == DATE_TOKEN_PLUS)
    {
        token = NextToken(state);
        has_sign = true;
    }
    else if (token->type == DATE_TOKEN_DASH)
    {
        sign *= -1;
        token = NextToken(state);
        has_sign = true;
    }

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        if (matched_something) /* UTC Monday */
        {
            state->idx = offset_start;
            state->tz_offset = 0;
            return true;
        }
        return false;
    }

    if (matched_something && !has_sign)
    {
        state->idx = offset_start;
        state->tz_offset = 0;
        return true;
    }


    int64_t hour;
    if (!ParseInt(token, &hour))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    if (token == NULL)
    {
        /* UTC+10 */
        state->tz_offset = hour * 3600 * sign;
        return true;
    }
    if (token->type != DATE_TOKEN_COLON)
    {
        /* Is the next token a relative time/weekday ? If yes, we should probably backtrack */

        if (token != NULL && (token->type == DATE_TOKEN_RELATIVE || (token->type == DATE_TOKEN_WEEKDAY && !has_sign)))
        {
            state->idx = offset_start;
            state->tz_offset = 0;
            return matched_something; /* returns true on UTC +1 years, false on +1 years */
        }

        /* UTC+10 */
        state->tz_offset = hour * 3600 * sign;
        return true;
    }
    token = NextToken(state);

    if (token == NULL || token->type != DATE_TOKEN_INTEGER)
    {
        return false;
    }

    int64_t minute;
    if (!ParseInt(token, &minute))
    {
        state->error = true;
        return false;
    }
    token = NextToken(state);

    state->tz_offset = (hour * 3600 + minute * 60) * sign;
    /* UTC+10:10 */
    return true;
}


/* next year */
static bool ParseRelativeTime(ParserState *state)
{
    DateToken *token = PeekToken(state);

    if (token == NULL)
    {
        return false;
    }

    int sign = 1;
    if (token->type == DATE_TOKEN_DEIXIS)
    {
        state->relative.day += token->offset;
        token = NextToken(state);
        /* tomorrow | today | yesterday */
        return true;
    }
    else if (token->type == DATE_TOKEN_PLUS)
    {
        token = NextToken(state);
    }
    else if (token->type == DATE_TOKEN_DASH)
    {
        sign = -1;
        token = NextToken(state);
    }

    if (token == NULL)
    {
        return false;
    }

    int64_t offset = 1; /* No number implies 1 */
    if (token->type == DATE_TOKEN_INTEGER)
    {
        if (!ParseInt(token, &offset))
        {
            state->error = true;
            return false;
        }
        token = NextToken(state);
    }
    else if (token->type == DATE_TOKEN_ORDINAL)
    {
        if (token->ordinal == ORD_LAST)
        {
            offset = -1;
        }
        else {
            offset = token->ordinal;
        }
        token = NextToken(state);
        sign = 1; /* ordinals are not modified by - | + */
    }

    if (token == NULL || token->type != DATE_TOKEN_RELATIVE)
    {
        return false;
    }

    
    Relative relative = token->relative;
    token = NextToken(state);

    if (token != NULL && token->type == DATE_TOKEN_AGO)
    {
        sign *= -1;
        token = NextToken(state);
    }

    switch (relative)
    {
    case RELATIVE_SECOND:
        state->relative.second += (offset * sign);
        break;
    case RELATIVE_MINUTE:
        state->relative.minute += (offset * sign);
        break;
    case RELATIVE_HOUR:
        state->relative.hour += (offset * sign);
        break;
    case RELATIVE_DAY:
        state->relative.day += (offset * sign);
        break;
    case RELATIVE_WEEK:
        state->relative.day += (7 * offset * sign);
        break;
    case RELATIVE_FORTNIGHT:
        state->relative.day += (14 * offset * sign);
        break;
    case RELATIVE_MONTH:
        state->relative.month += (offset * sign);
        break;
    case RELATIVE_YEAR:
        state->relative.year += (offset * sign);
        break;
    }
    return true;
}


/* first Monday */
static bool ParseWeekday(ParserState *state)
{
    DateToken *token = PeekToken(state);

    if (token == NULL)
    {
        return false;
    }

    int64_t offset = 1;
    if (token->type == DATE_TOKEN_INTEGER)
    {
        if (!ParseInt(token, &offset))
        {
            state->error = true;
            return false;
        }
        token = NextToken(state);
    }
    else if (token->type == DATE_TOKEN_ORDINAL)
    {
        if (token->ordinal == ORD_LAST)
        {
            offset = -1;
        }
        else {
            offset = token->ordinal;
        }
        token = NextToken(state);
    }
    if (token == NULL || token->type != DATE_TOKEN_WEEKDAY)
    {
        return false;
    }

    state->weekday.wday = token->day;
    state->weekday.multiple = offset;
    
    token = NextToken(state);
    if (token != NULL && token->type == DATE_TOKEN_COMMA)
    {
        NextToken(state);
    }
    return true;
}

/* =========================== */

typedef bool (*ParserFormatFn)(ParserState *);

typedef enum {
    FORMAT_DATE_FLAG = 1,
    FORMAT_TIME_FLAG = 1 << 2,
    FORMAT_TIMEZONE_FLAG = 1 << 3,
    FORMAT_RELATIVE_FLAG = 1 << 4,
    FORMAT_WEEKDAY_FLAG = 1 << 5
} ParserFormatMask;

typedef struct {
    ParserFormatFn fn;
    ParserFormatMask flag;
} ParserFormat;

/* The order is important: timezone should be before relative time */
static ParserFormat date_parser_functions[] = {
    {.fn = ParseDate, .flag = FORMAT_DATE_FLAG},
    {.fn = ParseTime, .flag = FORMAT_TIME_FLAG},
    {.fn = ParseTimezone, .flag = FORMAT_TIMEZONE_FLAG},
    {.fn = ParseRelativeTime, .flag = FORMAT_RELATIVE_FLAG},
    {.fn = ParseWeekday, .flag = FORMAT_WEEKDAY_FLAG}
    /* TODO: parse seconds from Epoch */
};


#define NUM_FN (sizeof(date_parser_functions) / sizeof(date_parser_functions[0]))


static int GetWeekDayOffset(int current, int target, int offset)
{
    int diff = target - current;
    if (offset > 0)
    {
        if (diff <= 0) 
        {
            diff += 7; 
        }
        diff += (offset - 1) * 7;
    }
    else if (offset < 0) 
    {
        if (diff >= 0) 
        {
            diff -= 7;
        }
        diff += (offset + 1) * 7; 
    }
    return diff;
}

time_t ApplyParserState(ParserState *state)
{
    struct tm *tm = state->tm;

    bool time_given = state->mask & FORMAT_TIME_FLAG;
    bool date_given = state->mask & (FORMAT_DATE_FLAG | FORMAT_WEEKDAY_FLAG);
    bool relative_only = (state->mask & FORMAT_RELATIVE_FLAG) && !date_given;

    if (!time_given && !relative_only)
    {
        tm->tm_hour = 0;
        tm->tm_min  = 0;
        tm->tm_sec  = 0;
    }
    if (state->mask & FORMAT_RELATIVE_FLAG)
    {
        tm->tm_year += state->relative.year;
        tm->tm_mon += state->relative.month;
        tm->tm_mday += state->relative.day;
        tm->tm_hour += state->relative.hour;
        tm->tm_min += state->relative.minute;
        tm->tm_sec += state->relative.second;
    }
    if (state->mask & FORMAT_WEEKDAY_FLAG && !(state->mask & FORMAT_DATE_FLAG))
    {
        tm->tm_mday += GetWeekDayOffset(tm->tm_wday, state->weekday.wday, state->weekday.multiple);
    }

    if (state->mask & FORMAT_TIMEZONE_FLAG)
    {
        return timegm(state->tm) - state->tz_offset;
    }

    tm->tm_isdst = -1;
    return mktime(state->tm);
}

int StringToTimeFrom(const char *date, time_t *out, struct tm *from)
{
    assert(date != NULL);
    assert(out != NULL);
    assert(from != NULL);

    struct tm tm = {0};
    memcpy(&tm, from, sizeof(struct tm));

    Seq *tokens = Tokenize(date);
    if (tokens == NULL)
    {
        return 1;
    }

    ParserState state = {
        .tokens = tokens, 
        .idx = 0, 
        .mask = 0,
        .tm = &tm,
        .relative = {0},
        .tz_offset = 0,
        .weekday = {.multiple = 0, .wday = tm.tm_wday}
    };

    size_t idx;
    ParserFormat fmt;

    while (!IsEnd(&state))
    {
        bool matched = false;

        for (size_t i = 0; i < NUM_FN; i++)
        {
            fmt = date_parser_functions[i];

            /* relative can be repeated: year year year == 3 year */
            if ((state.mask & fmt.flag) && (fmt.flag != FORMAT_RELATIVE_FLAG))
            {
                continue;
            }

            idx = state.idx;
            if (fmt.fn(&state))
            {
                state.mask |= fmt.flag;
                matched = true;
                break;
            }
            state.idx = idx;
        }

        if (!matched)
        {
            state.error = true;
            break;
        }
    }

    *out = ApplyParserState(&state);

    SeqDestroy(tokens); 
    return (state.error) ? 2 : 0;
}

int StringToTime(const char *date, time_t *out)
{
    if (date == NULL || out == NULL)
    {
        return 1;
    }
    time_t now = time(NULL);
    struct tm today;
    if (localtime_r(&now, &today) == NULL)
    {
        return 1;
    }

    return StringToTimeFrom(date, out, &today);
}
