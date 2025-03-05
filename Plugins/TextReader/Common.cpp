#include "pch.h"
#include "Common.h"

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

bool CCommon::IsUTF8Bytes(const char* data)
{
    int charByteCounter = 1;  //计算当前正分析的字符应还有的字节数
    unsigned char curByte; //当前分析的字节.
    bool ascii = true;
    int length = static_cast<int>(strlen(data));
    for (int i = 0; i < length; i++)
    {
        curByte = static_cast<unsigned char>(data[i]);
        if (charByteCounter == 1)
        {
            if (curByte >= 0x80)
            {
                ascii = false;
                //判断当前
                while (((curByte <<= 1) & 0x80) != 0)
                {
                    charByteCounter++;
                }
                //标记位首位若为非0 则至少以2个1开始 如:110XXXXX...........1111110X
                if (charByteCounter == 1 || charByteCounter > 6)
                {
                    return false;
                }
            }
        }
        else
        {
            //若是UTF-8 此时第一位必须为1
            if ((curByte & 0xC0) != 0x80)
            {
                return false;
            }
            charByteCounter--;
        }
    }
    if (ascii) return false;        //如果全是ASCII字符，返回false
    else return true;
}

void CCommon::StringSplit(const std::wstring& str, const std::wstring& div_str, std::vector<std::wstring>& results, bool skip_empty)
{
    results.clear();
    size_t split_index = 0 - div_str.size();
    size_t last_split_index = 0 - div_str.size();
    while (true)
    {
        split_index = str.find(div_str, split_index + div_str.size());
        std::wstring split_str = str.substr(last_split_index + div_str.size(), split_index - last_split_index - div_str.size());
        if (!split_str.empty() || !skip_empty)
            results.push_back(split_str);
        if (split_index == std::wstring::npos)
            break;
        last_split_index = split_index;
    }
}

bool CCommon::GetFileLastModified(const std::wstring& file_path, unsigned __int64& modified_time)
{
    WIN32_FILE_ATTRIBUTE_DATA file_attributes{};
    if (GetFileAttributesEx(file_path.c_str(), GetFileExInfoStandard, &file_attributes))
    {
        ULARGE_INTEGER last_modified_time{};
        last_modified_time.HighPart = file_attributes.ftLastWriteTime.dwHighDateTime;
        last_modified_time.LowPart = file_attributes.ftLastWriteTime.dwLowDateTime;
        modified_time = last_modified_time.QuadPart;
        return true;
    }
    return false;
}
