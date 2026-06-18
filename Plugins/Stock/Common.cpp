#include "pch.h"
#include "Common.h"
#include <afxinet.h>    //用于支持使用网络相关的类
#include <sstream>
#include "DataManager.h"

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

bool CCommon::GetURL(const std::wstring& url, std::string& result, bool utf8, LPCTSTR user_agent, LPCTSTR headers, DWORD dwHeadersLength)
{
	bool succeed{ false };
	CInternetSession* pSession{};
	CHttpFile* pfile{};
	try
	{
		pSession = new CInternetSession(user_agent);
		//pfile = (CHttpFile*)pSession->OpenURL(url.c_str());
		//CCommon::WriteLog(L"request>>>>", g_data.m_log_path.c_str());
		pfile = (CHttpFile*)pSession->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, headers, dwHeadersLength);
		//CCommon::WriteLog(L"request<<<", g_data.m_log_path.c_str());
		DWORD dwStatusCode;
		pfile->QueryInfoStatusCode(dwStatusCode);
		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString content;
			CString data;
			while (pfile->ReadString(data))
			{
				content += data;
			}
			result = (const char*)content.GetString();
			succeed = true;
		}
		pfile->Close();
		delete pfile;
		pSession->Close();
	}
	catch (CInternetException* e)
	{
		CCommon::WriteLog(L"request fail!", g_data.m_log_path.c_str());
		if (pfile != nullptr)
		{
			pfile->Close();
			delete pfile;
		}
		if (pSession != nullptr)
			pSession->Close();
		succeed = false;
		e->Delete();        //没有这句会造成内存泄露
		SAFE_DELETE(pSession);
	}
	SAFE_DELETE(pSession);
	return succeed;
}

std::wstring CCommon::URLEncode(const std::wstring& wstr)
{
	std::string str_utf8;
	std::wstring result{};
	wchar_t buff[4];
	str_utf8 = CCommon::UnicodeToStr(wstr.c_str(), true);
	for (const auto& ch : str_utf8)
	{
		if (ch == ' ')
			result.push_back(L'+');
		else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
			result.push_back(static_cast<wchar_t>(ch));
		else if (ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*'/* || ch == '\''*/ || ch == '(' || ch == ')')
			result.push_back(static_cast<wchar_t>(ch));
		else
		{
			swprintf_s(buff, L"%%%x", static_cast<unsigned char>(ch));
			result += buff;
		}
	}
	return result;
}

void CCommon::WriteLog(const WORD w, LPCTSTR file_path)
{
	char buff[32];
	sprintf_s(buff, "%d", w);
	CCommon::WriteLog(buff, file_path);
}

void CCommon::WriteLog(const char* str_text, LPCTSTR file_path)
{
	static std::string last_text;
	//过滤相同内容的日志
	if (last_text != str_text)
	{
		SYSTEMTIME cur_time;
		GetLocalTime(&cur_time);
		char buff[32];
		sprintf_s(buff, "%d/%.2d/%.2d %.2d:%.2d:%.2d.%.3d: ", cur_time.wYear, cur_time.wMonth, cur_time.wDay,
			cur_time.wHour, cur_time.wMinute, cur_time.wSecond, cur_time.wMilliseconds);
		std::ofstream file{ file_path, std::ios::app };  //以追加的方式打开日志文件
		file << buff;
		file << str_text << std::endl;

		last_text = str_text;
	}
}

void CCommon::WriteLog(const wchar_t* str_text, LPCTSTR file_path)
{
	WriteLog(UnicodeToStr(str_text, true).c_str(), file_path);
}

std::vector<std::string> CCommon::split(const std::string& str, const char pattern)
{
	std::vector<std::string> res;
	if (str.size() <= 0) {
		return res;
	}
	if (str.find(pattern) == -1) {
		res.push_back(str);
		return res;
	}
	std::stringstream input(str);   //读取str到字符串流中
	std::string temp;
	//使用getline函数从字符串流中读取,遇到分隔符时停止,和从cin中读取类似
	//注意,getline默认是可以读取空格的
	int len = 0;
	while (getline(input, temp, pattern))
	{
		res.push_back(temp);
		len++;
	}
	res.resize(len);
	return res;
}

std::vector<std::string> CCommon::split(const std::string& str, const std::string& delimiter) {
	std::vector<std::string> tokens;

	if (delimiter.empty()) {
		tokens.push_back(str);
		return tokens;
	}

	size_t pos = 0;
	size_t prev = 0;

	while ((pos = str.find(delimiter, prev)) != std::string::npos) {
		tokens.push_back(str.substr(prev, pos - prev));
		prev = pos + delimiter.length();
	}

	// 添加最后一个片段
	tokens.push_back(str.substr(prev));

	return tokens;
}

