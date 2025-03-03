#include "pch.h"
#include "Common.h"
#include <afxinet.h>    //用于支持使用网络相关的类
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

bool CCommon::GetURL(const std::wstring& url, std::string& result, bool utf8, LPCTSTR user_agent, LPCTSTR headers, DWORD dwHeadersLength)
{
    bool succeed{ false };
    CInternetSession* pSession{};
    CHttpFile* pfile{};
    try
    {
        pSession = new CInternetSession(user_agent);
        //pfile = (CHttpFile*)pSession->OpenURL(url.c_str());
        //CCommon::WriteLog(L"request>>>>", g_data.m_log_path.c_str());
        pfile = (CHttpFile*)pSession->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, headers, dwHeadersLength);
        //CCommon::WriteLog(L"request<<<", g_data.m_log_path.c_str());
        DWORD dwStatusCode;
        pfile->QueryInfoStatusCode(dwStatusCode);
        if (dwStatusCode == HTTP_STATUS_OK)
        {
            CString content;
            CString data;
            while (pfile->ReadString(data))
            {
                content += data;
            }
            result = (const char*)content.GetString();
            succeed = true;
        }
        pfile->Close();
        delete pfile;
        pSession->Close();
    }
    catch (CInternetException* e)
    {
        CCommon::WriteLog(L"request fail!", g_data.m_log_path.c_str());
        if (pfile != nullptr)
        {
            pfile->Close();
            delete pfile;
        }
        if (pSession != nullptr)
            pSession->Close();
        succeed = false;
        e->Delete();        //没有这句会造成内存泄露
        SAFE_DELETE(pSession);
    }
    SAFE_DELETE(pSession);
    return succeed;
}

std::wstring CCommon::URLEncode(const std::wstring& wstr)
{
    std::string str_utf8;
    std::wstring result{};
    wchar_t buff[4];
    str_utf8 = CCommon::UnicodeToStr(wstr.c_str(), true);
    for (const auto& ch : str_utf8)
    {
        if (ch == ' ')
            result.push_back(L'+');
        else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
            result.push_back(static_cast<wchar_t>(ch));
        else if (ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*'/* || ch == '\''*/ || ch == '(' || ch == ')')
            result.push_back(static_cast<wchar_t>(ch));
        else
        {
            swprintf_s(buff, L"%%%x", static_cast<unsigned char>(ch));
            result += buff;
        }
    }
    return result;
}

void CCommon::WriteLog(const WORD w, LPCTSTR file_path)
{
    char buff[32];
    sprintf_s(buff, "%d", w);
    CCommon::WriteLog(buff, file_path);
}

void CCommon::WriteLog(const char* str_text, LPCTSTR file_path)
{
    static std::string last_text;
    //过滤相同内容的日志
    if (last_text != str_text)
    {
        SYSTEMTIME cur_time;
        GetLocalTime(&cur_time);
        char buff[32];
        sprintf_s(buff, "%d/%.2d/%.2d %.2d:%.2d:%.2d.%.3d: ", cur_time.wYear, cur_time.wMonth, cur_time.wDay,
            cur_time.wHour, cur_time.wMinute, cur_time.wSecond, cur_time.wMilliseconds);
        std::ofstream file{ file_path, std::ios::app };  //以追加的方式打开日志文件
        file << buff;
        file << str_text << std::endl;

        last_text = str_text;
    }
}

void CCommon::WriteLog(const wchar_t* str_text, LPCTSTR file_path)
{
    WriteLog(UnicodeToStr(str_text, true).c_str(), file_path);
}

std::vector<std::string> CCommon::split(const std::string& str, const char pattern)
{
    std::vector<std::string> res;
    if (str.size() <= 0) {
        return res;
    }
    if (str.find(pattern) == -1) {
        res.push_back(str);
        return res;
    }
    std::stringstream input(str);   //读取str到字符串流中
    std::string temp;
    //使用getline函数从字符串流中读取,遇到分隔符时停止,和从cin中读取类似
    //注意,getline默认是可以读取空格的
    int len = 0;
    while (getline(input, temp, pattern))
    {
        res.push_back(temp);
        len++;
    }
    res.resize(len);
    return res;
}

std::vector<std::string> CCommon::split(std::string str, std::string pattern)
{
    std::string::size_type pos;
    std::vector<std::string> result;
    str += pattern;//扩展字符串以方便操作
    int size = static_cast<int>(str.size());
    for (int i = 0; i < size; i++)
    {
        pos = str.find(pattern, i);
        if (pos < size)
        {
            if (pos == 0)
            {
                i = static_cast<int>(pos + pattern.size() - 1);
                continue;
            }
            std::string s = str.substr(i, pos - i);
            result.push_back(s);
            i = static_cast<int>(pos + pattern.size() - 1);
        }
    }
    return result;
}

std::wstring CCommon::vectorJoinString(const std::vector<std::wstring> data, const std::wstring& pattern)
{
    std::wstring str{};
    for (size_t index = 0; index < data.size(); index++)
    {
        if (index > 0)
            str.append(pattern);
        str.append(data[index]);
    }
    return str;
}
