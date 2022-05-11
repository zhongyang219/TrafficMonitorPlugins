#pragma once
#include <map>
#include <vector>

//格式化当前时间

const std::vector<CString> FormularList
{
    _T("dddd"),	    // 长格式的星期（星期一~星期日 / Monday~Sunday）
    _T("ddd"),	    // 短格式的星期（周一~周日 / Mon~Sun）
    _T("dd"),	    // 日，带前面的0（01~31）
    _T("d"),	    // 日，不带前面的0（1~31）
    _T("MMMM"),	    // 长格式的月份（一月~十二月 / January~December）
    _T("MMM"),	    // 短格式的月份（1月~12月 / Jan~Dec）
    _T("MM"),	    // 月份，带前面的0（01~12）
    _T("M"),	    // 月份，不带前面的0（1~12）
    _T("yyyy"),	    // 四位数的年份
    _T("yy"),	    // 两位数的年份
    _T("hh"),	    // 小时，12小时制，带前面的0（01~12）
    _T("h"),	    // 小时，12小时制，不带前面的0（1~12）
    _T("HH"),	    // 小时，24小时制，带前面的0（00~23）
    _T("H"),	    // 小时，24小时制，不带前面的0（0~23）
    _T("mm"),	    // 分钟，带前面的0（00~59）
    _T("m"),	    // 分钟，不带前面的0（0~59）
    _T("ss"),	    // 秒钟，带前面的0（00~59）
    _T("s"),	    // 秒钟，不带前面的0（00~59）
    _T("AP"),	    // 表示上午还是下午，只能是“AM”或“PM”
    _T("ap"),	    // 表示上午还是下午，只能是“am”或“pm”
};

class CDataTimeFormatHelper
{
public:
    CDataTimeFormatHelper();
    ~CDataTimeFormatHelper();

    CString GetCurrentDateTimeByFormat(const CString& formular);
    static CString GetDateTimeByFormat(const CString& formular, SYSTEMTIME sys_time);
    void GetCurrentDateTime();
    const SYSTEMTIME& CurrentDateTime();

private:
    CString GetFormualrValue(CString formular_type);
    static CString GetFormualrValue(CString formular_type, SYSTEMTIME sys_time);
    static CString GetWeekString(WORD day_of_week);
    static CString GetWeekStringShort(WORD day_of_week);
    static CString GetMonthString(DWORD month);
    static CString GetMonthStringShort(DWORD month);

private:
    SYSTEMTIME m_cur_time{};
};

