#pragma once
#include <string>
#include <vector>
#include "Variant.h"

namespace utilities
{
    class CCommon
    {
    public:
        //读取文件内容
        static bool GetFileContent(const wchar_t* file_path, std::string& contents_buff);

        //读取文件内容
        //file_path: 文件的路径
        //length: 文件的长度
        //返回值: 读取成功返回读取到的文件内容的const char类型的指针，在使用完毕后这个指针需要自行使用delete释放。读取失败返回nullptr
        static const char* GetFileContent(const wchar_t* file_path, size_t& length);

        static void GetFiles(const wchar_t* path, std::vector<std::wstring>& files);
    };

    /////////////////////////////////////////////////////////////////////////////////////////
    class StringHelper
    {
    public:
        static std::wstring StrToUnicode(const char* str, bool utf8 = false);

        static std::string UnicodeToStr(const wchar_t* wstr, bool utf8 = false);

        //字符串替换
        static bool StringReplace(std::wstring& str, const std::wstring& str_old, const std::wstring& str_new);

        //安全的格式化字符串，将format_str中形如<%序号%>的字符串替换成初始化列表paras中的元素，元素支持int/size_t/double/const wchar_t*/std::wstring格式，序号从1开始
        static std::wstring StringFormat(const wchar_t* format_str, const std::initializer_list<CVariant>& paras);

        static void StringNormalize(std::wstring& str);
        static void StringNormalize(std::string& str);

        //将一个字符串分割成若干个字符
        //str: 原始字符串
        //div_ch: 用于分割的字符
        //result: 接收分割后的结果
        static void StringSplit(const std::wstring& str, wchar_t div_ch, std::vector<std::wstring>& results, bool skip_empty = true, bool trim = true);
        static void StringSplit(const std::string& str, char div_ch, std::vector<std::string>& results, bool skip_empty = true, bool trim = true);

        static void StringSplit(const std::wstring& str, const std::wstring& div_str, std::vector<std::wstring>& results, bool skip_empty = true, bool trim = true);
        static void StringSplit(const std::string& str, const std::string& div_str, std::vector<std::string>& results, bool skip_empty = true, bool trim = true);

    };
}

