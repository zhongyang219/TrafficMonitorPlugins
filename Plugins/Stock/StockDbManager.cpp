#include "pch.h"
#include "StockDbManager.h"
#include <sstream>
#include "sqlite3.h"
#include "utilities/yyjson/yyjson.h"

// ===== 文件内静态辅助函数 =====

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

// yyjson 值转 double（用于筹码分布 points_json 解析）
static double DbGetJsonDoubleValue(yyjson_val* val)
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

// ===== CStockDbManager 实现 =====

CStockDbManager::CStockDbManager()
{
}

CStockDbManager::~CStockDbManager()
{
	Close();
}

bool CStockDbManager::Init(const std::wstring& config_path)
{
	if (m_db != nullptr) return true;

	if (config_path.empty()) return false;
	size_t pos = config_path.find_last_of(L"\\/");
	if (pos == std::wstring::npos) return false;
	m_db_path = config_path.substr(0, pos + 1) + L"stock_trades.db";

	int rc = sqlite3_open16(m_db_path.c_str(), &m_db);
	if (rc != SQLITE_OK)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
		return false;
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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	const char* innerOuterSql = "CREATE TABLE IF NOT EXISTS inner_outer_snapshots ("
		"stock_code TEXT NOT NULL,"
		"snapshot_time INTEGER NOT NULL,"
		"inner_volume INTEGER NOT NULL,"
		"outer_volume INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, snapshot_time)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, innerOuterSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	const char* stockBasicSql = "CREATE TABLE IF NOT EXISTS stock_basic_data ("
		"stock_code TEXT PRIMARY KEY,"
		"circulating_a_shares INTEGER NOT NULL,"
		"updated_at INTEGER NOT NULL"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, stockBasicSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

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
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	// 关联股票最高均幅表
	const char* maxAvgDiffSql = "CREATE TABLE IF NOT EXISTS max_avg_diff ("
		"stock_code TEXT NOT NULL,"
		"max_avg_diff REAL NOT NULL,"
		"update_time TEXT NOT NULL,"
		"PRIMARY KEY (stock_code)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, maxAvgDiffSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	return true;
}

void CStockDbManager::Close()
{
	if (m_db != nullptr)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
	}
	m_db_path.clear();
}

void CStockDbManager::CleanExpiredData()
{
	if (m_db == nullptr) return;

	time_t cutoffTime = GetLocalMidnightTime(-30);
	std::string cutoffDate = GetCacheCutoffDateString();

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

bool CStockDbManager::SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName, int tradeType, const std::wstring& time, double price, double amount, double totalAmount, double fee, double total)
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

bool CStockDbManager::SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp, STOCK::Volume innerVolume, STOCK::Volume outerVolume)
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

std::vector<std::tuple<time_t, STOCK::Volume, STOCK::Volume>> CStockDbManager::LoadInnerOuterSnapshots(const std::wstring& stockCode, time_t startTime)
{
	std::vector<std::tuple<time_t, STOCK::Volume, STOCK::Volume>> result;
	if (m_db == nullptr) return result;

	const char* sql = "SELECT snapshot_time, inner_volume, outer_volume FROM inner_outer_snapshots "
		"WHERE stock_code = ? AND snapshot_time >= ? ORDER BY snapshot_time ASC;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return result;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(startTime));

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		time_t timestamp = static_cast<time_t>(sqlite3_column_int64(stmt, 0));
		STOCK::Volume innerVolume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 1));
		STOCK::Volume outerVolume = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
		result.emplace_back(timestamp, innerVolume, outerVolume);
	}
	sqlite3_finalize(stmt);
	return result;
}

bool CStockDbManager::SaveTimelineCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data)
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

bool CStockDbManager::HasTimelineCache(const std::wstring& stockCode)
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

std::vector<STOCK::TimelinePoint> CStockDbManager::LoadTimelineCache(const std::wstring& stockCode, const std::string& tradeDate)
{
	std::vector<STOCK::TimelinePoint> points;
	if (m_db == nullptr) return points;

	const char* sql = "SELECT time, volume, price, average_price, amount FROM timeline_cache WHERE stock_code = ? AND trade_date = ? ORDER BY time ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return points;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);

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
	return points;
}

std::vector<STOCK::TimelinePoint> CStockDbManager::LoadLatestTimelineCache(const std::wstring& stockCode)
{
	std::string tradeDate = GetTodayDateString();
	auto points = LoadTimelineCache(stockCode, tradeDate);
	if (!points.empty()) return points;

	// 今天没有缓存时，回退加载最近交易日的分时数据
	if (m_db == nullptr) return points;

	const char* fallbackSql = "SELECT time, volume, price, average_price, amount FROM timeline_cache WHERE stock_code = ? AND trade_date = (SELECT trade_date FROM timeline_cache WHERE stock_code = ? ORDER BY trade_date DESC LIMIT 1) ORDER BY time ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, fallbackSql, -1, &stmt, nullptr) != SQLITE_OK) return points;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text16(stmt, 2, stockCode.c_str(), -1, SQLITE_TRANSIENT);

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
	return points;
}

