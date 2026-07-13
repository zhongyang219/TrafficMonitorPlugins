#pragma once
#include <string>
class CCommon
{
public:
    //将const char*字符串转换成宽字符字符串
    static std::wstring StrToUnicode(const char* str, bool utf8 = false);

    static std::string UnicodeToStr(const wchar_t* wstr, bool utf8 = false);

    //获取URL的内容
    /**
     * @brief 获取URL的内容
     * @param url 链接
     * @param[out] result 接收返回的结果
     * @param user_agent
     * @param force_reload 是否强制从服务器下载数据
     * @return 
     */
    static bool GetURL(const std::wstring& url, std::string& result, const std::wstring& user_agent = std::wstring(), bool force_reload = false);

    //将一个字符串转换成URL编码（以UTF8编码格式）
    static std::wstring URLEncode(const std::wstring& wstr);

    //去掉日期部分仅保留时间部分
    static CTime GetDateOnly(CTime time);

    //根据日期计算星期(0~6：星期日~星期六)
    static int CaculateWeekDay(int y, int m, int d);

    //从资源加载自定义文本资源。id：资源的ID，code_type：文本的编码格式：0:ANSI, 1:UTF8, 2:UTF16
    static CString GetTextResource(UINT id, int code_type);

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
