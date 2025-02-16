#include "pch.h"
#include "BookmarkMgr.h"
#include "Common.h"

const std::set<int>& CBookmarkMgr::GetBookmark(const std::wstring& file_path)
{
    return m_bookmarks[file_path];
}

void CBookmarkMgr::AddBookmark(const std::wstring& file_path, int pos)
{
    m_bookmarks[file_path].insert(pos);
}

void CBookmarkMgr::LoadFromConfig(const std::wstring& config_file_path)
{
    TCHAR buff[MAX_PATH];
    GetPrivateProfileString(_T("config"), _T("bookmarks"), _T(""), buff, MAX_PATH, config_file_path.c_str());
    std::wstring bookmarks_raw_str = buff;
    std::vector<std::wstring> bookmarks_by_files;
    CCommon::StringSplit(bookmarks_raw_str, L"||", bookmarks_by_files);
    for (const auto& file_bookmarks : bookmarks_by_files)
    {
        std::vector<std::wstring> bookmarks_a_file;
        CCommon::StringSplit(file_bookmarks, L"|", bookmarks_a_file);
        if (bookmarks_a_file.empty())
            continue;
        std::wstring file_name = bookmarks_a_file[0];
        if (file_name.empty())
            continue;
        std::set<int> bookmark_indexes;
        if (bookmarks_a_file.size() >= 2)
        {
            std::vector<std::wstring> indexes;
            CCommon::StringSplit(bookmarks_a_file[1], L",", indexes);
            for (const auto& str_index : indexes)
            {
                bookmark_indexes.insert(_wtoi(str_index.c_str()));
            }
        }
        m_bookmarks[file_name] = bookmark_indexes;
    }
}

void CBookmarkMgr::SaveToConfig(const std::wstring& config_file_path) const
{
    std::wstring str_save;
    for (const auto& file_bookmark : m_bookmarks)
    {
        if (file_bookmark.first.empty() || file_bookmark.second.empty())
            continue;
        str_save += file_bookmark.first;
        str_save += L"|";
        for (const auto& index : file_bookmark.second)
        {
            wchar_t buff[16];
            swprintf_s(buff, 16, L"%d", index);
            str_save += buff;
            str_save += L',';
        }
        str_save.pop_back();
        str_save += L"||";
    }
    WritePrivateProfileString(_T("config"), _T("bookmarks"), str_save.c_str(), config_file_path.c_str());

}
