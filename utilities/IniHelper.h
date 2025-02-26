//ini读写类
//使用时将ini文件路径通过构造函数参数传递
//在向ini文件写入数据时，需要在最后调用Save()函数以将更改保存到文件
//默认以UTF8_BOM编码保存，如果要以ANSI保存，请调用SetSaveAsUTF8(false);
#pragma once
#include <string>
#include <vector>

namespace utilities
{
    class CIniHelper
    {
    public:
        CIniHelper(const std::wstring& file_path);
        ~CIniHelper();

        bool IsEmpty() const;
        void SetSaveAsUTF8(bool utf8);

        void WriteString(const wchar_t* AppName, const wchar_t* KeyName, const std::wstring& str);
        std::wstring GetString(const wchar_t* AppName, const wchar_t* KeyName, const wchar_t* default_str = L"") const;
        void WriteInt(const wchar_t * AppName, const wchar_t * KeyName, int value);
        int GetInt(const wchar_t * AppName, const wchar_t * KeyName, int default_value = 0) const;
        void WriteBool(const wchar_t * AppName, const wchar_t * KeyName, bool value);
        bool GetBool(const wchar_t * AppName, const wchar_t * KeyName, bool default_value = false) const;
        void WriteIntArray(const wchar_t * AppName, const wchar_t * KeyName, const int* values, int size);		//写入一个int数组，元素个数为size
        void GetIntArray(const wchar_t * AppName, const wchar_t * KeyName, int* values, int size, int default_value = 0) const;		//读取一个int数组，储存到values，元素个数为size
        void WriteBoolArray(const wchar_t * AppName, const wchar_t * KeyName, const bool* values, int size);
        void GetBoolArray(const wchar_t * AppName, const wchar_t * KeyName, bool* values, int size, bool default_value = false) const;
        void WriteStringList(const wchar_t* AppName, const wchar_t* KeyName, const std::vector<std::wstring>& values);      //写入一个字符串列表，由于保存到ini文件中时字符串前后会加上引号，所以字符串中不能包含引号
        void GetStringList(const wchar_t* AppName, const wchar_t* KeyName, std::vector<std::wstring>& values, const std::vector<std::wstring>& default_value) const;

        bool Save();		//将ini文件保存到文件，成功返回true

    protected:
        std::wstring m_file_path;
        std::wstring m_ini_str;
        bool m_save_as_utf8{ true };		//是否以及UTF8编码保存

        void _WriteString(const wchar_t* AppName, const wchar_t* KeyName, const std::wstring& str);
        std::wstring _GetString(const wchar_t* AppName, const wchar_t* KeyName, const wchar_t* default_str) const;

        static std::wstring MergeStringList(const std::vector<std::wstring>& values);
        static void SplitStringList(std::vector<std::wstring>& values, std::wstring str_value);
    };
}
