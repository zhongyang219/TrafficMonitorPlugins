#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include "Stock.h"
#include <vector>
#include <sstream>
#include "../utilities/IniHelper.h"
#include <iomanip>
#include <afxinet.h>
#include "sqlite3.h"
#include "utilities/yyjson/yyjson.h"
#include "utilities/JsonHelper.h"
#include <algorithm>
#include <cmath>

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

static double GetJsonDoubleValue(yyjson_val* val);
static const char* GetKLineCacheTable(STOCK::Period period);
static std::wstring GetKLineCacheTableW(STOCK::Period period);

static time_t GetLocalMidnightTime(int offsetDays = 0)
{
	time_t now = time(nullptr);
	tm localTm = {};
	localtime_s(&localTm, &now);
	localTm.tm_hour = 0;
	localTm.tm_min = 0;
	localTm.tm_sec = 0;
	return std::mktime(&localTm) + static_cast<time_t>(offsetDays) * 24 * 60 * 60;
}

static std::string GetLocalDateString(time_t t)
{
	tm localTm = {};
	localtime_s(&localTm, &t);
	char buf[16];
	sprintf_s(buf, "%04d-%02d-%02d", localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
	return buf;
}

static std::string GetTodayDateString()
{
	return GetLocalDateString(time(nullptr));
}

static std::string GetCacheCutoffDateString()
{
	return GetLocalDateString(GetLocalMidnightTime(-30));
}

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
	LoadChipDistributions();
	LoadStockBasicData();
	LoadTimelineCache();
	LoadKLineCache(STOCK::Period::DAY);
	LoadKLineCache(STOCK::Period::MIN5);
	LoadKLineCache(STOCK::Period::MIN30);

	// 清理数据库中超过1个月的快照、K线和筹码峰缓存
	if (m_db != nullptr)
	{
		time_t cutoffTime = GetLocalMidnightTime(-30);
		const char* cleanSql = "DELETE FROM inner_outer_snapshots WHERE snapshot_time < ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, cleanSql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTime));
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		const char* cleanChipSql = "DELETE FROM chip_distributions WHERE updated_at < ?;";
		stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, cleanChipSql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTime));
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		const char* cleanTimelineSql = "DELETE FROM timeline_cache WHERE trade_date < ?;";
		stmt = nullptr;
		std::string cutoffDate = GetCacheCutoffDateString();
		if (sqlite3_prepare_v2(m_db, cleanTimelineSql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, cutoffDate.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		const char* cleanKlineSql = "DELETE FROM kline_day_cache WHERE day < ?;";
		stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, cleanKlineSql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, cutoffDate.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		const char* cleanMin5Sql = "DELETE FROM kline_min5_cache WHERE day < ?;";
		stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, cleanMin5Sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, cutoffDate.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		const char* cleanMin30Sql = "DELETE FROM kline_min30_cache WHERE day < ?;";
		stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, cleanMin30Sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, cutoffDate.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
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
	const char* chipSql = "CREATE TABLE IF NOT EXISTS chip_distributions ("
		"stock_code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"
		"avg_cost REAL NOT NULL,"
		"benefit_ratio REAL NOT NULL,"
		"cost70_low REAL NOT NULL,"
		"cost70_high REAL NOT NULL,"
		"cost70_concentration REAL NOT NULL,"
		"cost90_low REAL NOT NULL,"
		"cost90_high REAL NOT NULL,"
		"cost90_concentration REAL NOT NULL,"
		"points_json TEXT NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, trade_date)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, chipSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}
	const char* stockBasicSql = "CREATE TABLE IF NOT EXISTS stock_basic_data ("
		"stock_code TEXT PRIMARY KEY,"
		"circulating_a_shares INTEGER NOT NULL,"
		"updated_at INTEGER NOT NULL"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, stockBasicSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}

	const char* timelineSql = "CREATE TABLE IF NOT EXISTS timeline_cache ("
		"stock_code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"
		"time TEXT NOT NULL,"
		"volume INTEGER NOT NULL,"
		"price REAL NOT NULL,"
		"average_price REAL NOT NULL,"
		"amount REAL NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, trade_date, time)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, timelineSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}

	const char* klineDaySql = "CREATE TABLE IF NOT EXISTS kline_day_cache ("
		"stock_code TEXT NOT NULL,"
		"day TEXT NOT NULL,"
		"open REAL NOT NULL,"
		"high REAL NOT NULL,"
		"low REAL NOT NULL,"
		"close REAL NOT NULL,"
		"volume INTEGER NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, day)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, klineDaySql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}

	const char* klineMin5Sql = "CREATE TABLE IF NOT EXISTS kline_min5_cache ("
		"stock_code TEXT NOT NULL,"
		"day TEXT NOT NULL,"
		"open REAL NOT NULL,"
		"high REAL NOT NULL,"
		"low REAL NOT NULL,"
		"close REAL NOT NULL,"
		"volume INTEGER NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, day)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, klineMin5Sql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		sqlite3_free(errMsg);
	}

	const char* klineMin30Sql = "CREATE TABLE IF NOT EXISTS kline_min30_cache ("
		"stock_code TEXT NOT NULL,"
		"day TEXT NOT NULL,"
		"open REAL NOT NULL,"
		"high REAL NOT NULL,"
		"low REAL NOT NULL,"
		"close REAL NOT NULL,"
		"volume INTEGER NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, day)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, klineMin30Sql, nullptr, nullptr, &errMsg);
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

