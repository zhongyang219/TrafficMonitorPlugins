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
	return GetLocalDateString(GetLocalMidnightTime(-7));
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

	// 启用 WAL 日志模式，提升读写并发性能（读不阻塞写，写不阻塞读）
	sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

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

	// 盘口分钟级快照表：每分钟记录每档盘口（价格/挂单量）+ 该价格对应的累计主动成交量
	// 10档列直存便于SQL统计：a=b_{1..5}/s_{1..5}；同一时刻每个档位一行，(code, time, is_ask, level_idx) 唯一
	const char* orderBookSnapSql = "CREATE TABLE IF NOT EXISTS order_book_snapshots ("
		"stock_code TEXT NOT NULL,"
		"snapshot_time INTEGER NOT NULL,"      // 快照时间戳(秒)
		"is_ask INTEGER NOT NULL,"             // 0=买盘, 1=卖盘
		"level_idx INTEGER NOT NULL,"          // 档位下标 0~4（0=一档）
		"price REAL NOT NULL,"                 // 档位价格
		"order_volume INTEGER NOT NULL,"       // 该档挂单量
		"cum_active_buy_vol INTEGER NOT NULL,"   // 该价格累计主动买入量（tick_trade聚合，界面同口径）
		"cum_active_sell_vol INTEGER NOT NULL,"  // 该价格累计主动卖出量
		"PRIMARY KEY(stock_code, snapshot_time, is_ask, level_idx)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, orderBookSnapSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	// 预估成交量记录表：双击盘口行录入的"买入时"快照（实时列不入库）
	const char* orderBookEstSql = "CREATE TABLE IF NOT EXISTS order_book_estimates ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"stock_code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"          // "YYYY-MM-DD"
		"create_time INTEGER NOT NULL,"      // 创建时间戳(秒)
		"trade_type INTEGER NOT NULL,"       // 0=买入, 1=卖出
		"buy_price REAL NOT NULL,"           // 买入价格
		"total_amount INTEGER NOT NULL,"     // 总数量
		"order_volume INTEGER NOT NULL,"     // 当前挂单量(买入时)
		"cum_volume INTEGER NOT NULL,"       // 当前累计成交量(买入时)
		"est_volume INTEGER NOT NULL"        // 买入预计成交累积量(=挂单量+累计成交量)
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, orderBookEstSql, nullptr, nullptr, &errMsg);
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

	// 关联股票均幅统计表
	// 按 (stock_code, trade_date) 唯一定位当天记录，保证重启后能可靠加载“当天”的最小/最大值
	// 旧表仅以 stock_code 为主键（无 trade_date 列），跨天会被覆盖，需迁移重建（仅在旧schema时执行）
	auto hasTradeDateColumn = [&]() -> bool {
		std::string sql = "PRAGMA table_info(avg_diff_stats);";
		sqlite3_stmt* stmt = nullptr;
		bool found = false;
		if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				const unsigned char* name = sqlite3_column_text(stmt, 1);
				if (name && strcmp(reinterpret_cast<const char*>(name), "trade_date") == 0)
				{
					found = true;
					break;
				}
			}
		}
		sqlite3_finalize(stmt);
		return found;
		};

	if (!hasTradeDateColumn())
	{
		errMsg = nullptr;
		sqlite3_exec(m_db, "DROP TABLE IF EXISTS avg_diff_stats;", nullptr, nullptr, &errMsg);
		if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }
	}
	errMsg = nullptr;
	const char* avgDiffStatsSql = "CREATE TABLE IF NOT EXISTS avg_diff_stats ("
		"stock_code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"
		"min_avg_diff REAL NOT NULL,"
		"max_avg_diff REAL NOT NULL,"
		"current_avg_diff REAL NOT NULL,"
		"update_time TEXT NOT NULL,"
		"PRIMARY KEY (stock_code, trade_date)"
		");";
	rc = sqlite3_exec(m_db, avgDiffStatsSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	// 基金净值按分钟缓存表
	const char* fundNavSql = "CREATE TABLE IF NOT EXISTS fund_nav_cache ("
		"stock_code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"
		"time TEXT NOT NULL,"
		"nav REAL NOT NULL,"
		"updated_at INTEGER NOT NULL,"
		"PRIMARY KEY(stock_code, trade_date, time)"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, fundNavSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	// 交易明细表，存放一档行情逐笔成交数据
	// trade_date 由业务层填入，避免跨天混淆；time_key 为接口返回的 HH:MM（无秒）
	const char* transactionSql = "CREATE TABLE IF NOT EXISTS tick_trade ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"code TEXT NOT NULL,"
		"trade_date TEXT NOT NULL,"
		"time_key TEXT NOT NULL,"
		"price REAL NOT NULL,"
		"vol INTEGER NOT NULL,"
		"buyorsell INTEGER NOT NULL"
		");";
	errMsg = nullptr;
	rc = sqlite3_exec(m_db, transactionSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) sqlite3_free(errMsg);

	return true;
}

