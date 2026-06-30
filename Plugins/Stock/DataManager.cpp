#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include "../utilities/IniHelper.h"
#include <iomanip>
#include <afxinet.h>
#include "sqlite3.h"

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
	// 初始化DPI
	HDC hDC = ::GetDC(HWND_DESKTOP);
	m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
	::ReleaseDC(HWND_DESKTOP, hDC);
}

CDataManager::~CDataManager()
{
	SaveConfig();
	if (m_db != nullptr)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

CDataManager& CDataManager::Instance()
{
	return m_instance;
}

bool CDataManager::IsMarketOpen()
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	// 周六日休市
	if (now.wDayOfWeek == 0 || now.wDayOfWeek == 6)
		return false;
	// A股交易时间：9:30-11:30, 13:00-15:00
	int minutes = now.wHour * 60 + now.wMinute;
	if (minutes < 9 * 60 + 30)          // 9:30之前
		return false;
	if (minutes > 11 * 60 + 30 && minutes < 13 * 60)  // 11:30-13:00午休
		return false;
	if (minutes > 15 * 60)              // 15:00之后
		return false;
	return true;
}

int CDataManager::GetTradingMinute(int hour, int minute)
{
	int totalMinutes = hour * 60 + minute;
	if (totalMinutes < 9 * 60 + 30)
		return -1;
	if (totalMinutes <= 11 * 60 + 30)
		return totalMinutes - (9 * 60 + 30);
	if (totalMinutes < 13 * 60)
		return 120;  // 午休期间映射到11:30的位置
	if (totalMinutes <= 15 * 60)
		return 120 + (totalMinutes - 13 * 60);
	return -1;
}

int CDataManager::GetTradingMinute(time_t t)
{
	std::tm tm = {};
	localtime_s(&tm, &t);
	return GetTradingMinute(tm.tm_hour, tm.tm_min);
}

void CDataManager::ResetText()
{
	stockMarket.ClearRealtimeData();
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
	wchar_t buff[16];
	swprintf_s(buff, L"%d", value);
	WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
	// 获取模块的路径
	HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(hModule, path, MAX_PATH);
	std::wstring module_path = path;
	m_config_path = module_path;
	m_log_path = module_path;
	if (!config_dir.empty())
	{
		size_t index = module_path.find_last_of(L"\\/");
		// 模块的文件名
		std::wstring module_file_name = module_path.substr(index + 1);
		module_file_name = module_file_name.substr(0, module_file_name.find_last_of(L"."));
		m_config_path = config_dir + module_file_name;
		m_log_path = config_dir + module_file_name;
	}
	m_config_path += L".ini";
	m_log_path += L".log";

	utilities::CIniHelper ini(m_config_path);
	ini.GetStringList(L"config", L"stock_code", m_setting_data.m_stock_codes, std::vector<std::wstring>{});
	m_setting_data.m_full_day = ini.GetBool(L"config", L"full_day", true);
	m_setting_data.m_show_stock_name = ini.GetBool(L"config", L"show_stock_name", true);
	m_setting_data.m_show_fluctuation = ini.GetBool(L"config", L"show_fluctuation", true);
	m_setting_data.m_color_with_price = ini.GetBool(L"config", L"color_with_price", true);
	m_setting_data.m_kline_width = ini.GetInt(L"config", L"kline_width", 450);
	m_setting_data.m_kline_height = ini.GetInt(L"config", L"kline_height", 210);

	// 加载每个股票的关注价格
	m_stock_alert_prices.clear();
	// 加载每个股票的持仓配置
	m_stock_positions.clear();
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		std::wstring low_str = ini.GetString(code.c_str(), L"alert_low", L"");
		std::wstring high_str = ini.GetString(code.c_str(), L"alert_high", L"");
		double low = 0.0, high = 0.0;
		if (!low_str.empty()) low = std::stod(low_str);
		if (!high_str.empty()) high = std::stod(high_str);
		m_stock_alert_prices[code] = std::make_pair(low, high);

		std::wstring cost_str = ini.GetString(code.c_str(), L"cost_price", L"");
		std::wstring count_str = ini.GetString(code.c_str(), L"holding_count", L"");
		std::wstring buy_date = ini.GetString(code.c_str(), L"buy_date", L"");
		double cost = 0.0, count = 0.0;
		if (!cost_str.empty()) cost = std::stod(cost_str);
		if (!count_str.empty()) count = std::stod(count_str);
		m_stock_positions[code] = std::make_tuple(cost, count, buy_date);
	}

	InitDatabase();
	LoadTodayInnerOuterSnapshots();

	// 清理数据库中非今天的快照数据
	if (m_db != nullptr)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		std::tm todayTm = {};
		todayTm.tm_year = st.wYear - 1900;
		todayTm.tm_mon = st.wMonth - 1;
		todayTm.tm_mday = st.wDay;
		time_t dayStart = std::mktime(&todayTm);
		if (dayStart > 0)
		{
			const char* cleanSql = "DELETE FROM inner_outer_snapshots WHERE snapshot_time < ?;";
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(m_db, cleanSql, -1, &stmt, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(dayStart));
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
	}
}

