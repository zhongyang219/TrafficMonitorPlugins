#include "Common.h"
#include <windows.h>
#include <fstream>
#include <io.h>

namespace utilities
{
    CCommon::CCommon()
    {
    }


    CCommon::~CCommon()
    {
    }

    std::wstring CCommon::StrToUnicode(const char* str, bool utf8 /*= false*/)
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

    std::string CCommon::UnicodeToStr(const wchar_t* wstr, bool utf8 /*= false*/)
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

    bool CCommon::GetFileContent(const wchar_t* file_path, std::string& contents_buff)
    {
        std::ifstream file{ file_path, std::ios::binary };
        if (file.fail())
            return false;
        //获取文件长度
        file.seekg(0, file.end);
        size_t length = file.tellg();
        file.seekg(0, file.beg);

        char* buff = new char[length];
        file.read(buff, length);
        file.close();

        contents_buff.assign(buff, length);
        delete[] buff;

        return true;
    }


    const char* CCommon::GetFileContent(const wchar_t* file_path, size_t& length)
    {
        std::ifstream file{ file_path, std::ios::binary };
        length = 0;
        if (file.fail())
            return nullptr;
        //获取文件长度
        file.seekg(0, file.end);
        length = file.tellg();
        file.seekg(0, file.beg);

        char* buff = new char[length];
        file.read(buff, length);
        file.close();

        return buff;
    }


    void CCommon::GetFiles(const wchar_t* path, std::vector<std::wstring>& files)
    {
        //文件句柄
        intptr_t hFile = 0;
        //文件信息
        _wfinddata_t fileinfo;
        if ((hFile = _wfindfirst(path, &fileinfo)) != -1)
        {
            do
            {
                std::wstring file_name(fileinfo.name);
                if (file_name != L"." && file_name != L"..")
                    files.push_back(file_name);  //将文件名保存(忽略"."和"..")
            } while (_wfindnext(hFile, &fileinfo) == 0);
        }
        _findclose(hFile);
    }
}