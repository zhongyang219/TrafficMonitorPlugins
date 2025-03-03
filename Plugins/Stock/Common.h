#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string.h>
#include <atltime.h>

using namespace::std;
// Log0("这是调试信息！\n")
#define Log0(fmt) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt));OutputDebugString(sOut);}
// Log1("这是调试信息%d\n", 10)
#define Log1(fmt,var) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var);OutputDebugString(sOut);}
// Log2("这是调试信息%d--%d\n", 10, 10 + 1)
#define Log2(fmt,var1,var2) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var1,var2);OutputDebugString(sOut);}
// Log3("这是调试信息%d--%d--%d\n", 10, 10 + 1, 10 + 2)
#define Log3(fmt,var1,var2,var3) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var1,var2,var3);OutputDebugString(sOut);}
// LogX(_T("error %d occured at %d line!\n"), 1170, 400)
static void LogX(LPCTSTR pstrFormat, ...)
{
    //ATLTRACE(_T("In %s ...\n"), __FUNCTIONW__);//函数名
    //ATLTRACE(_T("Run to %d ...\n"), __LINE__);	//函数行数
    CTime timeWrite;
    timeWrite = CTime::GetCurrentTime();
    CString str = timeWrite.Format(_T("%d %b %y %H:%M:%S - "));
    ATLTRACE(str);

    va_list args;
    va_start(args, pstrFormat);
    str.FormatV(pstrFormat, args);
    ATLTRACE(str);

    return;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t)
{
    std::stringstream str;
    str << t;
    out_type result;
    str >> result;
    return result;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t, bool bISWSring) //转wchar，wstring要用到这个
{
    std::wstringstream str;
    str << t;
    out_type result;
    str >> result;
    return result;
}

class CCommon
{
public:
    //将const char*字符串转换成宽字符字符串
    static std::wstring StrToUnicode(const char* str, bool utf8 = false);

    static std::string UnicodeToStr(const wchar_t* wstr, bool utf8 = false);

    //获取URL的内容
    static bool GetURL(const std::wstring& url, std::string& result, bool utf8 = false, LPCTSTR pstrAgent = NULL, const LPCTSTR headers = NULL, DWORD dwHeadersLength = 0);

    //将一个字符串转换成URL编码（以UTF8编码格式）
    static std::wstring URLEncode(const std::wstring& wstr);

    //将一个日志信息str_text写入到file_path文件中
    static void WriteLog(const WORD w, LPCTSTR file_path);
    static void WriteLog(const char* str_text, LPCTSTR file_path);
    static void WriteLog(const wchar_t* str_text, LPCTSTR file_path);
    // 字符串拆分
    static std::vector<std::string> split(const std::string& str, const char pattern);
    static std::vector<std::string> split(std::string str, std::string pattern);
    static std::wstring vectorJoinString(const std::vector<std::wstring> data, const std::wstring& pattern);
};


//通过构造函数传递一个bool变量的引用，在构造时将其置为true，析构时置为false
class CFlagLocker
{
public:
    CFlagLocker(bool& flag)
        : m_flag(flag)
    {
        m_flag = true;
    }

    ~CFlagLocker()
    {
        m_flag = false;
    }

private:
    bool& m_flag;
};
