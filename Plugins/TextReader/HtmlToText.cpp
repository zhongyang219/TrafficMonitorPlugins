#include "pch.h"
#include "HtmlToText.h"
#include "../utilities/Common.h"
#include "Common.h"
#include <regex>
#include <vector>

CHtmlToText::CHtmlToText()
{
}

bool CHtmlToText::ParseFromUrl(const std::wstring& url)
{
	//获取url内容
	std::string html;
	if (!CCommon::GetURL(url, html, true))
		return false;

	//转换成文本
	std::string text = HtmlToText(html);
	m_text = utilities::StringHelper::StrToUnicode(text.c_str(), true);

	return true;
}

bool CHtmlToText::ParseFromFile(const std::wstring& file_path)
{
	//读取文件内容
	std::string html;
	if (!utilities::CCommon::GetFileContent(file_path.c_str(), html))
		return false;

	//转换成文本
	std::string text = HtmlToText(html);
	m_text = utilities::StringHelper::StrToUnicode(text.c_str(), true);

	return false;
}

const std::wstring& CHtmlToText::GetText()
{
	return m_text;
}

std::string CHtmlToText::HtmlToText(const std::string& html)
{
	//提取p和h标签
	std::regex pattern(R"(<(p|h\d*).*?>.*?</\1>)", std::regex_constants::ECMAScript);
	std::sregex_iterator it(html.begin(), html.end(), pattern);
	std::sregex_iterator end;

	std::string text;
	for (; it != end; ++it)
	{
		std::smatch match = *it;
		std::string content = match.str();
		// 去除内容中的 HTML 标签
		std::regex innerTagPattern(R"(</?[^>]+>)", std::regex_constants::ECMAScript);
		std::string content_text = std::regex_replace(content, innerTagPattern, "");
		if (!content_text.empty())
		{
			text += ' ';
			text += content_text;
		}
	}

	// 处理 HTML 转义字符
	std::regex amp_regex("&amp;");
	text = std::regex_replace(text, amp_regex, "&");

	std::regex lt_regex("&lt;");
	text = std::regex_replace(text, lt_regex, "<");

	std::regex gt_regex("&gt;");
	text = std::regex_replace(text, gt_regex, ">");

	std::regex quot_regex("&quot;");
	text = std::regex_replace(text, quot_regex, "\"");

	std::regex apostrophe_regex("&apos;");
	text = std::regex_replace(text, apostrophe_regex, "'");

	return text;
}
