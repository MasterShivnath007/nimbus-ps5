// Calendar conversions shared by the app and the data workers.
//
// The PS5 runtime exposes gmtime()/mktime() but not the reentrant gmtime_r()
// or timegm().  These small, thread-safe routines keep the app independent of
// platform libc extensions and make the UTC parsing deterministic everywhere.
#pragma once

#include <cstdint>

namespace nimbus {

struct UtcTime {
    int year = 1970;
    unsigned month = 1;
    unsigned day = 1;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
};

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant).
inline int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

inline void civil_from_days(int64_t days, int &year, unsigned &month, unsigned &day) {
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp + (mp < 10 ? 3 : -9);
    year = y + (month <= 2 ? 1 : 0);
}

inline UtcTime utc_from_unix(int64_t unix_seconds) {
    int64_t days = unix_seconds / 86400;
    int64_t remainder = unix_seconds % 86400;
    if (remainder < 0) {
        remainder += 86400;
        --days;
    }
    UtcTime value;
    civil_from_days(days, value.year, value.month, value.day);
    value.hour = static_cast<unsigned>(remainder / 3600);
    value.minute = static_cast<unsigned>((remainder % 3600) / 60);
    value.second = static_cast<unsigned>(remainder % 60);
    return value;
}

inline int64_t unix_from_utc(int year, unsigned month, unsigned day, unsigned hour,
                             unsigned minute, unsigned second) {
    return days_from_civil(year, month, day) * 86400 + static_cast<int64_t>(hour) * 3600 +
           static_cast<int64_t>(minute) * 60 + second;
}

}  // namespace nimbus