void CDataManager::InitDatabase()
{
	if (m_db != nullptr) return;

	// 数据库路径与ini配置文件同目录
	if (m_config_path.empty()) return;
	size_t pos = m_config_path.find_last_of(L"\\/");
	if (pos == std::wstring::npos) return;
	m_db_path = m_config_path.substr(0, pos + 1) + L"stock_trades.db";

	int rc = sqlite3_open16(m_db_path.c_str(), &m_db);
	if (rc != SQLITE_OK)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
		return;
	}

	// 创建交易记录表
	const char* sql = "CREATE TABLE IF NOT EXISTS trades ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"stock_code TEXT NOT NULL,"
		"stock_name TEXT NOT NULL,"
		"trade_type INTEGER NOT NULL,"
		"trade_time DATETIME NOT NULL,"
		"price REAL NOT NULL,"
		"amount REAL NOT NULL,"
		"total_amount REAL NOT NULL,"
		"fee REAL NOT NULL,"
		"total REAL NOT NULL"
		");";
	char* errMsg = nullptr;
	rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}

	const char* innerOuterSql = "CREATE TABLE IF NOT EXISTS inner_outer_snapshots ("
		"stock_code TEXT NOT NULL,"
		"snapshot_time INTEGER NOT NULL,"
		"inner_volume INTEGER NOT NULL,"
		"outer_volume INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, snapshot_time)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, innerOuterSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}
}

bool CDataManager::SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName, int tradeType, const std::wstring& time, double price, double amount, double totalAmount, double fee, double total)
{
	if (m_db == nullptr) return false;

	const char* sql = "INSERT INTO trades(stock_code, stock_name, trade_type, trade_time, price, amount, total_amount, fee, total) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text16(stmt, 2, stockName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, tradeType);
	sqlite3_bind_text16(stmt, 4, time.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_double(stmt, 5, price);
	sqlite3_bind_double(stmt, 6, amount);
	sqlite3_bind_double(stmt, 7, totalAmount);
	sqlite3_bind_double(stmt, 8, fee);
	sqlite3_bind_double(stmt, 9, total);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool CDataManager::SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp, STOCK::Volume innerVolume, STOCK::Volume outerVolume)
{
	if (m_db == nullptr) return false;

	const char* sql = "INSERT OR REPLACE INTO inner_outer_snapshots(stock_code, snapshot_time, inner_volume, outer_volume) VALUES(?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(timestamp));
	sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(innerVolume));
	sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(outerVolume));

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

void CDataManager::LoadTodayInnerOuterSnapshots()
{
	if (m_db == nullptr) return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	std::tm todayTm = {};
	todayTm.tm_year = st.wYear - 1900;
	todayTm.tm_mon = st.wMonth - 1;
	todayTm.tm_mday = st.wDay;
	time_t dayStart = std::mktime(&todayTm);
	if (dayStart <= 0) return;
	time_t dayEnd = dayStart + 24 * 60 * 60;

	const char* sql = "SELECT snapshot_time, inner_volume, outer_volume FROM inner_outer_snapshots "
		"WHERE stock_code = ? AND snapshot_time >= ? AND snapshot_time < ? ORDER BY snapshot_time ASC;";

	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		stockData->ClearVolumePools();

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK) continue;

		sqlite3_bind_text16(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(dayStart));
		sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(dayEnd));

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			time_t timestamp = static_cast<time_t>(sqlite3_column_int64(stmt, 0));
			STOCK::Volume innerVolume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 1));
			STOCK::Volume outerVolume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
			int tradingMinute = GetTradingMinute(timestamp);
			if (tradingMinute >= 0)
				stockData->AddVolumeSample(timestamp, innerVolume, outerVolume);
			stockData->info.innerVolume = innerVolume;
			stockData->info.outerVolume = outerVolume;
		}

		sqlite3_finalize(stmt);
	}
}

