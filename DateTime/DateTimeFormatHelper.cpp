#include "pch.h"
#include "DateTimeFormatHelper.h"
#include "DataManager.h"

CDataTimeFormatHelper::CDataTimeFormatHelper()
{
}


CDataTimeFormatHelper::~CDataTimeFormatHelper()
{
}

CString CDataTimeFormatHelper::GetCurrentDateTimeByFormat(const CString& formular)
{
    return GetDateTimeByFormat(formular, m_cur_time);
}

CString CDataTimeFormatHelper::GetDateTimeByFormat(const CString& formular, SYSTEMTIME sys_time)
{
    CString result{ formular };
    for (int i = 0; i < result.GetLength(); i++)
    {
        for (const auto& item : FormularList)
        {
            if (result.Mid(i, item.GetLength()) == item)
            {
                result.Delete(i, item.GetLength());
                CString formular_value = GetFormualrValue(item, sys_time);
                result.Insert(i, formular_value);
                i += (formular_value.GetLength() - 1);
                break;
            }
        }
    }
    return result;
}

void CDataTimeFormatHelper::GetCurrentDateTime()
{
    GetLocalTime(&m_cur_time);
}

const SYSTEMTIME& CDataTimeFormatHelper::CurrentDateTime()
{
    return m_cur_time;
}

CString CDataTimeFormatHelper::GetFormualrValue(CString formular_type)
{
    return GetFormualrValue(formular_type, m_cur_time);
}

CString CDataTimeFormatHelper::GetFormualrValue(CString formular_type, SYSTEMTIME sys_time)
{
    CString str;
    if (formular_type == _T("d"))
        str.Format(_T("%d"), sys_time.wDay);
    else if (formular_type == _T("dd"))
        str.Format(_T("%02d"), sys_time.wDay);
    else if (formular_type == _T("ddd"))
        str = GetWeekStringShort(sys_time.wDayOfWeek);
    else if (formular_type == _T("dddd"))
        str = GetWeekString(sys_time.wDayOfWeek);
    else if (formular_type == _T("M"))
        str.Format(_T("%d"), sys_time.wMonth);
    else if (formular_type == _T("MM"))
        str.Format(_T("%02d"), sys_time.wMonth);
    else if (formular_type == _T("MMM"))
        str = GetMonthStringShort(sys_time.wMonth);
    else if (formular_type == _T("MMMM"))
        str = GetMonthString(sys_time.wMonth);
    else if (formular_type == _T("yy"))
    {
        str.Format(_T("%d"), sys_time.wYear);
        str = str.Right(2);
    }
    else if (formular_type == _T("yyyy"))
        str.Format(_T("%d"), sys_time.wYear);
    else if (formular_type == _T("h"))
    {
        int h = sys_time.wHour;
        if (sys_time.wHour == 0)
            h = 12;
        else if (sys_time.wHour >= 13)
            h = sys_time.wHour - 12;
        str.Format(_T("%d"), h);
    }
    else if (formular_type == _T("hh"))
    {
        int h = sys_time.wHour;
        if (sys_time.wHour == 0)
            h = 12;
        else if (sys_time.wHour >= 13)
            h = sys_time.wHour - 12;
        str.Format(_T("%02d"), h);
    }
    else if (formular_type == _T("H"))
        str.Format(_T("%d"), sys_time.wHour);
    else if (formular_type == _T("HH"))
        str.Format(_T("%02d"), sys_time.wHour);
    else if (formular_type == _T("m"))
        str.Format(_T("%d"), sys_time.wMinute);
    else if (formular_type == _T("mm"))
        str.Format(_T("%02d"), sys_time.wMinute);
    else if (formular_type == _T("s"))
        str.Format(_T("%d"), sys_time.wSecond);
    else if (formular_type == _T("ss"))
        str.Format(_T("%02d"), sys_time.wSecond);
    else if (formular_type == _T("AP"))
    {
        if (sys_time.wHour < 12)
            str = _T("AM");
        else
            str = _T("PM");
    }
    else if (formular_type == _T("ap"))
    {
        if (sys_time.wHour < 12)
            str = _T("am");
        else
            str = _T("pm");
    }
    return str;
}

CString CDataTimeFormatHelper::GetWeekString(WORD day_of_week)
{
    switch (day_of_week)
    {
    case 0: return g_data.StringRes(IDS_SUNDAY);
    case 1: return g_data.StringRes(IDS_MONDAY);
    case 2: return g_data.StringRes(IDS_TUESDAY);
    case 3: return g_data.StringRes(IDS_WEDNESDAY);
    case 4: return g_data.StringRes(IDS_THURSDAY);
    case 5: return g_data.StringRes(IDS_FRIDAY);
    case 6: return g_data.StringRes(IDS_SATURDAY);
    }
    return CString();
}

CString CDataTimeFormatHelper::GetWeekStringShort(WORD day_of_week)
{
    switch (day_of_week)
    {
    case 0: return g_data.StringRes(IDS_SUN);
    case 1: return g_data.StringRes(IDS_MON);
    case 2: return g_data.StringRes(IDS_TUE);
    case 3: return g_data.StringRes(IDS_WED);
    case 4: return g_data.StringRes(IDS_THU);
    case 5: return g_data.StringRes(IDS_FRI);
    case 6: return g_data.StringRes(IDS_SAT);
    }
    return CString();
}

CString CDataTimeFormatHelper::GetMonthString(DWORD month)
{
    switch (month)
    {
    case 1: return g_data.StringRes(IDS_JANUARY);
    case 2: return g_data.StringRes(IDS_FEBRUARY);
    case 3: return g_data.StringRes(IDS_MARCH);
    case 4: return g_data.StringRes(IDS_APRIL);
    case 5: return g_data.StringRes(IDS_MAY_);
    case 6: return g_data.StringRes(IDS_JUNE);
    case 7: return g_data.StringRes(IDS_JULY);
    case 8: return g_data.StringRes(IDS_AUGUST);
    case 9: return g_data.StringRes(IDS_SEPTEMBER);
    case 10: return g_data.StringRes(IDS_OCTOBER);
    case 11: return g_data.StringRes(IDS_NOVEMBER);
    case 12: return g_data.StringRes(IDS_DECEMBER);
    }
    return CString();
}

CString CDataTimeFormatHelper::GetMonthStringShort(DWORD month)
{
    switch (month)
    {
    case 1: return g_data.StringRes(IDS_JAN);
    case 2: return g_data.StringRes(IDS_FEB);
    case 3: return g_data.StringRes(IDS_MAR);
    case 4: return g_data.StringRes(IDS_ARI);
    case 5: return g_data.StringRes(IDS_MAY);
    case 6: return g_data.StringRes(IDS_JUN);
    case 7: return g_data.StringRes(IDS_JUL);
    case 8: return g_data.StringRes(IDS_AUT);
    case 9: return g_data.StringRes(IDS_SEP);
    case 10: return g_data.StringRes(IDS_OCT);
    case 11: return g_data.StringRes(IDS_NOV);
    case 12: return g_data.StringRes(IDS_DEC);
    }
    return CString();
}