bool CDataManager::SaveTimelineCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data)
{
	if (m_db == nullptr || data.empty()) return false;

	const char* sql = "INSERT OR IGNORE INTO timeline_cache(stock_code, trade_date, time, volume, price, average_price, amount, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	std::string tradeDate = GetTodayDateString();
	time_t now = time(nullptr);
	bool ok = true;
	for (const auto& item : data)
	{
		if (item.time.empty()) continue;
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, item.time.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(item.volume));
		sqlite3_bind_double(stmt, 5, item.price);
		sqlite3_bind_double(stmt, 6, item.averagePrice);
		sqlite3_bind_double(stmt, 7, item.amount);
		sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(now));
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			ok = false;
	}
	sqlite3_finalize(stmt);
	return ok;
}

bool CDataManager::SaveKLineCache(const std::wstring& stockCode, STOCK::Period period, const std::vector<STOCK::KLinePoint>& data)
{
	if (m_db == nullptr || data.empty()) return false;
	const char* table = GetKLineCacheTable(period);
	if (table[0] == '\0') return false;

	std::string sql = std::string("INSERT OR IGNORE INTO ") + table + "(stock_code, day, open, high, low, close, volume, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	time_t now = time(nullptr);
	bool ok = true;
	for (const auto& item : data)
	{
		if (item.day.empty()) continue;
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, item.day.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 3, item.open);
		sqlite3_bind_double(stmt, 4, item.high);
		sqlite3_bind_double(stmt, 5, item.low);
		sqlite3_bind_double(stmt, 6, item.close);
		sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(item.volume));
		sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(now));
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			ok = false;
	}
	sqlite3_finalize(stmt);
	return ok;
}

bool CDataManager::HasTimelineCache(const std::wstring& stockCode)
{
	if (m_db == nullptr) return false;
	const char* sql = "SELECT 1 FROM timeline_cache WHERE stock_code = ? AND trade_date = ? LIMIT 1;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	std::string tradeDate = GetTodayDateString();
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	bool hasCache = sqlite3_step(stmt) == SQLITE_ROW;
	sqlite3_finalize(stmt);
	return hasCache;
}

bool CDataManager::HasKLineCache(const std::wstring& stockCode, STOCK::Period period)
{
	if (m_db == nullptr) return false;
	std::wstring table = GetKLineCacheTableW(period);
	if (table.empty()) return false;
	std::wstring sql = L"SELECT 1 FROM " + table + L" WHERE stock_code = ? LIMIT 1;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	bool hasCache = sqlite3_step(stmt) == SQLITE_ROW;
	sqlite3_finalize(stmt);
	return hasCache;
}

void CDataManager::LoadTimelineCache()
{
	if (m_db == nullptr) return;
	const char* sql = "SELECT time, volume, price, average_price, amount FROM timeline_cache WHERE stock_code = ? AND trade_date = ? ORDER BY time ASC;";
	std::string tradeDate = GetTodayDateString();
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) continue;
		sqlite3_bind_text16(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
		std::vector<STOCK::TimelinePoint> points;
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			STOCK::TimelinePoint point;
			const unsigned char* timeText = sqlite3_column_text(stmt, 0);
			point.time = timeText ? reinterpret_cast<const char*>(timeText) : "";
			point.volume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 1));
			point.price = sqlite3_column_double(stmt, 2);
			point.averagePrice = sqlite3_column_double(stmt, 3);
			point.amount = sqlite3_column_double(stmt, 4);
			points.push_back(point);
		}
		sqlite3_finalize(stmt);
		if (!points.empty())
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			stockData->clearTimelinePoint();
			for (const auto& point : points)
				stockData->addTimelinePoint(point);
		}
	}
}

