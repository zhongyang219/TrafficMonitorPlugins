#pragma once
#include <string>

namespace utilities
{
    class CFilePathHelper
    {
    public:
        CFilePathHelper(const std::wstring& file_path);
        CFilePathHelper() {}
        ~CFilePathHelper();

        void SetFilePath(const std::wstring& file_path) { m_file_path = file_path; }

        std::wstring GetFileExtension(bool width_dot = false) const;		//获取文件的扩展名(width_dot:是否包含“.”)
        std::wstring GetFileName() const;							//获取文件名
        std::wstring GetFileNameWithoutExtension() const;			//获取文件名（不含扩展名）
        std::wstring GetFolderName() const;							//获取文件夹名
        std::wstring GetDir() const;									//获取目录
        std::wstring GetParentDir() const;							//获取上级目录
        std::wstring GetFilePath() const { return m_file_path; }		//获取完整路径
        const std::wstring& ReplaceFileExtension(const wchar_t* new_extension);		//替换文件的扩展名，返回文件完整路径
        std::wstring GetFilePathWithoutExtension() const;            //获取文件路径（不含扩展名）
    protected:
        std::wstring m_file_path;
    };

}