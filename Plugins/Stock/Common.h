#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string.h>
#include <atltime.h>

// 当前界面视图模式
enum UIViewMode
{
	UI_VIEW_OVERVIEW = 0,   // 总览界面
	UI_VIEW_AUCTION,		// 集合竞价界面
	UI_VIEW_TIMELINE,		// 分时图界面
	UI_VIEW_MIN5_KLINE,		// 5分钟K线界面
	UI_VIEW_MIN30_KLINE,	// 30分钟K线界面
	UI_VIEW_DAY_KLINE,		// 日K线界面
};

extern const COLORREF AVG_RED_COLORS[];
extern const COLORREF AVG_GREEN_COLORS[];

using namespace::std;
// Log0("这是调试信息！\n")
#define Log0(fmt) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt));OutputDebugString(sOut);}
// Log1("这是调试信息%d\n", 10)
#define Log1(fmt,var) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var);OutputDebugString(sOut);}
// Log2("这是调试信息%d--%d\n", 10, 10 + 1)
#define Log2(fmt,var1,var2) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var1,var2);OutputDebugString(sOut);}
// Log3("这是调试信息%d--%d--%d\n", 10, 10 + 1, 10 + 2)
#define Log3(fmt,var1,var2,var3) {TCHAR sOut[256];_stprintf_s(sOut,_T(fmt),var1,var2,var3);OutputDebugString(sOut);}
// LogX(_T("error %d occured at %d line!\n"), 1170, 400)
static void LogX(LPCTSTR pstrFormat, ...)
{
	//ATLTRACE(_T("In %s ...\n"), __FUNCTIONW__);//函数名
	//ATLTRACE(_T("Run to %d ...\n"), __LINE__);	//函数行数
	CTime timeWrite;
	timeWrite = CTime::GetCurrentTime();
	CString str = timeWrite.Format(_T("%d %b %y %H:%M:%S - "));
	ATLTRACE(str);

	va_list args;
	va_start(args, pstrFormat);
	str.FormatV(pstrFormat, args);
	ATLTRACE(str);

	return;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t)
{
	std::stringstream str;
	str << t;
	out_type result;
	str >> result;
	return result;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t, bool bISWSring) //转wchar，wstring要用到这个
{
	std::wstringstream str;
	str << t;
	out_type result;
	str >> result;
	return result;
}

class CCommon
{
public:
	//将const char*字符串转换成宽字符字符串
	static std::wstring StrToUnicode(const char* str, bool utf8 = false);

	static std::string UnicodeToStr(const wchar_t* wstr, bool utf8 = false);

	//获取URL的内容
	static bool GetURL(const std::wstring& url, std::string& result, bool utf8 = false, LPCTSTR pstrAgent = NULL, const LPCTSTR headers = NULL, DWORD dwHeadersLength = 0);

	//将一个日志信息str_text写入到file_path文件中
	static void WriteLog(const char* str_text, LPCTSTR file_path = nullptr);
	static void WriteLog(const wchar_t* str_text, LPCTSTR file_path = nullptr);
	// 字符串拆分
	static std::vector<std::string> split(const std::string& str, const char pattern);
	static std::vector<std::string> split(const std::string& str, const std::string& delimiter);
	static std::wstring vectorJoinString(const std::vector<std::wstring> data, const std::wstring& pattern);
	static std::string removeChar(const std::string& str, char ch);
	static std::string removeStr(const std::string str, const std::string del);

	// 格式化价格为 CString，至少保留2位小数，最多保留3位小数
	// 当第三位为0时去掉，例如：1.234→"1.234", 1.230→"1.23", 1.2→"1.20"
	static CString FormatFloat(double value);

	// 格式化价格为 CString，保留3位小数
	static CString FormatETFPrice(double value);

	// 格式化数字，最多保留指定小数位数，末尾0自动去除
	// 例如：123.456→"123.46", 123.00→"123", 123.40→"123.4"
	static CString FormatNumber(double value, int maxDecimals);

	// 金额格式化：>=1亿显示"X.XX亿"，>=1万显示"X.XX万"，否则显示具体数字
	static CString FormatAmount(double value);

	// 成交量格式化：>=1万显示"X.XX万"，否则显示具体数字
	static CString FormatVolume(double value);
	// 成交量格式化（整型版）：>=1万显示"X.XX万"，否则显示整数
	static CString FormatVolumeInt(double value);

	// 盈亏格式化：带正负号，可指定是否先显示百分比
	// showPercentFirst=true: "+X.XX%(+Y)" 或 "X.XX%(-Y)"
	// showPercentFirst=false: "+Y(+X.XX%)" 或 "-Y(X.XX%)"
	static CString FormatProfitLoss(double percent, double amount, bool showPercentFirst = true);

	// 带正负号的数值格式化：正数带"+"前缀，负数不带额外符号
	static CString FormatSignedValue(double value, const CString& format = _T("%.2f"));

	// 带正负号的数值格式化：正数带"+"前缀，负数不带额外符号
	static CString prefixFormat(double value, const CString& suffix = _T(""));

	// 判断股票代码是否为A股代码
	static bool IsAGStockCode(const std::wstring& code);
	// 判断股票代码是否为基金/ETF类标的
	static bool IsFundCode(const std::wstring& code);

	// 根据涨跌幅百分比获取颜色
	// >= 5%: 紫色, 0%~5%: 红色, -5%~0%: 绿色, <= -5%: 墨绿色
	static COLORREF GetProfitLossColor(double percent);

	// 判断当前是否在交易时间（A股：9:15-11:30, 12:55-15:00，非周末）
	static bool IsMarketSession();

	// 判断当前是否在集合竞价时段（9:15-9:30，非周末）
	static bool IsCallAuctionSession();

	// 将真实时间转换为交易分钟序号（9:30=0, ..., 11:30=119, 13:00=120, ..., 14:59=239）
	static int GetTradingMinute(time_t t);
	static int GetTradingMinute(int hour, int minute);

	// 获取今天的日期字符串，格式为"YYYY-MM-DD"
	static std::string GetTodayDate();
	// 输入“HH:MM”时间字符串，返回减去1分钟后的时间字符串，格式为"HH:MM"
	static std::string subtractOneMinuteFast(const std::string& s);

	static std::string get_stock_trade_date();
};

//通过构造函数传递一个bool变量的引用，在构造时将其置为true，析构时置为false
class CFlagLocker
{
public:
	CFlagLocker(bool& flag)
		: m_flag(flag)
	{
		m_flag = true;
	}

	~CFlagLocker()
	{
		m_flag = false;
	}

private:
	bool& m_flag;
};