std::wstring CCommon::vectorJoinString(const std::vector<std::wstring> data, const std::wstring& pattern)
{
	std::wstring str{};
	for (size_t index = 0; index < data.size(); index++)
	{
		if (index > 0)
			str.append(pattern);
		str.append(data[index]);
	}
	return str;
}

std::string CCommon::removeChar(const std::string& str, char ch)
{
	std::string result;
	for (char c : str)
	{
		if (c != ch)
		{
			result += c;
		}
	}
	return result;
}

std::string CCommon::removeStr(const std::string str, const std::string del)
{
	std::string result;

	if (del.empty()) {
		return str;
	}

	size_t pos = 0;
	size_t prev = 0;

	while ((pos = str.find(del, prev)) != std::string::npos) {
		result += str.substr(prev, pos - prev);
		prev = pos + del.length();
	}

	result += str.substr(prev);

	return result;
}

CString CCommon::FormatFloat(float value)
{
	CString str;
	str.Format(_T("%.3f"), value);

	if (str.Right(1) == _T("0"))
	{
		str = str.Left(str.GetLength() - 1);
	}

	return str;
}

CString CCommon::FormatNumber(double value, int maxDecimals)
{
	CString str;
	if (maxDecimals <= 0)
	{
		str.Format(_T("%lld"), static_cast<long long>(value));
		return str;
	}

	TCHAR format[32];
	wsprintf(format, _T("%%.%df"), maxDecimals);
	str.Format(format, value);

	int dotPos = str.Find(_T('.'));
	if (dotPos != -1)
	{
		int lastNonZero = str.GetLength() - 1;
		while (lastNonZero > dotPos && str[lastNonZero] == _T('0'))
		{
			lastNonZero--;
		}

		if (lastNonZero == dotPos)
		{
			str = str.Left(dotPos);
		}
		else
		{
			str = str.Left(lastNonZero + 1);
		}
	}

	return str;
}

CString CCommon::FormatAmount(double value)
{
	CString str;
	if (value >= 100000000)
	{
		str = FormatNumber(value / 100000000.0, 2) + _T("亿");
	}
	else if (value >= 10000)
	{
		str = FormatNumber(value / 10000.0, 2) + _T("万");
	}
	else
	{
		str = FormatNumber(value, 2);
	}
	return str;
}

CString CCommon::FormatVolume(double value)
{
	CString str;
	if (value >= 10000)
	{
		str.Format(_T("%.2f万"), value / 10000.0);
	}
	else
	{
		str.Format(_T("%.0f"), value);
	}
	return str;
}

CString CCommon::FormatVolumeInt(double value)
{
	CString str;
	if (value >= 10000)
	{
		str = FormatNumber(value / 10000.0, 2) + _T("万");
	}
	else
	{
		str.Format(_T("%lld"), static_cast<long long>(value));
	}
	return str;
}

CString CCommon::FormatProfitLoss(double percent, double amount, bool showPercentFirst)
{
	CString str;
	if (showPercentFirst)
	{
		if (percent >= 0)
			str.Format(_T("+%.2f%%(+%g)"), percent, amount);
		else
			str.Format(_T("%.2f%%(%g)"), percent, amount);
	}
	else
	{
		if (amount >= 0)
			str.Format(_T("+%g(+%.2f%%)"), amount, percent);
		else
			str.Format(_T("%g(%.2f%%)"), amount, percent);
	}
	return str;
}

CString CCommon::FormatDate(int year, int month, int day)
{
	CString str;
	str.Format(_T("%04d-%02d-%02d"), year, month, day);
	return str;
}

bool CCommon::ParseDate(const CString& dateStr, int& year, int& month, int& day)
{
	return swscanf_s(dateStr.GetString(), L"%d-%d-%d", &year, &month, &day) == 3;
}

CString CCommon::FormatSignedValue(double value, const CString& format)
{
	CString str;
	if (value >= 0)
	{
		CString tmp;
		tmp.Format(format, value);
		str.Format(_T("+%s"), tmp.GetString());
	}
	else
	{
		str.Format(format, value);
	}
	return str;
}

COLORREF CCommon::GetProfitLossColor(double percent)
{
	const COLORREF COLOR_RED_UP = RGB(179, 64, 65);
	const COLORREF COLOR_GREEN_DOWN = RGB(44, 144, 51);
	const COLORREF COLOR_BG_PURPLE = RGB(160, 50, 160);
	const COLORREF COLOR_BG_DARK_GREEN = RGB(20, 100, 40);

	if (percent >= 5.0)
		return COLOR_BG_PURPLE;
	else if (percent > 0 && percent < 5.0)
		return COLOR_RED_UP;
	else if (percent >= -5.0 && percent < 0)
		return COLOR_GREEN_DOWN;
	else if (percent < -5.0)
		return COLOR_BG_DARK_GREEN;
	return RGB(0, 0, 0);
}