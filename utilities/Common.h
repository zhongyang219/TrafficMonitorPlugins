#pragma once
#include <string>
#include <vector>

class CCommon
{
public:
    CCommon();
    ~CCommon();

    static std::wstring StrToUnicode(const char* str, bool utf8 = false);

    static std::string UnicodeToStr(const wchar_t* wstr, bool utf8 = false);

    //读取文件内容
    static bool GetFileContent(const wchar_t* file_path, std::string& contents_buff);

    //读取文件内容
    //file_path: 文件的路径
    //length: 文件的长度
    //返回值: 读取成功返回读取到的文件内容的const char类型的指针，在使用完毕后这个指针需要自行使用delete释放。读取失败返回nullptr
    static const char* GetFileContent(const wchar_t* file_path, size_t& length);

    static void GetFiles(const wchar_t* path, std::vector<std::wstring>& files);
};