void CStockDbManager::Close()
{
	if (m_db != nullptr)
	{
		// WAL 模式下先主动 checkpoint，把 WAL 日志合并回主库文件
		sqlite3_exec(m_db, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
		sqlite3_close(m_db);
		m_db = nullptr;
	}
	m_db_path.clear();
}

void CStockDbManager::CleanExpiredData()
{
	if (m_db == nullptr) return;

	time_t cutoffTime7d = GetLocalMidnightTime(-7);
	std::string cutoffDate7d = GetLocalDateString(cutoffTime7d);
	std::string cutoffDate3y = GetLocalDateString(GetLocalMidnightTime(-3 * 365));
	std::string cutoffDate1m = GetLocalDateString(GetLocalMidnightTime(-30));

	const char* cleanSql = "DELETE FROM inner_outer_snapshots WHERE snapshot_time < ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTime7d));
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanChipSql = "DELETE FROM chip_distributions WHERE updated_at < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanChipSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTime7d));
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanTimelineSql = "DELETE FROM timeline_cache WHERE trade_date < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanTimelineSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate7d.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanKlineSql = "DELETE FROM kline_day_cache WHERE day < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanKlineSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate3y.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanMin5Sql = "DELETE FROM kline_min5_cache WHERE day < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanMin5Sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate7d.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanMin30Sql = "DELETE FROM kline_min30_cache WHERE day < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanMin30Sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate1m.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* cleanFundNavSql = "DELETE FROM fund_nav_cache WHERE trade_date < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanFundNavSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate7d.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// 交易明细（tick_trade）只保留7天，删除更早的逐笔成交数据
	const char* cleanTickTradeSql = "DELETE FROM tick_trade WHERE trade_date < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanTickTradeSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate7d.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// 盘口分钟级快照只保留7天
	const char* cleanOrderBookSnapSql = "DELETE FROM order_book_snapshots WHERE snapshot_time < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanOrderBookSnapSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTime7d));
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// 预估成交量记录只保留7天
	const char* cleanEstSql = "DELETE FROM order_book_estimates WHERE trade_date < ?;";
	stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, cleanEstSql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, cutoffDate7d.c_str(), -1, SQLITE_TRANSIENT);
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

bool CStockDbManager::SaveOrderBookSnapshot(const std::wstring& stockCode, const STOCK::OrderBookSnapshotRecord& record)
{
	if (m_db == nullptr || record.levels.empty()) return false;	// 所有档位均无成交时本次不保存

	// 同一快照的最多10行放在一个事务里，保证原子性且减少WAL写入次数
	if (sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
		return false;

	bool ok = true;
	const char* sql = "INSERT OR REPLACE INTO order_book_snapshots(stock_code, snapshot_time, is_ask, level_idx, price, order_volume, cum_active_buy_vol, cum_active_sell_vol) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
	{
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	for (const auto& lv : record.levels)
	{
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(record.snapshotTime));
		sqlite3_bind_int(stmt, 3, lv.isAsk ? 1 : 0);
		sqlite3_bind_int(stmt, 4, lv.levelIndex);
		sqlite3_bind_double(stmt, 5, lv.price);
		sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(lv.orderVolume));
		sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(lv.cumActiveBuyVol));
		sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(lv.cumActiveSellVol));
		if (sqlite3_step(stmt) != SQLITE_DONE)
			ok = false;
	}
	sqlite3_finalize(stmt);

	if (!ok)
	{
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}
	sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
	return true;
}

