#include "pch.h"
#include "ChapterParser.h"
#include "DataManager.h"


CChapterParser::CChapterParser(const std::wstring& contents)
    : m_contents{ contents }
{
}


CChapterParser::~CChapterParser()
{
}

void CChapterParser::Parse()
{
    m_chapter.clear();
    size_t index = -1;
    while (true)
    {
        index = m_contents.find(L"第", index + 1);
        if (index == std::wstring::npos)
            break;
        size_t index1 = m_contents.find(L"章", index + 2);
        if (index1 == std::wstring::npos)
            break;
        if (index1 - index < 10)
        {
            size_t index2 = m_contents.find_first_of(L"\r\n", index1 + 1);
            if (index2 - index1 < 60)
            {
                std::wstring title = m_contents.substr(index, index2 - index);
                m_chapter[static_cast<int>(index)] = title;
                index = index2 + 1;
            }
        }

    }
    
    if (m_chapter.empty())
    {
        index = -1;
        while (true)
        {
            index = m_contents.find(L"Chapter", index + 1);
            if (index == std::wstring::npos)
                break;
            size_t index2 = m_contents.find_first_of(L"\r\n", index + 1);
            if (index2 - index < 60)
            {
                std::wstring title = m_contents.substr(index, index2 - index);
                m_chapter[static_cast<int>(index)] = title;
                index = index2 + 1;
            }
        }

    }
}

const std::map<int, std::wstring>& CChapterParser::GetChapterData() const
{
    return m_chapter;
}

std::wstring CChapterParser::GetChapterTitle(int chapter_index) const
{
    if (chapter_index >= 0)
    {
        auto iter = m_chapter.find(chapter_index);
        if (iter != m_chapter.end())
            return iter->second;
    }
    return std::wstring();
}

std::wstring CChapterParser::GetCurrentChapterTitle() const
{
    int chapter_index = GetChapterIndexByPos(g_data.m_setting_data.current_position);
    return GetChapterTitle(chapter_index);
}

int CChapterParser::GetChapterIndexByPos(int pos) const
{
    for (auto iter = m_chapter.begin(); iter != m_chapter.end(); ++iter)
    {
        if (iter->first >= pos)
        {
            if (iter == m_chapter.begin())
            {
                return iter->first;
            }
            else
            {
                auto privious_iter = iter;
                return (--privious_iter)->first;
            }
        }

    }
    return -1;
}

std::wstring CChapterParser::GetChapterByPos(int pos) const
{
    int chapter_index = GetChapterIndexByPos(pos);
    return GetChapterTitle(chapter_index);
}