void CDataManager::LoadKLineCache(STOCK::Period period)
{
	if (m_db == nullptr) return;
	std::wstring table = GetKLineCacheTableW(period);
	if (table.empty()) return;
	std::wstring sql = L"SELECT day, open, high, low, close, volume FROM " + table + L" WHERE stock_code = ? ORDER BY day ASC;";
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) continue;
		sqlite3_bind_text16(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		std::vector<STOCK::KLinePoint> points;
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			STOCK::KLinePoint point;
			const unsigned char* dayText = sqlite3_column_text(stmt, 0);
			point.day = dayText ? reinterpret_cast<const char*>(dayText) : "";
			point.open = sqlite3_column_double(stmt, 1);
			point.high = sqlite3_column_double(stmt, 2);
			point.low = sqlite3_column_double(stmt, 3);
			point.close = sqlite3_column_double(stmt, 4);
			point.volume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 5));
			points.push_back(point);
		}
		sqlite3_finalize(stmt);
		if (!points.empty())
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			if (period == STOCK::Period::DAY)
			{
				stockData->clearKLineData();
				for (const auto& point : points)
					stockData->addKLinePoint(point);
			}
			else if (period == STOCK::Period::MIN5)
			{
				stockData->clearMin5KLineData();
				for (const auto& point : points)
					stockData->addMin5KLinePoint(point);
			}
			else if (period == STOCK::Period::MIN30)
			{
				stockData->clearMin30KLineData();
				for (const auto& point : points)
					stockData->addMin30KLinePoint(point);
			}
		}
	}
}

bool CDataManager::SaveStockBasicData(const std::wstring& stockCode, STOCK::Volume circulatingAShares)
{
	if (m_db == nullptr || circulatingAShares <= 0) return false;

	const char* sql = "INSERT OR REPLACE INTO stock_basic_data(stock_code, circulating_a_shares, updated_at) VALUES(?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(circulatingAShares));
	sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(time(nullptr)));

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

void CDataManager::LoadStockBasicData()
{
	if (m_db == nullptr) return;

	const char* sql = "SELECT circulating_a_shares FROM stock_basic_data WHERE stock_code = ?;";
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK) continue;

		sqlite3_bind_text16(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			STOCK::Volume circulatingAShares = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 0));
			if (circulatingAShares > 0)
			{
				auto stockData = GetStockData(code);
				if (stockData)
				{
					std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
					stockData->info.circulatingAShares = circulatingAShares;
				}
			}
		}
		sqlite3_finalize(stmt);
	}
}

bool CDataManager::SaveChipDistribution(const std::wstring& stockCode, const STOCK::ChipDistribution& chipData)
{
	if (m_db == nullptr || !chipData.IsValid()) return false;

	std::ostringstream pointsStream;
	pointsStream << "[";
	for (size_t i = 0; i < chipData.points.size(); ++i)
	{
		if (i > 0) pointsStream << ",";
		pointsStream << "[" << chipData.points[i].price << "," << chipData.points[i].percent << "]";
	}
	pointsStream << "]";
	std::string pointsJson = pointsStream.str();
	time_t now = time(nullptr);

	const char* sql = "INSERT OR REPLACE INTO chip_distributions(stock_code, trade_date, avg_cost, benefit_ratio, cost70_low, cost70_high, cost70_concentration, cost90_low, cost90_high, cost90_concentration, points_json, updated_at) "
		"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, chipData.tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_double(stmt, 3, chipData.avgCost);
	sqlite3_bind_double(stmt, 4, chipData.benefitRatio);
	sqlite3_bind_double(stmt, 5, chipData.cost70Low);
	sqlite3_bind_double(stmt, 6, chipData.cost70High);
	sqlite3_bind_double(stmt, 7, chipData.cost70Concentration);
	sqlite3_bind_double(stmt, 8, chipData.cost90Low);
	sqlite3_bind_double(stmt, 9, chipData.cost90High);
	sqlite3_bind_double(stmt, 10, chipData.cost90Concentration);
	sqlite3_bind_text(stmt, 11, pointsJson.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 12, static_cast<sqlite3_int64>(now));

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool CDataManager::LoadLatestChipDistribution(const std::wstring& stockCode, STOCK::ChipDistribution& chipData)
{
	chipData.Clear();
	if (m_db == nullptr) return false;

	const char* sql = "SELECT trade_date, avg_cost, benefit_ratio, cost70_low, cost70_high, cost70_concentration, cost90_low, cost90_high, cost90_concentration, points_json, updated_at "
		"FROM chip_distributions WHERE stock_code = ? ORDER BY updated_at DESC LIMIT 1;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char* tradeDate = sqlite3_column_text(stmt, 0);
		chipData.tradeDate = tradeDate ? reinterpret_cast<const char*>(tradeDate) : "";
		chipData.avgCost = sqlite3_column_double(stmt, 1);
		chipData.benefitRatio = sqlite3_column_double(stmt, 2);
		chipData.cost70Low = sqlite3_column_double(stmt, 3);
		chipData.cost70High = sqlite3_column_double(stmt, 4);
		chipData.cost70Concentration = sqlite3_column_double(stmt, 5);
		chipData.cost90Low = sqlite3_column_double(stmt, 6);
		chipData.cost90High = sqlite3_column_double(stmt, 7);
		chipData.cost90Concentration = sqlite3_column_double(stmt, 8);
		const unsigned char* pointsJson = sqlite3_column_text(stmt, 9);
		chipData.updatedAt = static_cast<time_t>(sqlite3_column_int64(stmt, 10));
		if (pointsJson != nullptr)
		{
			std::string pointsStr = reinterpret_cast<const char*>(pointsJson);
			yyjson_doc* doc = yyjson_read(pointsStr.c_str(), pointsStr.size(), 0);
			if (doc != nullptr)
			{
				yyjson_val* root = yyjson_doc_get_root(doc);
				if (root != nullptr && yyjson_is_arr(root))
				{
					yyjson_val* item;
					yyjson_arr_iter iter;
					yyjson_arr_iter_init(root, &iter);
					while ((item = yyjson_arr_iter_next(&iter)))
					{
						if (item != nullptr && yyjson_is_arr(item) && yyjson_arr_size(item) >= 2)
						{
							yyjson_val* priceVal = yyjson_arr_get(item, 0);
							yyjson_val* percentVal = yyjson_arr_get(item, 1);
							STOCK::ChipPoint point;
							point.price = GetJsonDoubleValue(priceVal);
							point.percent = GetJsonDoubleValue(percentVal);
							chipData.points.push_back(point);
						}
					}
				}
				yyjson_doc_free(doc);
			}
		}
	}
	sqlite3_finalize(stmt);
	return chipData.IsValid();
}