std::vector<STOCK::OrderBookSnapshotRecord> CStockDbManager::LoadOrderBookSnapshots(
	const std::wstring& stockCode, time_t startTime)
{
	std::vector<STOCK::OrderBookSnapshotRecord> result;
	if (m_db == nullptr) return result;

	// 时间升序 + 卖盘在前(与盘口展示从卖五到买五的顺序一致) + 档位升序
	const char* sql = "SELECT snapshot_time, is_ask, level_idx, price, order_volume, cum_active_buy_vol, cum_active_sell_vol "
		"FROM order_book_snapshots WHERE stock_code = ? AND snapshot_time >= ? "
		"ORDER BY snapshot_time ASC, is_ask DESC, level_idx ASC;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return result;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(startTime));

	STOCK::OrderBookSnapshotRecord cur;
	cur.snapshotTime = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		time_t ts = static_cast<time_t>(sqlite3_column_int64(stmt, 0));
		if (ts != cur.snapshotTime)
		{
			if (cur.snapshotTime != 0)
				result.push_back(std::move(cur));
			cur = STOCK::OrderBookSnapshotRecord();
			cur.snapshotTime = ts;
		}
		STOCK::OrderBookLevelSnapshot lv;
		lv.isAsk = sqlite3_column_int(stmt, 1) != 0;
		lv.levelIndex = sqlite3_column_int(stmt, 2);
		lv.price = sqlite3_column_double(stmt, 3);
		lv.orderVolume = static_cast<long long>(sqlite3_column_int64(stmt, 4));
		lv.cumActiveBuyVol = static_cast<long long>(sqlite3_column_int64(stmt, 5));
		lv.cumActiveSellVol = static_cast<long long>(sqlite3_column_int64(stmt, 6));
		cur.levels.push_back(std::move(lv));
	}
	sqlite3_finalize(stmt);
	if (cur.snapshotTime != 0)
		result.push_back(std::move(cur));
	return result;
}

bool CStockDbManager::SaveOrderBookEstimate(const OrderBookEstimate& est)
{
	if (m_db == nullptr) return false;

	const char* sql = "INSERT INTO order_book_estimates(stock_code, trade_date, create_time, trade_type, buy_price, total_amount, order_volume, cum_volume, est_volume) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, est.stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, est.tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(est.createTime));
	sqlite3_bind_int(stmt, 4, est.tradeType);
	sqlite3_bind_double(stmt, 5, est.buyPrice);
	sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(est.totalAmount));
	sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(est.orderVolume));
	sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(est.cumVolume));
	sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(est.estVolume));

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool CStockDbManager::UpdateOrderBookEstimate(const OrderBookEstimate& est)
{
	if (m_db == nullptr || est.id <= 0) return false;

	const char* sql = "UPDATE order_book_estimates SET trade_type=?, buy_price=?, total_amount=?, order_volume=?, cum_volume=?, est_volume=? WHERE id=?;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_int(stmt, 1, est.tradeType);
	sqlite3_bind_double(stmt, 2, est.buyPrice);
	sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(est.totalAmount));
	sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(est.orderVolume));
	sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(est.cumVolume));
	sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(est.estVolume));
	sqlite3_bind_int(stmt, 7, est.id);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool CStockDbManager::DeleteOrderBookEstimates(const std::wstring& stockCode, const std::string& tradeDate,
	double buyPrice, int tradeType)
{
	if (m_db == nullptr) return false;

	const char* sql = "DELETE FROM order_book_estimates WHERE stock_code = ? AND trade_date = ? "
		"AND trade_type = ? AND ABS(buy_price - ?) < 0.0005;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, tradeType);
	sqlite3_bind_double(stmt, 4, buyPrice);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

std::vector<CStockDbManager::OrderBookEstimate> CStockDbManager::LoadOrderBookEstimates(
	const std::wstring& stockCode, const std::string& tradeDate)
{
	std::vector<OrderBookEstimate> result;
	if (m_db == nullptr) return result;

	const char* sql = "SELECT id, trade_date, create_time, trade_type, buy_price, total_amount, order_volume, cum_volume, est_volume "
		"FROM order_book_estimates WHERE stock_code = ? AND trade_date = ? ORDER BY id ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		OrderBookEstimate est;
		est.id = sqlite3_column_int(stmt, 0);
		const unsigned char* dateText = sqlite3_column_text(stmt, 1);
		est.tradeDate = dateText ? reinterpret_cast<const char*>(dateText) : "";
		est.createTime = static_cast<time_t>(sqlite3_column_int64(stmt, 2));
		est.tradeType = sqlite3_column_int(stmt, 3);
		est.buyPrice = sqlite3_column_double(stmt, 4);
		est.totalAmount = static_cast<long long>(sqlite3_column_int64(stmt, 5));
		est.orderVolume = static_cast<long long>(sqlite3_column_int64(stmt, 6));
		est.cumVolume = static_cast<long long>(sqlite3_column_int64(stmt, 7));
		est.estVolume = static_cast<long long>(sqlite3_column_int64(stmt, 8));
		est.stockCode = stockCode;
		result.push_back(std::move(est));
	}
	sqlite3_finalize(stmt);
	return result;
}