void CDataManager::SaveConfig()
{
	if (!m_config_path.empty())
	{
		utilities::CIniHelper ini(m_config_path);
		ini.WriteStringList(L"config", L"stock_code", m_setting_data.m_stock_codes);
		ini.WriteBool(L"config", L"full_day", m_setting_data.m_full_day);
		ini.WriteBool(L"config", L"show_stock_name", m_setting_data.m_show_stock_name);
		ini.WriteBool(L"config", L"show_fluctuation", m_setting_data.m_show_fluctuation);
		ini.WriteBool(L"config", L"color_with_price", m_setting_data.m_color_with_price);
		ini.WriteInt(L"config", L"kline_width", m_setting_data.m_kline_width);
		ini.WriteInt(L"config", L"kline_height", m_setting_data.m_kline_height);

		// 保存每个股票的关注价格到 CIniHelper 缓冲区
		for (const auto& alert : m_stock_alert_prices)
		{
			const std::wstring& code = alert.first;
			double low = alert.second.first;
			double high = alert.second.second;
			if (low > 0)
			{
				ini.WriteString(code.c_str(), L"alert_low", std::to_wstring(low));
			}
			else
			{
				ini.WriteString(code.c_str(), L"alert_low", L"");
			}
			if (high > 0)
			{
				ini.WriteString(code.c_str(), L"alert_high", std::to_wstring(high));
			}
			else
			{
				ini.WriteString(code.c_str(), L"alert_high", L"");
			}
		}

		// 保存每个股票的持仓配置到 CIniHelper 缓冲区
		for (const auto& position : m_stock_positions)
		{
			const std::wstring& code = position.first;
			double cost = std::get<0>(position.second);
			double count = std::get<1>(position.second);
			const std::wstring& buy_date = std::get<2>(position.second);
			if (cost > 0)
			{
				ini.WriteString(code.c_str(), L"cost_price", std::to_wstring(cost));
			}
			else
			{
				ini.WriteString(code.c_str(), L"cost_price", L"");
			}
			if (count > 0)
			{
				ini.WriteString(code.c_str(), L"holding_count", std::to_wstring(count));
			}
			else
			{
				ini.WriteString(code.c_str(), L"holding_count", L"");
			}
			if (!buy_date.empty())
			{
				ini.WriteString(code.c_str(), L"buy_date", buy_date);
			}
			else
			{
				ini.WriteString(code.c_str(), L"buy_date", L"");
			}
		}

		ini.Save();
	}
}

const CString& CDataManager::StringRes(UINT id)
{
	auto iter = m_string_table.find(id);
	if (iter != m_string_table.end())
	{
		return iter->second;
	}
	else
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		m_string_table[id].LoadString(id);
		return m_string_table[id];
	}
}

void CDataManager::DPIFromWindow(CWnd* pWnd)
{
	CWindowDC dc(pWnd);
	HDC hDC = dc.GetSafeHdc();
	m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

int CDataManager::DPI(int pixel)
{
	return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel)
{
	return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel)
{
	return pixel * 96 / m_dpi;
}

HICON CDataManager::GetIcon(UINT id)
{
	auto iter = m_icons.find(id);
	if (iter != m_icons.end())
	{
		return iter->second;
	}
	else
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
		m_icons[id] = hIcon;
		return hIcon;
	}
}

// std::wstring StockInfo::ToString(bool include_name) const
// {
//     std::wstringstream wss;
//     if (include_name)
//         wss << name << ": ";
//     wss << p << ' ' << pc;
//     return wss.str();
// }

// bool StockInfo::IsEmpty() const
// {
//     return (pc.empty() || pc == L"--%")
//         && (p.empty() || p == L"--")
//         && name.empty();
// }

std::shared_ptr<StockData> CDataManager::GetStockData(const std::wstring& code)
{
	return stockMarket.getStock(code);
}

double CDataManager::GetAlertLowPrice(const std::wstring& code)
{
	auto it = m_stock_alert_prices.find(code);
	Log1("GetAlertLowPrice: code=%s, found=%d\n", code.c_str(), it != m_stock_alert_prices.end());
	if (it != m_stock_alert_prices.end())
	{
		return it->second.first;
	}
	return 0.0;
}

double CDataManager::GetAlertHighPrice(const std::wstring& code)
{
	auto it = m_stock_alert_prices.find(code);
	Log1("GetAlertHighPrice: code=%s, found=%d\n", code.c_str(), it != m_stock_alert_prices.end());
	if (it != m_stock_alert_prices.end())
	{
		return it->second.second;
	}
	return 0.0;
}

void CDataManager::SetAlertPrice(const std::wstring& code, double low, double high)
{
	if (low > 0 || high > 0)
	{
		m_stock_alert_prices[code] = std::make_pair(low, high);
	}
	else
	{
		auto it = m_stock_alert_prices.find(code);
		if (it != m_stock_alert_prices.end())
		{
			m_stock_alert_prices.erase(it);
		}
	}
}

double CDataManager::GetCostPrice(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<0>(it->second);
	}
	return 0.0;
}

double CDataManager::GetHoldingCount(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<1>(it->second);
	}
	return 0.0;
}

std::wstring CDataManager::GetBuyDate(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<2>(it->second);
	}
	return L"";
}

