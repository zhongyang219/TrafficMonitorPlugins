#pragma once
#include <string>
#include <map>
class CChapterParser
{
public:
    CChapterParser(const std::wstring& contents);
    ~CChapterParser();
    void Parse();
    const std::map<int, std::wstring>& GetChapterData() const;
    std::wstring GetChapterTitle(int chapter_index) const;
    std::wstring GetCurrentChapterTitle() const;
    int GetChapterIndexByPos(int pos) const;
    std::wstring GetChapterByPos(int pos) const;

private:
    const std::wstring& m_contents;
    std::map<int, std::wstring> m_chapter;      //章节标题。key中索引，value为章节标题
};