bool CStockDbManager::LoadLatestOrderBookEstimate(const std::wstring& stockCode, const std::string& tradeDate,
	double buyPrice, int tradeType, OrderBookEstimate& outEst,
	const std::string& timeHHMM)
{
	if (m_db == nullptr) return false;

	// 价格为浮点（如0.3350000000000002），量化到0.001价位后匹配，避免浮点尾数导致不命中
	// 记录加载时统一按价位归一：查询任何一条 |库价-入参价| < 0.0005（半个tick）
	// timeHHMM非空时（"HH:MM"）额外要求 create_time 落在同一分钟内，实现按价格+方向+分钟去重
	std::string sql = "SELECT id, trade_date, create_time, trade_type, buy_price, total_amount, order_volume, cum_volume, est_volume "
		"FROM order_book_estimates WHERE stock_code = ? AND trade_date = ? AND trade_type = ? "
		"AND ABS(buy_price - ?) < 0.0005";
	if (!timeHHMM.empty())
	{
		// strftime('%H:%M', create_time, 'unixepoch', 'localtime') = HH:MM
		sql += " AND strftime('%H:%M', create_time, 'unixepoch', 'localtime') = ?";
	}
	sql += " ORDER BY id DESC LIMIT 1;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, tradeType);
	sqlite3_bind_double(stmt, 4, buyPrice);
	int bindIdx = 5;
	if (!timeHHMM.empty())
		sqlite3_bind_text(stmt, bindIdx, timeHHMM.c_str(), -1, SQLITE_TRANSIENT);

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		outEst.id = sqlite3_column_int(stmt, 0);
		const unsigned char* dateText = sqlite3_column_text(stmt, 1);
		outEst.tradeDate = dateText ? reinterpret_cast<const char*>(dateText) : "";
		outEst.createTime = static_cast<time_t>(sqlite3_column_int64(stmt, 2));
		outEst.tradeType = sqlite3_column_int(stmt, 3);
		outEst.buyPrice = sqlite3_column_double(stmt, 4);
		outEst.totalAmount = static_cast<long long>(sqlite3_column_int64(stmt, 5));
		outEst.orderVolume = static_cast<long long>(sqlite3_column_int64(stmt, 6));
		outEst.cumVolume = static_cast<long long>(sqlite3_column_int64(stmt, 7));
		outEst.estVolume = static_cast<long long>(sqlite3_column_int64(stmt, 8));
		outEst.stockCode = stockCode;
		found = true;
	}
	sqlite3_finalize(stmt);
	return found;
}

