#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <tuple>
#include "StockDef.h"

struct sqlite3;

// 股票数据库管理类
// 负责所有 SQLite 数据库的 CRUD 操作，与业务逻辑、UI 解耦。
// CDataManager 持有其一个实例，并将原数据库方法转发到此。
class CStockDbManager
{
public:
	CStockDbManager();
	~CStockDbManager();

	CStockDbManager(const CStockDbManager&) = delete;
	CStockDbManager& operator=(const CStockDbManager&) = delete;

	// 数据库生命周期管理
	// 根据配置文件路径初始化数据库连接并创建所有表
	bool Init(const std::wstring& config_path);
	void Close();
	bool IsOpen() const { return m_db != nullptr; }
	// 清理超过7天的快照、K线和筹码峰缓存
	void CleanExpiredData();

	// 交易记录
	bool SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName,
		int tradeType, const std::wstring& time,
		double price, double amount, double totalAmount,
		double fee, double total);

	// 内外盘快照
	bool SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp,
		STOCK::Volume innerVolume, STOCK::Volume outerVolume);
	// 加载指定股票从 startTime 之后的全部快照（按时间升序）
	std::vector<std::tuple<time_t, STOCK::Volume, STOCK::Volume>> LoadInnerOuterSnapshots(
		const std::wstring& stockCode, time_t startTime);

	// 分时缓存
	bool SaveTimelineCache(const std::wstring& stockCode,
		const std::vector<STOCK::TimelinePoint>& data);
	bool HasTimelineCache(const std::wstring& stockCode);
	// 加载指定交易日的分时数据
	std::vector<STOCK::TimelinePoint> LoadTimelineCache(const std::wstring& stockCode,
		const std::string& tradeDate);
	// 加载最近一个交易日的分时数据（今天无缓存时回退）
	std::vector<STOCK::TimelinePoint> LoadLatestTimelineCache(const std::wstring& stockCode);

	// K线缓存
	bool SaveKLineCache(const std::wstring& stockCode, STOCK::Period period,
		const std::vector<STOCK::KLinePoint>& data);
	bool HasKLineCache(const std::wstring& stockCode, STOCK::Period period);
	std::vector<STOCK::KLinePoint> LoadKLineCache(const std::wstring& stockCode,
		STOCK::Period period);

	// 股票基础数据
	bool SaveStockBasicData(const std::wstring& stockCode, STOCK::Volume circulatingAShares);
	bool LoadStockBasicData(const std::wstring& stockCode, STOCK::Volume& outCirculatingAShares);

	// 筹码分布
	bool SaveChipDistribution(const std::wstring& stockCode,
		const STOCK::ChipDistribution& chipData);
	bool LoadLatestChipDistribution(const std::wstring& stockCode,
		STOCK::ChipDistribution& chipData);

	// 关联股票最高均幅
	bool SaveMaxAvgDiff(const std::wstring& stockCode, double maxAvgDiff);
	double LoadMaxAvgDiff(const std::wstring& stockCode);
	void CleanExpiredMaxAvgDiff();

	// 关联股票最低均幅
	bool SaveMinAvgDiff(const std::wstring& stockCode, double minAvgDiff);
	double LoadMinAvgDiff(const std::wstring& stockCode);
	void CleanExpiredMinAvgDiff();

	// 基金净值按分钟缓存
	bool SaveFundNavCache(const std::wstring& stockCode,
		const std::vector<STOCK::TimelinePoint>& data);
	std::vector<STOCK::TimelinePoint> LoadFundNavCache(const std::wstring& stockCode,
		const std::string& tradeDate);
	std::vector<STOCK::TimelinePoint> LoadLatestFundNavCache(const std::wstring& stockCode);
	bool HasFundNavCache(const std::wstring& stockCode);

private:
	sqlite3* m_db{ nullptr };
	std::wstring m_db_path;
};
