#pragma once

#include <istream>
#include <set>
#include <sstream>
#include <string>

namespace CodexCalendar
{
    // Day numbers use the civil date in Beijing, matching TimeBarMarkers.
    inline long long DayNumber(int year, int month, int day)
    {
        year -= month <= 2;
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned y = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        return era * 146097LL + y * 365 + y / 4 - y / 100 + doy - 719468;
    }

    inline bool ParseDate(const std::string& text, int expected_year, long long& day)
    {
        if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
        for (size_t i = 0; i < text.size(); ++i)
            if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9')) return false;
        const int year = std::stoi(text.substr(0, 4));
        const int month = std::stoi(text.substr(5, 2));
        const int date = std::stoi(text.substr(8, 2));
        if (year != expected_year || year < 1970 || month < 1 || month > 12) return false;
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        const int lengths[] = { 31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (date < 1 || date > lengths[month - 1]) return false;
        day = DayNumber(year, month, date);
        return true;
    }

    struct Calendar
    {
        std::set<long long> holidays;
        std::set<long long> workdays;

        bool IsWorkday(long long day) const
        {
            if (workdays.count(day)) return true;
            if (holidays.count(day)) return false;
            const int weekday = static_cast<int>((day + 4) % 7);
            return weekday >= 1 && weekday <= 5;
        }

        bool HasOverride(long long day) const
        {
            return workdays.count(day) != 0 || holidays.count(day) != 0;
        }

        void ReadYear(std::istream& input, int year)
        {
            std::string line;
            while (std::getline(input, line))
            {
                if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
                line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
                std::istringstream fields(line);
                std::string kind, from, to, extra;
                if (!(fields >> kind >> from) || (kind != "holiday" && kind != "workday")) continue;
                if (!(fields >> to)) to = from;
                else if (fields >> extra) continue;
                long long first{}, last{};
                if (!ParseDate(from, year, first) || !ParseDate(to, year, last) || first > last) continue;
                auto& dates = kind == "workday" ? workdays : holidays;
                for (long long day = first; day <= last; ++day) dates.insert(day);
            }
        }
    };
}