bool CStockDbManager::LoadMinOrderBookVolume(const std::wstring& stockCode, time_t startTime,
	double price, bool isAsk, long long& minOrderVolume)
{
	if (m_db == nullptr) return false;

	// 挂单量取整个快照的历史最低；价格为浮点容差0.0005匹配
	const char* sql = "SELECT MIN(order_volume) FROM order_book_snapshots "
		"WHERE stock_code = ? AND snapshot_time >= ? AND is_ask = ? "
		"AND ABS(price - ?) < 0.0005;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(startTime));
	sqlite3_bind_int(stmt, 3, isAsk ? 1 : 0);
	sqlite3_bind_double(stmt, 4, price);

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL)
	{
		minOrderVolume = static_cast<long long>(sqlite3_column_int64(stmt, 0)) / 100;   // 股→手
		found = true;
	}
	sqlite3_finalize(stmt);
	return found;
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

	// 日K线：缓存中最早数据距今超过1个月才算有效
	if (period == STOCK::Period::DAY)
	{
		std::string cutoffDate1m = GetLocalDateString(GetLocalMidnightTime(-30));
		std::wstring sql = L"SELECT 1 FROM " + table + L" WHERE stock_code = ? AND day < ? LIMIT 1;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare16_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, cutoffDate1m.c_str(), -1, SQLITE_TRANSIENT);
		bool hasCache = sqlite3_step(stmt) == SQLITE_ROW;
		sqlite3_finalize(stmt);
		return hasCache;
	}

	// 5分钟/30分钟K线：缓存中最新数据在7天内即算有效
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
			std::string cutoffStr = GetCacheCutoffDateString();
			hasCache = (dayStr.length() >= 10 && dayStr.substr(0, 10) >= cutoffStr);
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

bool CStockDbManager::SaveAvgDiffStats(const std::wstring& stockCode, double minVal, double maxVal, double currentVal)
{
	if (m_db == nullptr) return false;

	std::string code(stockCode.begin(), stockCode.end());
	std::string dateStr = GetTodayDateString();

	const char* sql = "INSERT OR REPLACE INTO avg_diff_stats (stock_code, trade_date, min_avg_diff, max_avg_diff, current_avg_diff, update_time) VALUES (?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, dateStr.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 3, minVal);
		sqlite3_bind_double(stmt, 4, maxVal);
		sqlite3_bind_double(stmt, 5, currentVal);
		sqlite3_bind_text(stmt, 6, dateStr.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	return true;
}

AvgDiffStats CStockDbManager::LoadAvgDiffStats(const std::wstring& stockCode)
{
	AvgDiffStats result = { 0.0, 0.0, 0.0 };
	if (m_db == nullptr) return result;

	std::string code(stockCode.begin(), stockCode.end());
	std::string today = GetTodayDateString();

	// 只加载当天的记录，避免加载昨日数据导致重启后min/max被旧值污染
	const char* sql = "SELECT min_avg_diff, max_avg_diff, current_avg_diff FROM avg_diff_stats WHERE stock_code = ? AND trade_date = ? LIMIT 1;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, today.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			result.minVal = sqlite3_column_double(stmt, 0);
			result.maxVal = sqlite3_column_double(stmt, 1);
			result.currentVal = sqlite3_column_double(stmt, 2);
		}
	}
	sqlite3_finalize(stmt);
	return result;
}

bool CStockDbManager::SaveFundNavCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data)
{
	if (m_db == nullptr || data.empty()) return false;

	const char* sql = "INSERT OR IGNORE INTO fund_nav_cache(stock_code, trade_date, time, nav, updated_at) VALUES(?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	std::string tradeDate = GetTodayDateString();
	time_t now = time(nullptr);
	bool ok = true;
	for (const auto& item : data)
	{
		if (item.time.empty() || item.iopv <= 0) continue;
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, item.time.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 4, item.iopv);
		sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(now));
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			ok = false;
	}
	sqlite3_finalize(stmt);
	return ok;
}

std::vector<STOCK::TimelinePoint> CStockDbManager::LoadFundNavCache(const std::wstring& stockCode, const std::string& tradeDate)
{
	std::vector<STOCK::TimelinePoint> points;
	if (m_db == nullptr) return points;

	const char* sql = "SELECT time, nav FROM fund_nav_cache WHERE stock_code = ? AND trade_date = ? ORDER BY time ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return points;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::TimelinePoint point;
		const unsigned char* timeText = sqlite3_column_text(stmt, 0);
		point.time = timeText ? reinterpret_cast<const char*>(timeText) : "";
		point.iopv = sqlite3_column_double(stmt, 1);
		points.push_back(point);
	}
	sqlite3_finalize(stmt);
	return points;
}

// 过滤掉非交易时间的净值记录（防止非交易时段写入的脏数据被加载）
// A股交易时段：9:30-11:30, 13:00-15:00
static bool IsValidNavTime(const std::string& timeStr)
{
	if (timeStr.size() < 5) return false;
	int hour = 0, minute = 0;
	if (sscanf_s(timeStr.c_str(), "%d:%d", &hour, &minute) != 2) return false;
	int minutes = hour * 60 + minute;
	// 9:30-11:30
	if (minutes >= 9 * 60 + 30 && minutes <= 11 * 60 + 30) return true;
	// 13:00-15:00
	if (minutes >= 13 * 60 && minutes <= 15 * 60) return true;
	return false;
}

