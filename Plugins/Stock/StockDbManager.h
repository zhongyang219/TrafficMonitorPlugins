#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <tuple>
#include "StockDef.h"

struct sqlite3;

// 均幅统计数据
struct AvgDiffStats
{
	double minVal;
	double maxVal;
	double currentVal;
};

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

	// 交易明细（一档行情逐笔成交）
	// 批量插入，写前会先按 (code, trade_date) 删除同日旧数据，保证幂等覆盖
	bool SaveTransactions(const std::wstring& stockCode, const std::string& tradeDate,
		const std::vector<STOCK::Transaction>& data);
	// 按 (code, trade_date) 删除
	bool DeleteTransactions(const std::wstring& stockCode, const std::string& tradeDate);
	// 按 code + trade_date 批量查询（按插入顺序，即时间先后返回）
	std::vector<STOCK::Transaction> LoadTransactions(const std::wstring& stockCode,
		const std::string& tradeDate);
	// 获取某股票最近 limit 条成交明细（按写入顺序倒序，最新在前）
	// 仅包含主动买(0)/主动卖(1)，排除中性(2)、集合竞价(5)、尾盘定价(8)
	std::vector<STOCK::Transaction> LoadLatestTransactions(const std::wstring& stockCode, int limit = 20);
	// 按 code + trade_date 统计每档价格在买卖方向上的总成交量（先按 time_key/price/vol/buyorsell 去重，防采集端重复INSERT虚增）
	// buyOrSell: 0=S卖 / 1=B买 时仅统计该方向；其它值(默认-1)只统计方向0和1，排除中性(2)
	std::vector<STOCK::PriceVolumeStat> LoadPriceVolumeStats(const std::wstring& stockCode,
		const std::string& tradeDate, int buyOrSell = -1);

	//按股票名称和日期统计每分钟的交易记录数量（仅包含主动买/主动卖，排除中性、集合竞价、尾盘定价）
	std::map<std::string, STOCK::TickSummary> LoadTickSummary(const std::wstring& stockCode, const std::string& tradeDate);

	// 内外盘快照
	bool SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp,
		STOCK::Volume innerVolume, STOCK::Volume outerVolume);
	// 加载指定股票从 startTime 之后的全部快照（按时间升序）
	std::vector<std::tuple<time_t, STOCK::Volume, STOCK::Volume>> LoadInnerOuterSnapshots(
		const std::wstring& stockCode, time_t startTime);

	// 盘口分钟级快照（每分钟保存五档买卖盘价格/挂单量 + 累计主动买卖量）
	bool SaveOrderBookSnapshot(const std::wstring& stockCode, const STOCK::OrderBookSnapshotRecord& record);
	// 加载指定股票从 startTime 之后的全部盘口快照（按时间升序）
	std::vector<STOCK::OrderBookSnapshotRecord> LoadOrderBookSnapshots(
		const std::wstring& stockCode, time_t startTime);

	// 预估成交量记录（双击盘口行时用户录入的"买入时"快照，实时列不入库）
	struct OrderBookEstimate
	{
		int id{ 0 };
		std::wstring stockCode;
		std::string tradeDate;        // "YYYY-MM-DD"
		time_t createTime{ 0 };       // 创建时间戳
		int tradeType{ 0 };           // 0=买入, 1=卖出
		double buyPrice{ 0.0 };       // 买入价格
		long long totalAmount{ 0 };   // 总数量
		long long orderVolume{ 0 };   // 当前挂单量
		long long cumVolume{ 0 };     // 当前累计成交量
		long long estVolume{ 0 };     // 买入预计成交累积量（=挂单量+累计成交量）
	};
	bool SaveOrderBookEstimate(const OrderBookEstimate& est);
	// 按id更新数值列（total/order/cum/est/buy_price/trade_type），id和create_time不变
	bool UpdateOrderBookEstimate(const OrderBookEstimate& est);
	// 删除某股票某交易日、指定价格+买卖方向（价格0.0005容差）的全部预估记录
	bool DeleteOrderBookEstimates(const std::wstring& stockCode, const std::string& tradeDate,
		double buyPrice, int tradeType);
	// 加载某股票某交易日的全部预估记录（按id升序）
	std::vector<OrderBookEstimate> LoadOrderBookEstimates(
		const std::wstring& stockCode, const std::string& tradeDate);
	// 查询某股票某交易日、指定价格+买卖方向的一条预估记录（取最新一条，供双击盘口回显）
	// timeHHMM非空时额外要求 create_time 落在同一 HH:MM（按价格+方向+时间去重）
	bool LoadLatestOrderBookEstimate(const std::wstring& stockCode, const std::string& tradeDate,
		double buyPrice, int tradeType, OrderBookEstimate& outEst,
		const std::string& timeHHMM = "");
	// 查询某股票某交易日、指定价格+盘别在 [startTime, +∞) 的最低挂单量（手），供剩余排队计算吸收中途撤单
	// 找不到该价位记录时返回false（minOrderVolume保持入参）
	bool LoadMinOrderBookVolume(const std::wstring& stockCode, time_t startTime,
		double price, bool isAsk, long long& minOrderVolume);

	// 分时缓存
	bool SaveTimelineCache(const std::wstring& stockCode,
		const std::vector<STOCK::TimelinePoint>& data);
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

	// 关联股票均幅统计
	bool SaveAvgDiffStats(const std::wstring& stockCode, double minVal, double maxVal, double currentVal);
	AvgDiffStats LoadAvgDiffStats(const std::wstring& stockCode);

	// 基金净值按分钟缓存
	bool SaveFundNavCache(const std::wstring& stockCode,
		const std::vector<STOCK::TimelinePoint>& data);
	std::vector<STOCK::TimelinePoint> LoadFundNavCache(const std::wstring& stockCode,
		const std::string& tradeDate);
	std::vector<STOCK::TimelinePoint> LoadLatestFundNavCache(const std::wstring& stockCode);

private:
	sqlite3* m_db{ nullptr };
	std::wstring m_db_path;
};