void CDataManager::SetPosition(const std::wstring& code, double cost, double count, const std::wstring& buy_date)
{
	if (cost > 0 || count > 0 || !buy_date.empty())
	{
		std::wstring existing_date = L"";
		auto it = m_stock_positions.find(code);
		if (it != m_stock_positions.end())
		{
			existing_date = std::get<2>(it->second);
		}
		if (buy_date.empty())
		{
			m_stock_positions[code] = std::make_tuple(cost, count, existing_date);
		}
		else
		{
			m_stock_positions[code] = std::make_tuple(cost, count, buy_date);
		}
	}
	else
	{
		// 保留记录为0值，确保SaveConfig时能写入空字符串覆盖ini中的旧值
		m_stock_positions[code] = std::make_tuple(0.0, 0.0, L"");
	}
}

double CDataManager::CalculateMA(const std::wstring& code, double currentPrice, int N)
{
	auto stockData = GetStockData(code);
	if (!stockData)
		return 0.0;

	auto klineObj = stockData->getKLineData();
	if (!klineObj || klineObj->data.empty())
		return 0.0;

	return klineObj->CalculateMA(N);
}

double CDataManager::CalculateAverageAmplitude(const std::wstring& code, int days)
{
	auto stockData = GetStockData(code);
	if (!stockData)
		return 0.0;

	auto klineObj = stockData->getKLineData();
	if (!klineObj || klineObj->data.empty())
		return 0.0;

	return klineObj->CalculateAverageAmplitude(days);
}

static double generateRandomDouble()
{
	srand(time(nullptr)); // 设置随机种子
	double random = (double)rand() / RAND_MAX;
	std::cout << std::fixed << std::setprecision(16);
	return random;
}

void CDataManager::RequestRealtimeData()
{
	TRACE(L"RequestRealtimeData...\n");
	std::vector<std::wstring> codes = m_setting_data.m_stock_codes;
	std::wstring url{ L"https://hq.sinajs.cn/?" };
	std::vector<std::wstring> params;
	params.push_back(L"_=" + std::to_wstring(generateRandomDouble()));
	params.push_back(L"list=" + CCommon::vectorJoinString(codes, L","));

	url += CCommon::vectorJoinString(params, L"&");
	CString strHeaders = _T("Referer: https://finance.sina.com.cn");

	std::string Stock_data;
	if (CCommon::GetURL(url, Stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
	{
		stockMarket.LoadRealtimeDataByJson(Stock_data);
	}

	RequestInnerOuterData();
}

void CDataManager::RequestInnerOuterData()
{
	TRACE(L"RequestInnerOuterData... 使用腾讯API获取内外盘\n");
	std::vector<std::wstring> codes = m_setting_data.m_stock_codes;
	if (codes.empty()) return;

	std::wstring url{ L"http://qt.gtimg.cn/q=" };
	url += CCommon::vectorJoinString(codes, L",");

	CString strHeaders = _T("Referer: https://finance.qq.com");

	std::string stock_data;
	if (CCommon::GetURL(url, stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
	{
		stockMarket.LoadInnerOuterData(stock_data);
	}
}

void CDataManager::RequestTimelineData(std::wstring stock_id)
{
	try
	{
		TRACE(L"RequestTimelineData...\n");

		std::wstring url{ L"https://cn.finance.sina.com.cn/minline/getMinlineData?" };
		// https://cn.finance.sina.com.cn/minline/getMinlineData?symbol=sz000100&version=7.11.0&dpc=1
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"version=7.11.0");
		params.push_back(L"dpc=1");

		SYSTEMTIME st;
		GetLocalTime(&st);
		wchar_t dateBuf[20];
		swprintf_s(dateBuf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
		params.push_back(L"date=" + std::wstring(dateBuf));

		url += CCommon::vectorJoinString(params, L"&");
		// CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

		// CString strHeaders = L"Referer: https://finance.sina.com.cn/realstock/company/" + m_stock_id + L"/nc.shtml";
		std::wstring strHeaders{ L"Referer: https://finance.sina.com.cn/realstock/company/" };
		strHeaders += stock_id;
		strHeaders += L"/nc.shtml";
		CString headers = strHeaders.c_str();

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, headers, headers.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadTimelineDataByJson(stock_id, &strData);
		}

		// 清理资源
		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadTimelineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestKLineData(std::wstring stock_id, int days /*= 750*/)
{
	try
	{
		TRACE(L"RequestKLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=240");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(days));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadKLineDataByJson(stock_id, &strData);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadKLineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestMin5KLineData(std::wstring stock_id, int datalen /*= 250*/)
{
	try
	{
		TRACE(L"RequestMin5KLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=5");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(datalen));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadMin5KLineDataByJson(stock_id, &strData);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadMin5KLineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestMin30KLineData(std::wstring stock_id, int datalen /*= 250*/)
{
	try
	{
		TRACE(L"RequestMin30KLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=30");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(datalen));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadMin30KLineDataByJson(stock_id, &strData);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadMin30KLineDataByJson(stock_id, NULL);
	}
}