static std::vector<STOCK::TimelinePoint> FilterValidNavPoints(const std::vector<STOCK::TimelinePoint>& points)
{
	std::vector<STOCK::TimelinePoint> filtered;
	filtered.reserve(points.size());
	for (const auto& p : points)
	{
		if (IsValidNavTime(p.time))
			filtered.push_back(p);
	}
	return filtered;
}

std::vector<STOCK::TimelinePoint> CStockDbManager::LoadLatestFundNavCache(const std::wstring& stockCode)
{
	std::string tradeDate = GetTodayDateString();
	auto points = LoadFundNavCache(stockCode, tradeDate);
	if (!points.empty()) return FilterValidNavPoints(points);

	// 今天没有缓存时，回退加载最近交易日的基金净值
	if (m_db == nullptr) return points;

	const char* fallbackSql = "SELECT time, nav FROM fund_nav_cache WHERE stock_code = ? AND trade_date = (SELECT trade_date FROM fund_nav_cache WHERE stock_code = ? ORDER BY trade_date DESC LIMIT 1) ORDER BY time ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, fallbackSql, -1, &stmt, nullptr) != SQLITE_OK) return points;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text16(stmt, 2, stockCode.c_str(), -1, SQLITE_TRANSIENT);

	std::vector<STOCK::TimelinePoint> rawPoints;
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::TimelinePoint point;
		const unsigned char* timeText = sqlite3_column_text(stmt, 0);
		point.time = timeText ? reinterpret_cast<const char*>(timeText) : "";
		point.iopv = sqlite3_column_double(stmt, 1);
		rawPoints.push_back(point);
	}
	sqlite3_finalize(stmt);
	return FilterValidNavPoints(rawPoints);
}

bool CStockDbManager::SaveTransactions(const std::wstring& stockCode,
	const std::string& tradeDate, const std::vector<STOCK::Transaction>& data)
{
	if (m_db == nullptr || data.empty()) return false;

	// 先删除同一天同一股票的历史明细，避免重复累加（同一天数据是幂等覆盖的）
	if (!DeleteTransactions(stockCode, tradeDate)) return false;

	const char* sql = "INSERT INTO tick_trade(code, trade_date, time_key, price, vol, buyorsell) VALUES(?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	bool ok = true;
	for (const auto& item : data)
	{
		if (item.timeKey.empty()) continue;
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, item.timeKey.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 4, item.price);
		sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(item.vol));
		sqlite3_bind_int(stmt, 6, item.buyOrSell);
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			ok = false;
	}
	sqlite3_finalize(stmt);
	return ok;
}

bool CStockDbManager::DeleteTransactions(const std::wstring& stockCode, const std::string& tradeDate)
{
	if (m_db == nullptr) return false;

	const char* sql = "DELETE FROM tick_trade WHERE code = ? AND trade_date = ?;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) return false;

	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return rc == SQLITE_DONE;
}

std::vector<STOCK::Transaction> CStockDbManager::LoadTransactions(const std::wstring& stockCode,
	const std::string& tradeDate)
{
	std::vector<STOCK::Transaction> result;
	if (m_db == nullptr) return result;

	const char* sql = "SELECT time_key, price, vol, buyorsell FROM tick_trade "
		"WHERE code = ? AND trade_date = ? ORDER BY id ASC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::Transaction item;
		const unsigned char* timeText = sqlite3_column_text(stmt, 0);
		item.timeKey = timeText ? reinterpret_cast<const char*>(timeText) : "";
		item.price = sqlite3_column_double(stmt, 1);
		item.vol = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
		item.buyOrSell = sqlite3_column_int(stmt, 3);
		result.push_back(std::move(item));
	}
	sqlite3_finalize(stmt);
	return result;
}

