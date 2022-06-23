#include "pch.h"
#include "ChapterParser.h"


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
        index = m_contents.find(L"ตฺ", index + 1);
        if (index == std::wstring::npos)
            break;
        size_t index1 = m_contents.find(L"ีย", index + 2);
        if (index1 == std::wstring::npos)
            break;
        if (index1 - index < 10)
        {
            size_t index2 = m_contents.find_first_of(L"\r\n", index1 + 1);
            if (index2 - index1 < 60)
            {
                std::wstring title = m_contents.substr(index, index2 - index);
                m_chapter[index] = title;
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
                m_chapter[index] = title;
                index = index2 + 1;
            }
        }

    }
}

const std::map<int, std::wstring>& CChapterParser::GetChapterData() const
{
    return m_chapter;
}

std::wstring CChapterParser::GetChapterByPos(int pos) const
{
    int cur_chapter_position{};
    for (const auto& item : m_chapter)
    {
        if (item.first >= pos)
        {
            return item.second;
        }
    }
    return std::wstring();
}
