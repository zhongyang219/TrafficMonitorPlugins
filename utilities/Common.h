#pragma once
#include <string>
#include <vector>

namespace utilities
{
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

        template<class T>
        static void StringNormalize(T& str)
        {
            if (str.empty()) return;

            size_t size = str.size();  //字符串的长度
            if (size < 0) return;
            size_t index1 = 0;     //字符串中第1个不是空格或控制字符的位置
            size_t index2 = size - 1;  //字符串中最后一个不是空格或控制字符的位置
            while (index1 < size && str[index1] >= 0 && str[index1] <= 32)
                index1++;
            while (index2 >= 0 && str[index2] >= 0 && str[index2] <= 32)
                index2--;
            if (index1 > index2)    //如果index1 > index2，说明字符串全是空格或控制字符
                str.clear();
            else if (index1 == 0 && index2 == size - 1) //如果index1和index2的值分别为0和size - 1，说明字符串前后没有空格或控制字符，直接返回
                return;
            else
                str = str.substr(index1, index2 - index1 + 1);
        }

        //将一个字符串分割成若干个字符（模板类型只能为string或wstring）
        //str: 原始字符串
        //div_ch: 用于分割的字符
        //result: 接收分割后的结果
        template<class T>
        static void StringSplit(const T& str, wchar_t div_ch, std::vector<T>& results, bool skip_empty = true, bool trim = true)
        {
            results.clear();
            size_t split_index = -1;
            size_t last_split_index = -1;
            while (true)
            {
                split_index = str.find(div_ch, split_index + 1);
                T split_str = str.substr(last_split_index + 1, split_index - last_split_index - 1);
                if (trim)
                    StringNormalize(split_str);
                if (!split_str.empty() || !skip_empty)
                    results.push_back(split_str);
                if (split_index == std::wstring::npos)
                    break;
                last_split_index = split_index;
            }
        }

        template<class T>
        static void StringSplit(const T& str, const T& div_str, std::vector<T>& results, bool skip_empty = true, bool trim = true)
        {
            results.clear();
            size_t split_index = 0 - div_str.size();
            size_t last_split_index = 0 - div_str.size();
            while (true)
            {
                split_index = str.find(div_str, split_index + div_str.size());
                T split_str = str.substr(last_split_index + div_str.size(), split_index - last_split_index - div_str.size());
                if (trim)
                    StringNormalize(split_str);
                if (!split_str.empty() || !skip_empty)
                    results.push_back(split_str);
                if (split_index == std::wstring::npos)
                    break;
                last_split_index = split_index;
            }
        }

    };
}