bool CStockDbManager::SaveKLineCache(const std::wstring& stockCode, STOCK::Period period, const std::vector<STOCK::KLinePoint>& data)
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

bool CStockDbManager::HasKLineCache(const std::wstring& stockCode, STOCK::Period period)
{
	if (m_db == nullptr) return false;
	std::wstring table = GetKLineCacheTableW(period);
	if (table.empty()) return false;

	// 日K线：只要有缓存即可（历史数据不过期）
	if (period == STOCK::Period::DAY)
	{
		std::wstring sql = L"SELECT 1 FROM " + table + L" WHERE stock_code = ? LIMIT 1;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		bool hasCache = sqlite3_step(stmt) == SQLITE_ROW;
		sqlite3_finalize(stmt);
		return hasCache;
	}

	// 5分钟/30分钟K线：缓存中最新数据必须包含今天才算有效
	std::wstring sql = L"SELECT day FROM " + table + L" WHERE stock_code = ? ORDER BY day DESC LIMIT 1;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	bool hasCache = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char* dayText = sqlite3_column_text(stmt, 0);
		if (dayText)
		{
			// day格式为 "YYYY-MM-DD HH:MM" 或 "YYYY-MM-DD"，取前10字符比较日期
			std::string dayStr = reinterpret_cast<const char*>(dayText);
			std::string todayStr = GetTodayDateString();
			hasCache = (dayStr.length() >= 10 && dayStr.substr(0, 10) == todayStr);
		}
	}
	sqlite3_finalize(stmt);
	return hasCache;
}

std::vector<STOCK::KLinePoint> CStockDbManager::LoadKLineCache(const std::wstring& stockCode, STOCK::Period period)
{
	std::vector<STOCK::KLinePoint> points;
	if (m_db == nullptr) return points;
	std::wstring table = GetKLineCacheTableW(period);
	if (table.empty()) return points;
	std::wstring sql = L"SELECT day, open, high, low, close, volume FROM " + table + L" WHERE stock_code = ? ORDER BY day ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return points;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);

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
	return points;
}

bool CStockDbManager::SaveStockBasicData(const std::wstring& stockCode, STOCK::Volume circulatingAShares)
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

bool CStockDbManager::LoadStockBasicData(const std::wstring& stockCode, STOCK::Volume& outCirculatingAShares)
{
	outCirculatingAShares = 0;
	if (m_db == nullptr) return false;

	const char* sql = "SELECT circulating_a_shares FROM stock_basic_data WHERE stock_code = ?;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::Volume value = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 0));
		if (value > 0)
		{
			outCirculatingAShares = value;
			found = true;
		}
	}
	sqlite3_finalize(stmt);
	return found;
}

bool CStockDbManager::SaveChipDistribution(const std::wstring& stockCode, const STOCK::ChipDistribution& chipData)
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

bool CStockDbManager::LoadLatestChipDistribution(const std::wstring& stockCode, STOCK::ChipDistribution& chipData)
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
							point.price = DbGetJsonDoubleValue(priceVal);
							point.percent = DbGetJsonDoubleValue(percentVal);
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

bool CStockDbManager::SaveMaxAvgDiff(const std::wstring& stockCode, double maxAvgDiff)
{
	if (m_db == nullptr) return false;

	std::string code(stockCode.begin(), stockCode.end());
	std::string updateTime = GetTodayDateString();

	const char* sql = "INSERT OR REPLACE INTO max_avg_diff (stock_code, max_avg_diff, update_time) VALUES (?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 2, maxAvgDiff);
		sqlite3_bind_text(stmt, 3, updateTime.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	return true;
}

double CStockDbManager::LoadMaxAvgDiff(const std::wstring& stockCode)
{
	if (m_db == nullptr) return 0.0;

	std::string code(stockCode.begin(), stockCode.end());
	std::string today = GetTodayDateString();

	// 只加载今天的记录，非今天的视为过期，每日开盘后重新记录
	const char* sql = "SELECT max_avg_diff FROM max_avg_diff WHERE stock_code = ? AND update_time = ?;";
	sqlite3_stmt* stmt = nullptr;
	double result = 0.0;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, today.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			result = sqlite3_column_double(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	return result;
}

void CStockDbManager::CleanExpiredMaxAvgDiff()
{
	if (m_db == nullptr) return;

	std::string cutoffDate = GetCacheCutoffDateString();

	const char* sql = "DELETE FROM max_avg_diff WHERE update_time < ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
}
