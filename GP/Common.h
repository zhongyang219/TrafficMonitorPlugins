#pragma once
#include <string>
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