void CDataManager::LoadChipDistributions()
{
	if (m_db == nullptr) return;

	for (const auto& code : m_setting_data.m_stock_codes)
	{
		STOCK::ChipDistribution chipData;
		if (LoadLatestChipDistribution(code, chipData))
		{
			auto stockData = GetStockData(code);
			if (stockData)
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				stockData->chipDistribution = chipData;
			}
		}
	}
}

void CDataManager::LoadTodayInnerOuterSnapshots()
{
	if (m_db == nullptr) return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	std::tm nowTm = {};
	nowTm.tm_year = st.wYear - 1900;
	nowTm.tm_mon = st.wMonth - 1;
	nowTm.tm_mday = st.wDay;
	time_t nowTime = std::mktime(&nowTm);
	if (nowTime <= 0) return;
	// 加载最近2天的数据，覆盖跨0点场景
	time_t startTime = nowTime - 24 * 60 * 60;

	const char* sql = "SELECT snapshot_time, inner_volume, outer_volume FROM inner_outer_snapshots "
		"WHERE stock_code = ? AND snapshot_time >= ? ORDER BY snapshot_time ASC;";

	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		stockData->ClearVolumePools();

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK) continue;

		sqlite3_bind_text16(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(startTime));

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

const STOCK::ChipDistribution* CDataManager::GetChipDistribution(const std::wstring& code)
{
	auto stockData = GetStockData(code);
	if (!stockData || !stockData->chipDistribution.IsValid())
		return nullptr;
	return &stockData->chipDistribution;
}

