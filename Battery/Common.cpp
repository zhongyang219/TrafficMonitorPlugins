#include "pch.h"
#include "Common.h"
#include <sstream>
#include "DataManager.h"

std::wstring CCommon::StrToUnicode(const char* str, bool utf8)
{
    if (str == nullptr)
        return std::wstring();
    std::wstring result;
    int size;
    size = MultiByteToWideChar((utf8 ? CP_UTF8 : CP_ACP), 0, str, -1, NULL, 0);
    if (size <= 0) return std::wstring();
    wchar_t* str_unicode = new wchar_t[size + 1];
    MultiByteToWideChar((utf8 ? CP_UTF8 : CP_ACP), 0, str, -1, str_unicode, size);
    result.assign(str_unicode);
    delete[] str_unicode;
    return result;
}

std::string CCommon::UnicodeToStr(const wchar_t* wstr, bool utf8)
{
    if (wstr == nullptr)
        return std::string();
    std::string result;
    int size{ 0 };
    size = WideCharToMultiByte((utf8 ? CP_UTF8 : CP_ACP), 0, wstr, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return std::string();
    char* str = new char[size + 1];
    WideCharToMultiByte((utf8 ? CP_UTF8 : CP_ACP), 0, wstr, -1, str, size, NULL, NULL);
    result.assign(str);
    delete[] str;
    return result;
}

std::wstring CCommon::TimeFormat(int seconds)
{
    int hour = seconds / 3600;
    int minute = seconds % 3600 / 60;
    //int second = seconds % 60;
    std::wstringstream wss;
    wss << hour << L' ' << g_data.StringRes(IDS_HOUR).GetString()
        << L' ' << minute << L' ' << g_data.StringRes(IDS_MINUTE).GetString()
        //<< L' ' << second << L' ' << g_data.StringRes(IDS_SECOND).GetString()
        ;
    return wss.str();
}
