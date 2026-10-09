#include "../TimeBarMarkers.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace CodexCalendar;
long long at(int y,int m,int d,int hour=0) { return DayNumber(y,m,d)*86400-8*3600+hour*3600; }
int main() {
 Calendar c; std::ifstream file("calendar/2026.txt"); assert(file); c.ReadYear(file,2026);
 assert(c.holidays.size()==33 && c.workdays.size()==6);
 for(auto day:c.holidays) assert(!c.IsWorkday(day));
 for(auto day:c.workdays) assert(c.IsWorkday(day));
 assert(!c.IsWorkday(DayNumber(2026,10,1)));
 assert(c.IsWorkday(DayNumber(2026,10,10)));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,10,9),false,c));
 assert(CodexTimeBar::IsWorkingTime(at(2026,10,10,9)+30*60,false,c));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,10,12),false,c));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,10,13),false,c));
 assert(CodexTimeBar::IsWorkingTime(at(2026,10,10,13)+30*60,false,c));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,10,18)+30*60,false,c));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,1,10),false,c));
 assert(!CodexTimeBar::IsWorkingTime(at(2026,10,1,10),true,c));
 assert(CodexTimeBar::IsWorkingTime(at(2026,10,10,23),true,c));
 assert(!c.IsWorkday(DayNumber(2026,10,11)));
 assert(c.IsWorkday(DayNumber(2027,1,1))); // Unknown-year weekday fallback, not a prediction.
 assert(!c.IsWorkday(DayNumber(2027,1,2)));
 assert(CodexTimeBar::MarkerPixels(at(2026,10,1,8),at(2026,10,1,13),false,1000,c).empty());
 assert(CodexTimeBar::MarkerPixels(at(2026,10,10,8),at(2026,10,10,13),false,1000,c).size()==2);
 const auto weekly=CodexTimeBar::MarkerPixels(at(2026,9,19),at(2026,9,26),true,1000,c);
 assert((weekly==std::vector<int>{143,857}));
 Calendar precedence; std::istringstream lines("\xEF\xBB\xBFworkday 2026-10-10 # comment\r\nholiday 2026-10-10\r\nholiday 2026-02-29\nholiday 2027-01-01\nholiday 2026-12-31 2027-01-01\nholiday 2026-01-03 2026-01-01\nholiday 2026-01-01 invalid\nworkday 2026-01-05 extra tokens\n"); precedence.ReadYear(lines,2026);
 assert(precedence.holidays.size()==1 && precedence.workdays.size()==1 && precedence.IsWorkday(DayNumber(2026,10,10)));
 std::cout<<"PASS: all 33 holiday dates, 6 make-up workdays, holiday/workday time markers, workday precedence, missing year, BOM/CRLF/comments, invalid dates/ranges\n";
}