STOCK::Volume CDataManager::GetCirculatingAShares(const std::wstring& code)
{
	auto stockData = GetStockData(code);
	return stockData ? stockData->info.circulatingAShares : 0;
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

static double GetJsonDoubleValue(yyjson_val* val)
{
	if (val == nullptr) return 0.0;
	if (yyjson_is_real(val)) return yyjson_get_real(val);
	if (yyjson_is_sint(val)) return static_cast<double>(yyjson_get_sint(val));
	if (yyjson_is_uint(val)) return static_cast<double>(yyjson_get_uint(val));
	if (yyjson_is_str(val))
	{
		try
		{
			return std::stod(yyjson_get_str(val));
		}
		catch (...)
		{
			return 0.0;
		}
	}
	return 0.0;
}

static bool TryParseDouble(const std::string& value, double& result)
{
	try
	{
		result = std::stod(value);
		return true;
	}
	catch (...)
	{
		result = 0.0;
		return false;
	}
}

static bool IsSameLocalDate(time_t lhs, time_t rhs)
{
	if (lhs <= 0 || rhs <= 0) return false;
	tm lhsTm = {};
	tm rhsTm = {};
	localtime_s(&lhsTm, &lhs);
	localtime_s(&rhsTm, &rhs);
	return lhsTm.tm_year == rhsTm.tm_year && lhsTm.tm_mon == rhsTm.tm_mon && lhsTm.tm_mday == rhsTm.tm_mday;
}

static const char* GetKLineCacheTable(STOCK::Period period)
{
	switch (period)
	{
	case STOCK::Period::DAY:
		return "kline_day_cache";
	case STOCK::Period::MIN5:
		return "kline_min5_cache";
	case STOCK::Period::MIN30:
		return "kline_min30_cache";
	default:
		return "";
	}
}

static std::wstring GetKLineCacheTableW(STOCK::Period period)
{
	switch (period)
	{
	case STOCK::Period::DAY:
		return L"kline_day_cache";
	case STOCK::Period::MIN5:
		return L"kline_min5_cache";
	case STOCK::Period::MIN30:
		return L"kline_min30_cache";
	default:
		return L"";
	}
}

static std::wstring GetEastMoneySecId(const std::wstring& stockId)
{
	std::wstring code = stockId;
	if (code.rfind(kSH, 0) == 0 || code.rfind(kSZ, 0) == 0 || code.rfind(kBJ, 0) == 0)
		code = code.substr(2);

	if (code.size() != 6)
		return L"";

	if (code[0] == L'6' || code[0] == L'5')
		return L"1." + code;
	if (code[0] == L'0' || code[0] == L'3' || code[0] == L'1')
		return L"0." + code;
	if (code[0] == L'8' || code[0] == L'4')
		return L"0." + code;
	return L"";
}

struct ChipKLinePoint
{
	std::string date;
	double open{ 0.0 };
	double close{ 0.0 };
	double high{ 0.0 };
	double low{ 0.0 };
	double turnoverRate{ 0.0 };
	STOCK::Volume volume{ 0 };
};

static double GetCostByChip(const std::vector<double>& chips, double minPrice, double accuracy, double targetChip)
{
	double sum = 0.0;
	for (size_t i = 0; i < chips.size(); ++i)
	{
		if (sum + chips[i] > targetChip)
			return minPrice + i * accuracy;
		sum += chips[i];
	}
	return minPrice + (chips.empty() ? 0 : chips.size() - 1) * accuracy;
}

static void ComputePercentChips(const std::vector<double>& chips, double minPrice, double accuracy, double totalChips, double percent, double& low, double& high, double& concentration)
{
	double lowPercent = (1.0 - percent) / 2.0;
	double highPercent = (1.0 + percent) / 2.0;
	low = GetCostByChip(chips, minPrice, accuracy, totalChips * lowPercent);
	high = GetCostByChip(chips, minPrice, accuracy, totalChips * highPercent);
	concentration = (low + high == 0.0) ? 0.0 : (high - low) / (low + high);
}

static void FillChipDistributionFromShares(const std::vector<double>& chips, double minPrice, double accuracy, double totalShares, double currentPrice, const std::string& tradeDate, STOCK::ChipDistribution& chipData)
{
	chipData.Clear();
	chipData.tradeDate = tradeDate;
	if (chips.empty() || totalShares <= 0.0) return;

	double benefitChips = 0.0;
	double weightSum = 0.0;
	for (size_t i = 0; i < chips.size(); ++i)
	{
		double price = minPrice + accuracy * i;
		weightSum += price * chips[i];
		if (currentPrice >= price)
			benefitChips += chips[i];
		STOCK::ChipPoint point;
		point.price = round(price * 100.0) / 100.0;
		point.percent = chips[i] / totalShares;
		chipData.points.push_back(point);
	}

	chipData.avgCost = weightSum / totalShares;
	ComputePercentChips(chips, minPrice, accuracy, totalShares, 0.7, chipData.cost70Low, chipData.cost70High, chipData.cost70Concentration);
	ComputePercentChips(chips, minPrice, accuracy, totalShares, 0.9, chipData.cost90Low, chipData.cost90High, chipData.cost90Concentration);
	chipData.benefitRatio = benefitChips / totalShares;
}

static void AddChipByRange(std::vector<double>& chips, double minPrice, double accuracy, double low, double high, double addShares)
{
	if (chips.empty() || addShares <= 0.0) return;
	int lowIndex = static_cast<int>(ceil((low - minPrice) / accuracy));
	int highIndex = static_cast<int>(floor((high - minPrice) / accuracy));
	lowIndex = min(static_cast<int>(chips.size()) - 1, max(0, lowIndex));
	highIndex = min(static_cast<int>(chips.size()) - 1, max(0, highIndex));
	if (highIndex < lowIndex)
		std::swap(highIndex, lowIndex);
	int rangeCnt = highIndex - lowIndex + 1;
	if (rangeCnt <= 0) return;
	double perPriceShare = addShares / rangeCnt;
	for (int i = lowIndex; i <= highIndex; ++i)
		chips[i] += perPriceShare;
}

static void UpdateChipByKLine(std::vector<double>& chips, double minPrice, double accuracy, double totalShares, const ChipKLinePoint& bar)
{
	if (chips.empty() || totalShares <= 0.0 || bar.volume <= 0) return;
	const double CHIP_ATTRITION_N = 1.3;
	const double MAX_EFFECT_TURN = 0.85;
	const double FLOAT_CORRECT_THRESHOLD = 0.01;

	double minuteTurn = static_cast<double>(bar.volume) / totalShares;
	double effTurn = min(MAX_EFFECT_TURN, minuteTurn * CHIP_ATTRITION_N);
	double retainRate = 1.0 - effTurn;
	double addTotalShare = effTurn * totalShares;
	for (auto& val : chips)
		val *= retainRate;
	AddChipByRange(chips, minPrice, accuracy, bar.low, bar.high, addTotalShare);

	double sumAll = 0.0;
	for (double val : chips)
		sumAll += val;
	if (sumAll > 0.0 && fabs(sumAll - totalShares) > FLOAT_CORRECT_THRESHOLD)
	{
		double scale = totalShares / sumAll;
		for (auto& val : chips)
			val *= scale;
	}
}

static bool CalculateEtfChipDistribution(const std::vector<ChipKLinePoint>& klines, STOCK::Volume totalShares, STOCK::ChipDistribution& chipData)
{
	if (klines.empty() || totalShares <= 0) return false;

	const double PRICE_STEP = 0.01;
	double minPrice = 999999.0;
	double maxPrice = 0.0;
	for (const auto& item : klines)
	{
		if (item.low > 0.0) minPrice = min(minPrice, item.low);
		if (item.high > 0.0) maxPrice = max(maxPrice, item.high);
	}
	if (minPrice <= 0.0 || maxPrice < minPrice) return false;

	minPrice = floor(minPrice * 100.0) / 100.0;
	maxPrice = ceil(maxPrice * 100.0) / 100.0;
	int gridCount = static_cast<int>(floor((maxPrice - minPrice) / PRICE_STEP + 0.5)) + 1;
	if (gridCount <= 0) return false;
	std::vector<double> chips(gridCount, 0.0);

	const double CHIP_ATTRITION_N = 1.3;
	const double MAX_EFFECT_TURN = 0.85;
	const auto& first = klines.front();
	double firstTurn = static_cast<double>(first.volume) / static_cast<double>(totalShares);
	double firstEffTurn = min(MAX_EFFECT_TURN, firstTurn * CHIP_ATTRITION_N);
	AddChipByRange(chips, minPrice, PRICE_STEP, first.low, first.high, firstEffTurn * totalShares);

	for (size_t i = 1; i < klines.size(); ++i)
		UpdateChipByKLine(chips, minPrice, PRICE_STEP, static_cast<double>(totalShares), klines[i]);

	FillChipDistributionFromShares(chips, minPrice, PRICE_STEP, static_cast<double>(totalShares), klines.back().close, klines.back().date, chipData);
	return chipData.IsValid();
}

static bool CalculateChipDistribution(const std::vector<ChipKLinePoint>& klines, STOCK::ChipDistribution& chipData)
{
	if (klines.empty()) return false;

	const int factor = 150;
	int index = static_cast<int>(klines.size()) - 1;
	int start = max(0, index - 120 + 1);
	double maxPrice = 0.0;
	double minPrice = 0.0;
	for (int i = start; i <= index; ++i)
	{
		maxPrice = maxPrice == 0.0 ? klines[i].high : max(maxPrice, klines[i].high);
		minPrice = minPrice == 0.0 ? klines[i].low : min(minPrice, klines[i].low);
	}
	if (maxPrice <= 0.0 || minPrice <= 0.0 || maxPrice < minPrice) return false;

	double accuracy = max(0.01, (maxPrice - minPrice) / (factor - 1));
	std::vector<double> chips(factor, 0.0);
	for (int i = start; i <= index; ++i)
	{
		const auto& item = klines[i];
		double high = item.high;
		double low = item.low;
		double avg = (item.open + item.close + item.high + item.low) / 4.0;
		double turnoverRate = min(1.0, item.turnoverRate / 100.0);
		int highIndex = static_cast<int>(floor((high - minPrice) / accuracy));
		int lowIndex = static_cast<int>(ceil((low - minPrice) / accuracy));
		highIndex = min(factor - 1, max(0, highIndex));
		lowIndex = min(factor - 1, max(0, lowIndex));
		int avgIndex = min(factor - 1, max(0, static_cast<int>(floor((avg - minPrice) / accuracy))));
		double gPoint = high == low ? factor - 1.0 : 2.0 / (high - low);

		for (double& chip : chips)
			chip *= (1.0 - turnoverRate);

		if (high == low)
		{
			chips[avgIndex] += gPoint * turnoverRate / 2.0;
		}
		else
		{
			for (int j = lowIndex; j <= highIndex; ++j)
			{
				double curPrice = minPrice + accuracy * j;
				if (curPrice <= avg)
				{
					chips[j] += fabs(avg - low) < 1e-8 ? gPoint * turnoverRate : (curPrice - low) / (avg - low) * gPoint * turnoverRate;
				}
				else
				{
					chips[j] += fabs(high - avg) < 1e-8 ? gPoint * turnoverRate : (high - curPrice) / (high - avg) * gPoint * turnoverRate;
				}
			}
		}
	}

	double totalChips = 0.0;
	for (double chip : chips)
		totalChips += chip;
	if (totalChips <= 0.0) return false;

	chipData.Clear();
	chipData.tradeDate = klines.back().date;
	chipData.avgCost = GetCostByChip(chips, minPrice, accuracy, totalChips * 0.5);
	ComputePercentChips(chips, minPrice, accuracy, totalChips, 0.7, chipData.cost70Low, chipData.cost70High, chipData.cost70Concentration);
	ComputePercentChips(chips, minPrice, accuracy, totalChips, 0.9, chipData.cost90Low, chipData.cost90High, chipData.cost90Concentration);

	double benefitChips = 0.0;
	double currentPrice = klines.back().close;
	for (int i = 0; i < factor; ++i)
	{
		double price = minPrice + accuracy * i;
		if (currentPrice >= price)
			benefitChips += chips[i];
		STOCK::ChipPoint point;
		point.price = round(price * 100.0) / 100.0;
		point.percent = chips[i] / totalChips;
		chipData.points.push_back(point);
	}
	chipData.benefitRatio = benefitChips / totalChips;
	return true;
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

void CDataManager::RequestAllStockBasicData()
{
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		RequestStockBasicData(code);
	}
}

bool CDataManager::RequestStockBasicData(std::wstring stock_id)
{
	std::wstring secId = GetEastMoneySecId(stock_id);
	if (secId.empty()) return false;

	try
	{
		TRACE(L"RequestStockBasicData...\n");
		std::wstring url{ L"https://push2.eastmoney.com/api/qt/stock/get?" };
		std::vector<std::wstring> params;
		params.push_back(L"secid=" + secId);
		params.push_back(L"fields=f43,f44,f45,f46,f47,f48,f49,f50,f51,f57,f58,f60,f85,f116,f117");
		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: https://quote.eastmoney.com");
		std::string response;
		if (!CCommon::GetURL(url, response, true, WEB_USERAGENT, strHeaders, strHeaders.GetLength()) || response.empty())
			return false;

		yyjson_doc* doc = yyjson_read(response.c_str(), response.size(), 0);
		if (doc == nullptr) return false;

		STOCK::Volume circulatingAShares = 0;
		yyjson_val* root = yyjson_doc_get_root(doc);
		yyjson_val* data = root ? yyjson_obj_get(root, "data") : nullptr;
		if (data != nullptr)
		{
			circulatingAShares = static_cast<STOCK::Volume>(GetJsonDoubleValue(yyjson_obj_get(data, "f85")));
		}
		yyjson_doc_free(doc);

		if (circulatingAShares <= 0)
			return false;

		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = GetStockData(stock_id);
			if (!stockData) return false;
			stockData->info.circulatingAShares = circulatingAShares;
		}
		SaveStockBasicData(stock_id, circulatingAShares);
		return true;
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}
	catch (...)
	{
	}
	return false;
}