std::vector<STOCK::Transaction> CStockDbManager::LoadLatestTransactions(const std::wstring& stockCode, int limit)
{
	std::vector<STOCK::Transaction> result;
	if (m_db == nullptr || limit <= 0) return result;

	// 按自增id倒序取最新N条；只取主动买(0)/主动卖(1)，排除中性(2)、集合竞价(5)、尾盘定价(8)
	const char* sql = "SELECT time_key, price, vol, buyorsell FROM tick_trade "
		"WHERE code = ? AND buyorsell IN (0,1) ORDER BY id DESC LIMIT ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, limit);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::Transaction item;
		const unsigned char* timeText = sqlite3_column_text(stmt, 0);
		item.timeKey = timeText ? reinterpret_cast<const char*>(timeText) : "";
		item.price = sqlite3_column_double(stmt, 1);
		item.vol = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
		item.buyOrSell = sqlite3_column_int(stmt, 3);
		result.push_back(std::move(item));
	}
	sqlite3_finalize(stmt);
	return result;	// 已按id倒序，最新在前
}

std::vector<STOCK::PriceVolumeStat> CStockDbManager::LoadPriceVolumeStats(const std::wstring& stockCode,
	const std::string& tradeDate, int buyOrSell)
{
	std::vector<STOCK::PriceVolumeStat> result;
	if (m_db == nullptr) return result;

	// 先对 (time_key, price, vol, buyorsell) 去重再按档求和：
	// 采集端偶发会把同一笔成交原样重复INSERT（实测某档重复率高达37~70倍），
	// 若直接SUM会把这些重复量全部累加，导致“没成交累计量也显示很大”。正常日去重前后几乎不变。
	std::string sql = "SELECT price, buyorsell, SUM(vol) FROM ( "
		"SELECT DISTINCT time_key, price, vol, buyorsell FROM tick_trade "
		"WHERE code = ? AND trade_date = ?";
	if (buyOrSell == 0 || buyOrSell == 1)
	{
		// 指定具体方向（0=S卖 或 1=B买）
		sql += " AND buyorsell = ?";
	}
	else
	{
		// 默认只统计买卖两个方向(0,1)，排除中性(2)等其它值
		sql += " AND buyorsell IN (0,1)";
	}
	sql += " ) GROUP BY price, buyorsell ORDER BY price ASC, buyorsell ASC;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
	sqlite3_bind_text16(stmt, 1, stockCode.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	if (buyOrSell == 0 || buyOrSell == 1)
		sqlite3_bind_int(stmt, 3, buyOrSell);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		STOCK::PriceVolumeStat stat;
		stat.price = sqlite3_column_double(stmt, 0);
		stat.buyOrSell = sqlite3_column_int(stmt, 1);
		stat.vol = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
		result.push_back(std::move(stat));
	}
	sqlite3_finalize(stmt);
	return result;
}

std::map<std::string, STOCK::TickSummary> CStockDbManager::LoadTickSummary(
	const std::wstring& stockCode,
	const std::string& tradeDate)
{
	std::map<std::string, STOCK::TickSummary> result;
	if (m_db == nullptr) return result;

	std::string sql = "SELECT trade_date, time_key,"
		"SUM(CASE WHEN buyorsell = 0 THEN vol ELSE 0 END) AS vol_buy,"
		"SUM(CASE WHEN buyorsell = 1 THEN vol ELSE 0 END) AS vol_sell,"
		"SUM(CASE buyorsell WHEN 0 THEN vol WHEN 1 THEN - vol ELSE 0 END) AS net_vol "
		"FROM main.tick_trade WHERE trade_date = ? AND code = ? "
		"GROUP BY trade_date, code, time_key "
		"ORDER BY time_key";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
	sqlite3_bind_text(stmt, 1, tradeDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text16(stmt, 2, stockCode.c_str(), -1, SQLITE_TRANSIENT);

	STOCK::TickSummary ret;
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char* dateText = sqlite3_column_text(stmt, 0);
		ret.tradeDate = dateText ? reinterpret_cast<const char*>(dateText) : "";
		const unsigned char* timeText = sqlite3_column_text(stmt, 1);
		ret.timeKey = timeText ? reinterpret_cast<const char*>(timeText) : "";
		ret.buy = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 2));
		ret.sell = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 3));
		ret.netBuy = static_cast<STOCK::Volume>(sqlite3_column_int64(stmt, 4));
		result.emplace(ret.timeKey, std::move(ret));
	}
	sqlite3_finalize(stmt);
	return result;
}