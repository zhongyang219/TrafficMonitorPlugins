#pragma once
#include <map>
#include <string>
#include <set>

class CBookmarkMgr
{
public:
    const std::set<int>& GetBookmark(const std::wstring& file_path);     //获取一个文件的书签
    void AddBookmark(const std::wstring& file_path, int pos);           //添加一个书签
    void LoadFromConfig(const std::wstring& config_file_path);
    void SaveToConfig(const std::wstring& config_file_path) const;

private:
    std::map<std::wstring, std::set<int>> m_bookmarks;    //保存所有文件的书签
};

