#pragma once
#include <string>
class CHtmlToText
{
public:
	CHtmlToText();
	bool ParseFromUrl(const std::wstring& url);
	bool ParseFromFile(const std::wstring& file_path);
	const std::wstring& GetText();

private:
	std::string HtmlToText(const std::string& html);

private:
	std::wstring m_text;
};