void CDataManager::RequestAllChipDistributionData()
{
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		RequestChipDistributionData(code);
	}
}

bool CDataManager::RequestChipDistributionData(std::wstring stock_id)
{
	std::wstring secId = GetEastMoneySecId(stock_id);
	if (secId.empty()) return false;

	STOCK::ChipDistribution cachedData;
	if (LoadLatestChipDistribution(stock_id, cachedData) && IsSameLocalDate(cachedData.updatedAt, time(nullptr)))
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(stock_id);
		if (stockData)
		{
			stockData->chipDistribution = cachedData;
			return true;
		}
	}

	try
	{
		TRACE(L"RequestChipDistributionData...\n");
		std::wstring url{ L"https://push2his.eastmoney.com/api/qt/stock/kline/get?" };
		std::vector<std::wstring> params;
		params.push_back(L"secid=" + secId);
		params.push_back(L"fields1=f1,f2,f3,f4,f5,f6");
		params.push_back(L"fields2=f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61");
		params.push_back(L"klt=101");
		params.push_back(L"fqt=0");
		SYSTEMTIME st;
		GetLocalTime(&st);
		wchar_t dateBuf[16];
		swprintf_s(dateBuf, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
		params.push_back(L"end=" + std::wstring(dateBuf));
		params.push_back(L"lmt=750");
		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: https://quote.eastmoney.com");
		std::string response;
		if (!CCommon::GetURL(url, response, true, WEB_USERAGENT, strHeaders, strHeaders.GetLength()) || response.empty())
			return false;

		yyjson_doc* doc = yyjson_read(response.c_str(), response.size(), 0);
		if (doc == nullptr) return false;

		std::vector<ChipKLinePoint> klines;
		yyjson_val* root = yyjson_doc_get_root(doc);
		yyjson_val* data = yyjson_obj_get(root, "data");
		yyjson_val* klineArr = data ? yyjson_obj_get(data, "klines") : nullptr;
		if (klineArr != nullptr && yyjson_is_arr(klineArr))
		{
			yyjson_val* item;
			yyjson_arr_iter iter;
			yyjson_arr_iter_init(klineArr, &iter);
			while ((item = yyjson_arr_iter_next(&iter)))
			{
				const char* line = yyjson_get_str(item);
				if (line == nullptr) continue;
				std::vector<std::string> values = CCommon::split(line, ',');
				if (values.size() < 11) continue;

				double open = 0.0, close = 0.0, high = 0.0, low = 0.0, volumeHands = 0.0, turnoverRate = 0.0;
				if (!TryParseDouble(values[1], open) || !TryParseDouble(values[2], close) || !TryParseDouble(values[3], high) || !TryParseDouble(values[4], low))
					continue;
				TryParseDouble(values[5], volumeHands);
				TryParseDouble(values[10], turnoverRate);
				if (high <= 0.0 || low <= 0.0 || volumeHands <= 0.0)
					continue;

				ChipKLinePoint point;
				point.date = values[0];
				point.open = open;
				point.close = close;
				point.high = high;
				point.low = low;
				point.turnoverRate = turnoverRate;
				point.volume = static_cast<STOCK::Volume>(volumeHands * 100.0);
				klines.push_back(point);
			}
		}
		yyjson_doc_free(doc);

		if (klines.empty())
			return false;

		bool isFund = false;
		STOCK::Volume totalShares = 0;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = GetStockData(stock_id);
			if (stockData)
			{
				isFund = CCommon::IsFundCode(stock_id);
				totalShares = stockData->info.circulatingAShares;
			}
		}

		STOCK::ChipDistribution chipData;
		if (isFund)
		{
			if (totalShares <= 0)
			{
				RequestStockBasicData(stock_id);
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = GetStockData(stock_id);
				if (stockData)
					totalShares = stockData->info.circulatingAShares;
			}
			if (!CalculateEtfChipDistribution(klines, totalShares, chipData))
				return false;
		}
		else if (!CalculateChipDistribution(klines, chipData))
		{
			if (totalShares <= 0)
			{
				RequestStockBasicData(stock_id);
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = GetStockData(stock_id);
				if (stockData)
					totalShares = stockData->info.circulatingAShares;
			}
			if (!CalculateEtfChipDistribution(klines, totalShares, chipData))
				return false;
		}

		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(stock_id);
		if (!stockData) return false;
		stockData->chipDistribution = chipData;
		SaveChipDistribution(stock_id, chipData);
		return true;
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}
	catch (...)
	{
	}
	return false;
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
			auto stockData = GetStockData(stock_id);
			auto timelineData = stockData ? stockData->getTimelineData() : nullptr;
			if (timelineData && !timelineData->data.empty())
				SaveTimelineCache(stock_id, timelineData->data);
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
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getKLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::DAY, klineData->data);
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
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getMin5KLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::MIN5, klineData->data);
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
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getMin30KLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::MIN30, klineData->data);
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