#pragma once

#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>
#include "WorkdayCalendar.h"

namespace CodexTimeBar
{
    struct Schedule
    {
        int rest_schedule{};
        int morning_start{9 * 60 + 30};
        int morning_end{12 * 60};
        int afternoon_start{13 * 60 + 30};
        int afternoon_end{18 * 60 + 30};
    };

    inline bool IsScheduledWorkday(long long day, const CodexCalendar::Calendar& calendar, int rest_schedule)
    {
        if (calendar.HasOverride(day)) return calendar.IsWorkday(day);
        const int weekday = static_cast<int>((day + 4) % 7); // Sunday=0
        const bool saturday = weekday == 6;
        const bool sunday = weekday == 0;
        if (sunday) return false;
        if (!saturday) return true;
        if (rest_schedule == 1) return true;
        if (rest_schedule == 2 || rest_schedule == 3)
        {
            // ISO week 1 of 1970 starts on 1969-12-29; A means odd ISO weeks rest on Saturday.
            const long long monday = day - ((weekday + 6) % 7);
            const long long week_index = (monday - CodexCalendar::DayNumber(1969, 12, 29)) / 7;
            const bool odd_week = ((week_index % 2) + 2) % 2 == 0;
            const bool double_rest_week = rest_schedule == 2 ? odd_week : !odd_week;
            return !double_rest_week;
        }
        return false;
    }

    inline bool IsWorkingTime(long long timestamp, bool weekly, const CodexCalendar::Calendar& calendar, const Schedule& schedule = {})
    {
        const long long beijing = timestamp + 8 * 3600;
        const long long day = beijing / 86400;
        if (!IsScheduledWorkday(day, calendar, schedule.rest_schedule)) return false;
        if (weekly) return true;
        const long long seconds = beijing % 86400;
        return (seconds >= schedule.morning_start * 60 && seconds < schedule.morning_end * 60) ||
            (seconds >= schedule.afternoon_start * 60 && seconds < schedule.afternoon_end * 60);
    }

    inline std::vector<int> MarkerPixels(long long now, long long reset, bool weekly, int width,
        const CodexCalendar::Calendar& calendar = {}, long long custom_duration = 0, const Schedule& schedule = {})
    {
        std::vector<int> pixels;
        if (width <= 0 || reset <= now) return pixels;
        constexpr long long day_seconds = 86400;
        constexpr long long beijing_offset = 8 * 3600;
        const long long duration = custom_duration > 0 ? custom_duration : (weekly ? 7 * day_seconds : 5 * 3600);
        const long long visible_start = (std::max)(now, reset - duration);
        const long long first_day = (visible_start + beijing_offset) / day_seconds;
        const long long last_day = (reset + beijing_offset) / day_seconds;
        const auto add = [&](long long boundary) {
            if (boundary <= visible_start || boundary >= reset) return;
            // The remaining bar shrinks from the right; reset is its left endpoint.
            const int pixel = static_cast<int>(std::lround(width * static_cast<double>(reset - boundary) / duration));
            const int visible_width = static_cast<int>(std::lround(width * (std::min)(1.0, static_cast<double>(reset - now) / duration)));
            if (pixel > 0 && pixel < visible_width - 1) pixels.push_back(pixel);
        };
        for (long long day = first_day; day <= last_day; ++day)
        {
            const long long midnight = day * day_seconds - beijing_offset;
            if (weekly)
            {
                if (IsScheduledWorkday(day, calendar, schedule.rest_schedule) != IsScheduledWorkday(day - 1, calendar, schedule.rest_schedule)) add(midnight);
            }
            else if (IsScheduledWorkday(day, calendar, schedule.rest_schedule))
            {
                for (int minutes : { schedule.morning_start, schedule.morning_end, schedule.afternoon_start, schedule.afternoon_end })
                    add(midnight + minutes * 60);
            }
        }
        std::sort(pixels.begin(), pixels.end());
        pixels.erase(std::unique(pixels.begin(), pixels.end()), pixels.end());
        return pixels;
    }
}
