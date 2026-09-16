#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <iostream>
#include <limits>
#include <ctime>
#include <algorithm>
#include "MarketOrderBook.h"

// 线性回归结果（基础数据结构，供CSignalAnalyzer/CDataManager等使用）
struct RegResult
{
	bool valid;    // true=数据足够，结果可信
	double slope;  // 回归斜率k
	double r2;     // 拟合优度R²

	int getArrawCount() const
	{
		if (!valid)
			return 0;
		if (r2 >= 0.7)
		{
			if (r2 >= 0.7)
				return 3; // 高强度箭头
			else if (r2 >= 0.55)
				return 2; // 中强度箭头
			else
				return 1; // 弱强度箭头
		}
		return 0; // 无明显趋势
	}
};

namespace STOCK
{
	// 价格
	using Price = double;
	// 数量
	using Volume = long long;
	// 金额
	using Amount = double;
	using TimePoint = std::string;

	// 买卖盘信息
	struct OrderLevel
	{
		Price price;   // 价格
		Volume volume; // 数量

		OrderLevel() : price(0.0), volume(0) {}
		OrderLevel(Price p, Volume v) : price(p), volume(v) {}
	};

	// 分时数据点
	struct TimelinePoint
	{
		TimePoint time;        // 时间点
		TimePoint fullTime;    // 完整日期时间（5分/30分K线模式：yyyy-mm-dd hh:mm）
		Volume volume;         // 成交量
		Price price;           // 价格
		Price openPrice;       // 开盘价（K线模式下用于判断涨跌颜色，分时模式下为0）
		Price averagePrice;    // 均价（全天累计）
		Amount amount;         // 成交额（price * volume）
		Price ma5;             // 5分钟滚动均价
		Price ma10;            // 10分钟滚动均价
		Price ma20;            // 20分钟滚动均价
		Price iopv;            // IOPV 基金份额参考净值

		TimelinePoint() : volume(0), price(0), openPrice(0), averagePrice(0), amount(0), ma5(0), ma10(0), ma20(0), iopv(0) {}
	};

	// 交易明细点（一档行情逐笔成交）
	struct Transaction
	{
		std::string timeKey;   // 接口返回时间 HH:MM（无秒）
		Price price{ 0.0 };    // 成交价格
		Volume vol{ 0 };       // 成交量（手）
		int buyOrSell{ 0 };    // 1=B买，0=S卖，2=中性

		Transaction() = default;
		Transaction(std::string tk, Price p, Volume v, int b)
			: timeKey(tk), price(p), vol(v), buyOrSell(b) {
		}
	};

	// 每分钟交易明细汇总统计
	struct TickSummary
	{
		std::string tradeDate; // 交易日期 YYYY-MM-DD
		std::string timeKey;   // 接口返回时间 HH:MM（无秒）
		Volume buy{ 0 };       // 主动买入成交量（手）
		Volume sell{ 0 };      // 主动卖出成交量（手）
		Volume netBuy;		   // 净买入成交量（手）

		TickSummary() = default;
		TickSummary(std::string td, std::string tk, Volume b, Volume s, Volume nb)
			:tradeDate(td), timeKey(tk), buy(b), sell(s), netBuy(nb) {
		}

		double BuyRatio() const
		{
			if (buy + sell == 0) return 0.0;
			return static_cast<double>(buy) / (buy + sell);
		}
		double SellRatio() const
		{
			if (buy + sell == 0) return 0.0;
			return static_cast<double>(sell) / (buy + sell);
		}
	};

	// 每档价格的买卖方向成交量统计结果
	struct PriceVolumeStat
	{
		Price price{ 0.0 };    // 成交价格档位
		int buyOrSell{ 0 };    // 1=B买，0=S卖，2=中性
		Volume vol{ 0 };       // 该档该方向的总成交量（手）

		PriceVolumeStat() = default;
		PriceVolumeStat(Price p, int b, Volume v) : price(p), buyOrSell(b), vol(v) {}
	};

	// 筹码分布数据点
	struct ChipPoint
	{
		Price price{ 0.0 };      // 价格档位
		double percent{ 0.0 };   // 对应筹码占比
	};

	// 筹码峰数据
	struct ChipDistribution
	{
		std::string tradeDate;
		time_t updatedAt{ 0 };
		double avgCost{ 0.0 };
		double benefitRatio{ 0.0 };
		double cost70Low{ 0.0 };
		double cost70High{ 0.0 };
		double cost70Concentration{ 0.0 };
		double cost90Low{ 0.0 };
		double cost90High{ 0.0 };
		double cost90Concentration{ 0.0 };
		std::vector<ChipPoint> points;

		void Clear()
		{
			tradeDate.clear();
			updatedAt = 0;
			avgCost = 0.0;
			benefitRatio = 0.0;
			cost70Low = 0.0;
			cost70High = 0.0;
			cost70Concentration = 0.0;
			cost90Low = 0.0;
			cost90High = 0.0;
			cost90Concentration = 0.0;
			points.clear();
		}
		bool IsValid() const { return !tradeDate.empty() && !points.empty(); }
	};

	// 周期类型
	enum class Period
	{
		TIMELINE,  // 分时
		DAY,       // 日线
		WEEK,      // 周线
		MONTH,     // 月线
		YEAR,      // 年线
		MIN5,      // 5分钟线
		MIN30      // 30分钟线
	};

	// 历史数据基类
	class HistoricalDataBase
	{
	public:
		virtual ~HistoricalDataBase() = default;
		virtual Period GetPeriod() const = 0;
		virtual TimePoint GetStartTime() const = 0;
		virtual TimePoint GetEndTime() const = 0;
	};

	// 分时历史数据
	class TimelineData : public HistoricalDataBase
	{
	public:
		std::vector<TimelinePoint> data;
		TimelineData() {};
		Period GetPeriod() const override { return Period::TIMELINE; }
		TimePoint GetStartTime() const { return data.front().time; }
		TimePoint GetEndTime() const { return data.back().time; }
		void Clear() { data.clear(); }
	};

	// // 日K线数据点
	struct KLinePoint
	{
		std::string day;      // 日期 YYYY-MM-DD
		Price open;           // 开盘价
		Price high;           // 最高价
		Price low;            // 最低价
		Price close;          // 收盘价
		Volume volume;        // 成交量
	};

	// 筹码分布计算用K线点（含换手率）
	// 由数据获取层（HTTP fetcher）填充，交由 DataManager 计算筹码分布
	struct ChipKLinePoint
	{
		std::string date;
		double open{ 0.0 };
		double close{ 0.0 };
		double high{ 0.0 };
		double low{ 0.0 };
		double turnoverRate{ 0.0 };  // 换手率(%)
		Volume volume{ 0 };          // 成交量(股)
	};

	// ========== 智能分析模块：统一K线基础结构体 ==========
	// 每一根K线统一存储，用于30min/5min周期指标计算
	struct Bar
	{
		double open;     // 开盘
		double high;     // 最高
		double low;      // 最低
		double close;    // 收盘
		double volume;   // 成交量
		long   time;     // 时间戳，区分30min/5min周期

		Bar() : open(0), high(0), low(0), close(0), volume(0), time(0) {}
		Bar(double o, double h, double l, double c, double v, long t)
			: open(o), high(h), low(l), close(c), volume(v), time(t) {
		}

		// 从KLinePoint转换（日K/5minK/30minK通用）
		static Bar FromKLinePoint(const KLinePoint& kp, long timestamp = 0)
		{
			return Bar(kp.open, kp.high, kp.low, kp.close, static_cast<double>(kp.volume), timestamp);
		}
	};

	// 30分钟趋势状态
	enum class TrendState30m
	{
		STATE_WEAK,        // 弱势：仅反T，禁止加仓正T
		STATE_WEAK_SHAKE,  // 弱震荡：偏弱但信号不足，谨慎操作
		STATE_SHAKE,       // 震荡：正反T均可
		STATE_STRONG       // 强势：回踩可低吸正T
	};

	// 30分钟趋势判定结果（含置信度）
	struct TrendStateResult
	{
		TrendState30m state;     // 趋势状态
		int confidence;          // 置信度 0-100，越高越可信（<50为低可信度）
		double weakScore;        // 弱势加权得分
		double strongScore;      // 强势加权得分

		TrendStateResult() : state(TrendState30m::STATE_SHAKE), confidence(0), weakScore(0), strongScore(0) {}
	};

	// 5分钟共振信号
	enum class Signal5m
	{
		SIG_SELL,  // 卖出信号
		SIG_BUY,   // 买入信号
		SIG_NONE   // 无信号
	};

	// 趋势方向（双周期共振判定结果）
	enum class TrendDir
	{
		DIR_UP,    // 上涨
		DIR_DOWN,  // 下跌
		DIR_SIDE   // 震荡
	};

	// 震荡区间位置标记
	enum class SideTag
	{
		SIDE_LONG_POINT,  // 震荡下轨低吸点
		SIDE_SHORT_POINT, // 震荡上轨高抛点
		SIDE_MID          // 震荡中间，观望
	};

	// 双周期共振趋势判定结果
	struct TrendResult
	{
		TrendDir FinalTrend;      // 最终趋势: DIR_UP / DIR_DOWN / DIR_SIDE
		TrendDir BaseDir;         // 30分钟底层方向
		bool Is5Up;               // 5分钟短线多头
		bool Is5Down;             // 5分钟短线空头
		bool IsShortPullback;     // 上涨趋势中短线回调
		bool IsShortRebound;      // 下跌趋势中短线反弹
		SideTag SideTagValue;     // 震荡区间位置标记
		double OuterInnerRatio;   // 内外盘净比 (外盘-内盘)/(外盘+内盘)

		TrendResult() : FinalTrend(TrendDir::DIR_SIDE), BaseDir(TrendDir::DIR_SIDE),
			Is5Up(false), Is5Down(false), IsShortPullback(false), IsShortRebound(false),
			SideTagValue(SideTag::SIDE_MID), OuterInnerRatio(0.0) {
		}
	};

	// 布林带计算结果
	struct BollResult
	{
		double mid;       // 中轨（MA20）
		double up;        // 上轨
		double dn;        // 下轨
		double bandwidth; // 带宽
		BollResult() : mid(0), up(0), dn(0), bandwidth(0) {}
	};

	// MACD计算结果
	struct MACDResult
	{
		double dif;      // DIF
		double dea;      // DEA
		double macd_bar; // MACD柱值
		MACDResult() : dif(0), dea(0), macd_bar(0) {}
	};

	// KDJ计算结果
	struct KDJResult
	{
		double k;
		double d;
		double j;
		KDJResult() : k(0), d(0), j(0) {}
	};

	// K线历史数据
	class KLineData : public HistoricalDataBase
	{
	public:
		std::vector<KLinePoint> data;
		Period GetPeriod() const override { return Period::DAY; }
		TimePoint GetStartTime() const override { return data.empty() ? "" : data.front().day; }
		TimePoint GetEndTime() const override { return data.empty() ? "" : data.back().day; }
		void Clear() { data.clear(); }

		// 计算N日均线（从末尾往前取period天）
		double CalculateMA(int period) const;
		// 计算N日均线（从1年/2年/3年的周期范围内计算）
		double CalculateMAPeriod(int days, int totalDays) const;
		// 计算N日平均振幅
		double CalculateAverageAmplitude(int days) const;
	};

	// 5分钟K线历史数据（继承自KLineData，复用CalculateMA等方法）
	class Min5KLineData : public KLineData
	{
	public:
		Period GetPeriod() const override { return Period::MIN5; }
	};

	// 30分钟K线历史数据（继承自KLineData，复用CalculateMA等方法）
	class Min30KLineData : public KLineData
	{
	public:
		Period GetPeriod() const override { return Period::MIN30; }
	};

	// 股票基础信息
	struct StockInfo
	{
		std::wstring code;              // 股票代码
		std::wstring displayName = L""; // 股票名称

		bool is_ok = TRUE;              // 加载成功标志

		Price openPrice;      // 今日开盘价
		Price prevClosePrice; // 昨日收盘价
		Price currentPrice;   // 当前价格
		Price highPrice;      // 最高价
		Price lowPrice;       // 最低价
		Volume volume;        // 成交量(股)
		Amount turnover;      // 成交额(元)

		Volume innerVolume;   // 内盘(股) - 主动卖出
		Volume outerVolume;   // 外盘(股) - 主动买入
		Amount turnoverRate;  // 换手率(%)
		Volume circulatingAShares; // 无限售流通A股(股)

		std::wstring displayPrice = L" --";
		std::wstring displayFluctuationDiff = L"+--";  // 涨跌额
		std::wstring displayFluctuation = L"--% ";      // 涨跌幅百分比

		Price priceLimit; // 价格限制

		// ETF基金份额参考净值(IOPV)相关数据
		Price iopv;              // IOPV 基金份额参考净值
		Price iopvPrevClose;     // IOPV 昨收（前一日参考净值）
		double iopvPremium;      // 溢价额 = 当前价 - IOPV
		double iopvPremiumRate;  // 溢折率 = (当前价 - IOPV) / IOPV * 100(%)
		double iopvChange;       // IOPV涨跌幅 = (IOPV - iopvPrevClose) / iopvPrevClose * 100(%)

		static const int MAX_LEVEL = 5;

		OrderLevel askLevels[MAX_LEVEL]; // 卖盘(5档)
		OrderLevel bidLevels[MAX_LEVEL]; // 买盘(5档)

		StockInfo() : openPrice(0.0),
			prevClosePrice(0.0),
			currentPrice(0.0),
			highPrice(0.0),
			lowPrice(0.0),
			volume(0),
			turnover(0.0),
			innerVolume(0),
			outerVolume(0),
			turnoverRate(0.0),
			circulatingAShares(0),
			priceLimit(0.0),
			iopv(0.0),
			iopvPrevClose(0.0),
			iopvPremium(0.0),
			iopvPremiumRate(0.0),
			iopvChange(0.0)
		{
			// bidLevels.resize(MAX_LEVEL);
			// askLevels.resize(MAX_LEVEL);
		}

		void Load(std::wstring key, std::vector<std::string> data);
		void UpdateDisplayFields();  // 更新displayPrice/displayFluctuation等显示字段
		void LoadMG(std::vector<std::string> data, size_t size);
		void LoadAG(std::vector<std::string> data, size_t size);
		void LoadSH(std::vector<std::string> data, size_t size);
		void LoadSZ(std::vector<std::string> data, size_t size);
		void LoadBJ(std::vector<std::string> data, size_t size);
		void LoadHK(std::vector<std::string> data, size_t size);
		void LoadNF(std::vector<std::string> data, size_t size);
		void LoadHF(std::vector<std::string> data, size_t size);

		CString GetStockShortName();

		CString GetStockListName() const;

		// 涨跌额
		double GetChangeAmount() const { return currentPrice - prevClosePrice; }
		// 涨跌幅
		double GetChangePercent() const { return prevClosePrice != 0 ? (currentPrice - prevClosePrice) / prevClosePrice * 100 : 0; }
		// 振幅
		double GetFluctuation() const { return highPrice - lowPrice; }
		double GetFluctuationPercent() const { return prevClosePrice != 0 ? (highPrice - lowPrice) / prevClosePrice * 100 : 0; }
		// 均价（成交额/成交量）
		double GetAveragePrice() const { return volume > 0 ? turnover / volume : 0; }

		bool IsETF() const;

		Volume GetMaxOrderVolume() const
		{
			Volume maxVol = 0;
			for (int i = 0; i < MAX_LEVEL; ++i)
			{
				if (askLevels[i].volume >= maxVol) maxVol = askLevels[i].volume;
				if (bidLevels[i].volume >= maxVol) maxVol = bidLevels[i].volume;
			}
			return maxVol;
		}
		double GetOrderRowRatio(Volume rowVol) const
		{
			Volume maxVol = GetMaxOrderVolume();
			if (maxVol == 0) return 0.0;
			return static_cast<double>(rowVol) / maxVol;
		}

		bool IsHighstPrice(Price price) const { return highPrice > 0 && price == highPrice; }
		bool IsLowestPrice(Price price) const { return lowPrice > 0 && price == lowPrice; }
		bool IsCurrentPrice(Price price) const { return currentPrice > 0 && price == currentPrice; }

		double GetPriceChange() const;
	};

	// 集合竞价快照数据点
	struct CallAuctionSnapshot
	{
		time_t timestamp{ 0 };       // 快照时间
		Price matchPrice{ 0.0 };     // 虚拟撮合价
		Volume matchVolume{ 0 };     // 虚拟撮合量（股）
		Volume addVol{ 0 };          // 本次新增撮合量（当前matchVolume - 上一次matchVolume）
		Volume totalAskVolume{ 0 };  // 委卖总量
		Volume totalBidVolume{ 0 };  // 委买总量
		Volume unmatchAskVol{ 0 };   // 未匹配委卖量
		Volume unmatchBidVol{ 0 };   // 未匹配委买量
		Price askLevels[5];          // 卖一到卖五价格
		Volume askVolumes[5];        // 卖一到卖五量
		Price bidLevels[5];          // 买一到买五价格
		Volume bidVolumes[5];        // 买一到买五量

		CallAuctionSnapshot()
		{
			memset(askLevels, 0, sizeof(askLevels));
			memset(askVolumes, 0, sizeof(askVolumes));
			memset(bidLevels, 0, sizeof(bidLevels));
			memset(bidVolumes, 0, sizeof(bidVolumes));
		}
	};

	// 集合竞价数据（9:15-9:30 期间持续更新）
	struct CallAuctionData
	{
		bool isValid{ false };           // 数据是否有效
		time_t lastUpdateTime{ 0 };      // 最后更新时间
		Price prevClosePrice{ 0.0 };     // 昨收价
		Price matchPrice{ 0.0 };         // 当前虚拟撮合价
		Volume matchVolume{ 0 };         // 当前虚拟撮合量
		Volume totalAskVolume{ 0 };      // 委卖总量
		Volume totalBidVolume{ 0 };      // 委买总量
		Price limitUpPrice{ 0.0 };       // 涨停价
		Price limitDownPrice{ 0.0 };     // 跌停价
		Price openPrice{ 0.0 };          // 开盘价（9:25后确定）
		OrderLevel askLevels[5];         // 卖一到卖五
		OrderLevel bidLevels[5];         // 买一到买五
		std::vector<CallAuctionSnapshot> snapshots; // 竞价期间快照序列

		void Clear()
		{
			isValid = false;
			lastUpdateTime = 0;
			prevClosePrice = 0.0;
			matchPrice = 0.0;
			matchVolume = 0;
			totalAskVolume = 0;
			totalBidVolume = 0;
			limitUpPrice = 0.0;
			limitDownPrice = 0.0;
			openPrice = 0.0;
			snapshots.clear();
		}

		// 添加快照（最多保留300条，覆盖9:15-9:30共15分钟每3秒一条）
		void AddSnapshot(const CallAuctionSnapshot& snapshot)
		{
			snapshots.push_back(snapshot);
			if (snapshots.size() > 300)
				snapshots.erase(snapshots.begin());
		}
	};

	// 盘口价格累计数据
	struct OrderPriceAccum
	{
		Volume prevVolume{ 0 };       // 上一次该价格挂盘数量
		Volume deltaVolume{ 0 };      // 本次挂单瞬时变化量（正=增加，负=减少，0=无变化）
		bool isAskSide{ true };       // true=卖方，false=买方
	};

	// 盘口分钟级快照单档记录
	// 每档 = 盘口挂单信息 + 该价格对应的累计主动成交量（与盘口界面明细同口径，tick_trade聚合）
	struct OrderBookLevelSnapshot
	{
		bool isAsk{ true };          // true=卖盘, false=买盘
		int levelIndex{ 0 };         // 档位下标 0~4（0=卖一/买一）
		double price{ 0.0 };         // 档位价格
		long long orderVolume{ 0 };  // 该档挂单量
		long long cumActiveBuyVol{ 0 };  // 该价格累计主动买入量（无记录为0）
		long long cumActiveSellVol{ 0 }; // 该价格累计主动卖出量（无记录为0）
	};

	// 盘口分钟级快照记录（每60秒持久化一次）
	struct OrderBookSnapshotRecord
	{
		time_t snapshotTime{ 0 };      // 快照时间戳
		std::vector<OrderBookLevelSnapshot> levels;  // 有效档位（价格>0，卖5+买5最多10条）
	};

	// 内外盘2秒采样数据（增量净比+增量成交量）
	struct VolumeSample {
		double netRatio;      // 增量净比 (Δ外-Δ内)/(Δ外+Δ内)*100
		Volume deltaVol;      // 增量成交量 (Δ外+Δ内)
		VolumeSample() : netRatio(0), deltaVol(0) {}
		VolumeSample(double r, Volume v) : netRatio(r), deltaVol(v) {}
	};

	struct VolumePool {
		std::map<time_t, VolumeSample> samples;  // 按时间戳索引的采样数据
		int capacity{ 0 };   // 最大保留条数
		void Init(int cap)
		{
			capacity = cap;
			samples.clear();
		}
		void Clear()
		{
			samples.clear();
		}
		// 添加采样数据（时间戳去重）
		void Push(time_t t, const VolumeSample& sample)
		{
			samples[t] = sample;
			// 超出容量时删除最旧的
			while (static_cast<int>(samples.size()) > capacity)
				samples.erase(samples.begin());
		}
		// 获取最新样本
		bool Newest(VolumeSample& out) const
		{
			if (samples.empty()) return false;
			auto it = samples.rbegin();
			out = it->second;
			return true;
		}
		// 获取倒数第N个样本（0=最新，1=次新，...）
		bool FromEnd(int n, VolumeSample& out) const
		{
			if (n < 0 || n >= static_cast<int>(samples.size())) return false;
			auto it = samples.rbegin();
			std::advance(it, n);
			out = it->second;
			return true;
		}
		// 获取倒数第N个的时间戳
		bool FromEndTime(int n, time_t& out) const
		{
			if (n < 0 || n >= static_cast<int>(samples.size())) return false;
			auto it = samples.rbegin();
			std::advance(it, n);
			out = it->first;
			return true;
		}
		// 获取指定时间范围内的加权平均净比和总增量成交量
		bool GetRangeAvg(time_t fromTime, double& avgRatio, Volume& totalDeltaVol) const
		{
			avgRatio = 0;
			totalDeltaVol = 0;
			auto it = samples.lower_bound(fromTime);
			Volume weightedSum = 0;
			for (; it != samples.end(); ++it)
			{
				Volume dv = it->second.deltaVol;
				if (dv > 0)
				{
					weightedSum += static_cast<Volume>(it->second.netRatio * dv);
					totalDeltaVol += dv;
				}
			}
			if (totalDeltaVol <= 0) return false;
			avgRatio = static_cast<double>(weightedSum) / totalDeltaVol;
			return true;
		}
	};

	// 股票数据结构
	class StockData
	{
	public:
		StockInfo info;
		ChipDistribution chipDistribution;
		CallAuctionData callAuctionData;  // 集合竞价数据

		// 盘口数据模型（数据与视图分离：行情推送时Update，渲染层只读）
		MarketOrderBook orderBook;

		// 买一到买五、卖一到卖五按价格跟踪挂单瞬时变化量（股）
		std::map<Price, OrderPriceAccum> orderPriceAccumMap;

		// 盘口累计成交量已改为从数据库 tick_trade 聚合读取，不再在 StockData 中维护

		// 使用智能指针管理历史数据
		std::map<Period, std::shared_ptr<HistoricalDataBase>> historicalData;

		template <typename T>
		std::shared_ptr<T> MakesureHistoricalData(Period period)
		{
			auto it = historicalData.find(period);
			if (it != historicalData.end())
			{
				auto _data = std::dynamic_pointer_cast<T>(it->second);
				if (_data)
				{
					return _data;
				}
			}
			auto _data = std::make_shared<T>();
			historicalData[period] = _data;
			return _data;
		}

		RegResult CalTimeLineTrend();

		void clearTimelinePoint()
		{
			auto timelineData = MakesureHistoricalData<TimelineData>(Period::TIMELINE);
			timelineData->Clear();
		}

		// 添加分时数据点
		void addTimelinePoint(const TimelinePoint& point)
		{
			auto timelineData = MakesureHistoricalData<TimelineData>(Period::TIMELINE);
			timelineData->data.push_back(point);
		}

		void addTimelinePoint(const CString& json_data);

		// 将JSON解析的分时数据点追加到外部向量（不修改内部数据）
		void addTimelinePointTo(const CString& json_data, std::vector<TimelinePoint>& outPoints);

		// 获取分时走势数据
		STOCK::TimelineData* getTimelineData()
		{
			return MakesureHistoricalData<TimelineData>(Period::TIMELINE).get();
		}

		void clearKLineData()
		{
			auto klineData = MakesureHistoricalData<KLineData>(Period::DAY);
			klineData->Clear();
		}

		void addKLinePoint(const KLinePoint& point)
		{
			auto klineData = MakesureHistoricalData<KLineData>(Period::DAY);
			klineData->data.push_back(point);
		}

		void addKLineData(const CString& json_data);

		// 获取日K线数据
		STOCK::KLineData* getKLineData()
		{
			return MakesureHistoricalData<KLineData>(Period::DAY).get();
		}

		void clearMin5KLineData()
		{
			auto klineData = MakesureHistoricalData<Min5KLineData>(Period::MIN5);
			klineData->Clear();
		}

		void addMin5KLinePoint(const KLinePoint& point)
		{
			auto klineData = MakesureHistoricalData<Min5KLineData>(Period::MIN5);
			klineData->data.push_back(point);
		}

		void addMin5KLineData(const CString& json_data);

		// 获取5分钟K线数据
		STOCK::Min5KLineData* getMin5KLineData()
		{
			return MakesureHistoricalData<Min5KLineData>(Period::MIN5).get();
		}

		void clearMin30KLineData()
		{
			auto klineData = MakesureHistoricalData<Min30KLineData>(Period::MIN30);
			klineData->Clear();
		}

		void addMin30KLinePoint(const KLinePoint& point)
		{
			auto klineData = MakesureHistoricalData<Min30KLineData>(Period::MIN30);
			klineData->data.push_back(point);
		}

		void addMin30KLineData(const CString& json_data);

		// 获取30分钟K线数据
		STOCK::Min30KLineData* getMin30KLineData()
		{
			return MakesureHistoricalData<Min30KLineData>(Period::MIN30).get();
		}

		// 买一/卖一等于现价的累计计时（持久保存，窗口关闭不清零）
		int ask1EqualSec{ 0 };       // 卖一等于现价的累计秒数
		int bid1EqualSec{ 0 };       // 买一等于现价的累计秒数
		bool isAsk1EqualCurrent{ false };  // 卖一当前是否等于现价
		bool isBid1EqualCurrent{ false };  // 买一当前是否等于现价
		Price lastAsk1Price{ 0 };    // 上次卖一价格（变化时清零）
		Price lastBid1Price{ 0 };    // 上次买一价格（变化时清零）
		DWORD lastTickTime{ 0 };     // 上次计时的时间戳

		VolumePool secVolumePool;  // 秒级缓存池（30条=1分钟2秒采样）
		VolumePool minVolumePool;  // 分钟缓存池（30条=30分钟1分采样）
		time_t lastSampleTime{ 0 }; // 上次采样时间（2秒间隔去重）
		time_t lastSaveTime{ 0 };   // 上次持久化时间（5秒间隔）
		time_t lastMinSampleTime{ 0 }; // 上次分钟级采样时间（1分钟间隔）
		time_t lastOrderBookSaveTime{ 0 }; // 上次盘口快照持久化时间（1分钟间隔）
		Volume lastInnerVolume{ 0 };  // 上次采样的内盘累计值（用于算增量）
		Volume lastOuterVolume{ 0 };  // 上次采样的外盘累计值（用于算增量）

		void InitVolumePools()
		{
			secVolumePool.Init(30);
			minVolumePool.Init(30);
			lastSampleTime = 0;
			lastSaveTime = 0;
			lastMinSampleTime = 0;
			lastInnerVolume = 0;
			lastOuterVolume = 0;
		}
		void ClearVolumePools()
		{
			secVolumePool.Clear();
			minVolumePool.Clear();
			lastSampleTime = 0;
			lastSaveTime = 0;
			lastMinSampleTime = 0;
			lastInnerVolume = 0;
			lastOuterVolume = 0;
		}
		// 添加2秒采样数据，返回是否入池
		bool AddVolumeSample(time_t t, Volume inner, Volume outer)
		{
			// 2秒间隔去重
			time_t sampleTime = t - t % 2;
			if (sampleTime <= 0 || sampleTime == lastSampleTime)
				return false;

			// 计算增量
			Volume deltaInner = inner - lastInnerVolume;
			Volume deltaOuter = outer - lastOuterVolume;
			Volume deltaTotal = deltaInner + deltaOuter;

			double netRatio = 0;
			if (deltaTotal > 0)
				netRatio = static_cast<double>(deltaOuter - deltaInner) / deltaTotal * 100;

			// 首次采样不存数据（没有增量基准），只记录基准值
			if (lastSampleTime == 0)
			{
				lastSampleTime = sampleTime;
				lastInnerVolume = inner;
				lastOuterVolume = outer;
				return false;
			}

			lastSampleTime = sampleTime;
			lastInnerVolume = inner;
			lastOuterVolume = outer;

			secVolumePool.Push(sampleTime, VolumeSample(netRatio, deltaTotal));

			// 分钟级聚合：每1分钟将secVolumePool中该分钟的数据聚合后入minVolumePool
			time_t minTime = sampleTime - sampleTime % 60;  // 对齐到分钟
			if (minTime > 0 && minTime != lastMinSampleTime)
			{
				lastMinSampleTime = minTime;
				// 聚合上一分钟的数据（minTime-60 ~ minTime）
				time_t prevMinStart = minTime - 60;
				double avgRatio = 0;
				Volume totalDeltaVol = 0;
				if (secVolumePool.GetRangeAvg(prevMinStart, avgRatio, totalDeltaVol))
				{
					minVolumePool.Push(minTime, VolumeSample(avgRatio, totalDeltaVol));
				}
			}

			return true;
		}
		// 判断是否需要持久化（10秒间隔）
		bool ShouldSaveSnapshot(time_t t)
		{
			time_t saveTime = t - t % 10;
			if (saveTime <= 0 || saveTime == lastSaveTime)
				return false;
			lastSaveTime = saveTime;
			return true;
		}

		// 更新内外盘采样并持久化（与数据来源解耦，任何数据源更新后都应调用）
		void UpdateVolumeSample();
		void UpdateOrderPriceAccum();  // 更新五档挂单变化量（+N/-N）
		// 每分钟持久化一次盘口快照（五档挂单+累计主动买卖量），内部1分钟去重
		void SaveOrderBookSnapshot();

		// 从secVolumePool获取加权平均净比和净差（minutes: 1/5/10/20）
		// 不足目标条数时有多少根就计算多少根
		bool GetSecNetDiff(int minutes, Volume& diff, double& ratio) const
		{
			if (secVolumePool.samples.empty())
				return false;

			time_t newestTime;
			if (!secVolumePool.FromEndTime(0, newestTime))
				return false;
			time_t fromTime = newestTime - minutes * 60;

			double avgRatio = 0;
			Volume totalDeltaVol = 0;
			if (!secVolumePool.GetRangeAvg(fromTime, avgRatio, totalDeltaVol))
				return false;

			ratio = avgRatio;
			diff = static_cast<Volume>(totalDeltaVol * avgRatio / 100);
			return true;
		}

		// 从minVolumePool获取加权平均净比和净差（minutes: 5/30）
		// 不足目标条数时有多少根就计算多少根
		bool GetInnerOuterNetDiff(int minutes, Volume& diff, double& ratio) const
		{
			if (minVolumePool.samples.empty())
				return false;

			// 获取最新时间戳，往前推 minutes*60 秒
			time_t newestTime;
			if (!minVolumePool.FromEndTime(0, newestTime))
				return false;
			time_t fromTime = newestTime - minutes * 60;

			double avgRatio = 0;
			Volume totalDeltaVol = 0;
			if (!minVolumePool.GetRangeAvg(fromTime, avgRatio, totalDeltaVol))
				return false;

			ratio = avgRatio;
			// 净差 = 总增量成交量 * 加权平均净比 / 100
			diff = static_cast<Volume>(totalDeltaVol * avgRatio / 100);
			return true;
		}

		// 获取前一次加权平均净比（去掉最新一条采样）
		bool GetPreviousInnerOuterNetDiff(int minutes, Volume& diff, double& ratio) const
		{
			if (minVolumePool.samples.size() < 2)
				return false;

			// 获取倒数第2条的时间戳，往前推 minutes*60 秒
			time_t prevTime;
			if (!minVolumePool.FromEndTime(1, prevTime))
				return false;
			time_t fromTime = prevTime - minutes * 60;

			double avgRatio = 0;
			Volume totalDeltaVol = 0;
			// 取 fromTime ~ prevTime 范围（不含最新一条）
			auto itEnd = minVolumePool.samples.upper_bound(prevTime);
			auto itStart = minVolumePool.samples.lower_bound(fromTime);
			Volume weightedSum = 0;
			for (auto it = itStart; it != itEnd; ++it)
			{
				Volume dv = it->second.deltaVol;
				if (dv > 0)
				{
					weightedSum += static_cast<Volume>(it->second.netRatio * dv);
					totalDeltaVol += dv;
				}
			}
			if (totalDeltaVol <= 0) return false;
			ratio = static_cast<double>(weightedSum) / totalDeltaVol;
			diff = static_cast<Volume>(totalDeltaVol * ratio / 100);
			return true;
		}

		bool GetPreviousInnerOuterTotalRatio(double& ratio) const
		{
			VolumeSample prev;
			if (!secVolumePool.FromEnd(1, prev))
				return false;
			ratio = prev.netRatio;
			return prev.deltaVol > 0;
		}

		// 获取最近N条增量净比（从新到旧排列，index 0=最新, N-1=最旧）
		bool GetRecentNetRatios(int count, std::vector<double>& ratios) const
		{
			ratios.clear();
			if (static_cast<int>(secVolumePool.samples.size()) < count)
				return false;
			ratios.resize(count);
			auto it = secVolumePool.samples.rbegin();
			for (int i = 0; i < count; i++, ++it)
			{
				ratios[i] = it->second.netRatio;
			}
			return true;
		}

		// 持仓盈亏计算（需要外部传入成本价和持股数）
		struct PositionInfo {
			double totalCost;        // 总成本
			double marketValue;      // 市值
			double profitLoss;       // 盈亏额
			double profitLossPercent;// 盈亏比
			double todayProfitLoss;  // 当日盈亏额
			double todayProfitLossPercent; // 当日盈亏比
			bool isValid;

			PositionInfo() : totalCost(0), marketValue(0), profitLoss(0),
				profitLossPercent(0), todayProfitLoss(0), todayProfitLossPercent(0),
				isValid(false) {
			}
		};
		PositionInfo CalculatePositionInfo(double costPrice, double holdingCount) const;

		// 年化收益率计算
		double CalculateAnnualizedReturn(double costPrice, double holdingCount, const std::wstring& buyDate) const;

		// ========== 多周期MACD分层缓存 ==========
		// 每个周期独立缓存MACD序列+价格序列+计算时间戳
		// 计算频率：日K每天1次 / 30分10分钟1次 / 5分1分钟1次 / 1分30秒1次
		struct MacdPoint {
			double dif{ 0 };
			double dea{ 0 };
			double bar{ 0 };          // BAR = DIF - DEA
			bool isAboveZero{ false }; // DIF > 0
		};
		struct PeriodMacdCache {
			std::vector<MacdPoint> macdData;  // MACD序列
			std::vector<double> priceSeq;     // 收盘价序列
			time_t calcTime{ 0 };             // 上次MACD计算时间戳
			bool dataReady{ false };          // 数据是否充足（>=26根）

			void Clear() {
				macdData.clear();
				priceSeq.clear();
				calcTime = 0;
				dataReady = false;
			}
		};
		PeriodMacdCache dayMacdCache;     // 日K MACD缓存
		PeriodMacdCache m30MacdCache;     // 30分钟 MACD缓存
		PeriodMacdCache m5MacdCache;      // 5分钟 MACD缓存
		PeriodMacdCache m1MacdCache;      // 1分钟 MACD缓存

		// 趋势判定结果缓存（每30秒判定一次，使用上述MACD缓存数据）
		CString macdTrendSignal;      // 信号标签：如"正T"/"反T"/"持有"/"观望"
		CString macdTrendDesc;        // 逻辑说明
		time_t macdTrendJudgeTime{ 0 };   // 上次趋势判定时间戳
		static constexpr int MACD_TREND_JUDGE_INTERVAL = 30; // 趋势判定间隔（秒）

		// 各周期MACD计算间隔
		static constexpr int DAY_MACD_INTERVAL = 86400;    // 日K：每天1次
		static constexpr int M30_MACD_INTERVAL = 600;      // 30分钟：10分钟1次
		static constexpr int M5_MACD_INTERVAL = 60;        // 5分钟：1分钟1次
		static constexpr int M1_MACD_INTERVAL = 30;        // 1分钟：30秒1次
	};

	// 股票市场类，管理多个股票
	class StockMarket
	{
	private:
		std::map<std::wstring, std::shared_ptr<StockData>> stocks; // 以股票代码为键的股票数据映射

	public:
		void LoadRealtimeDataByJson(std::string data, const std::vector<std::wstring>& codes = {});
		void LoadTimelineDataByJson(std::wstring stock_id, CString* data);
		void LoadKLineDataByJson(std::wstring stock_id, CString* data);
		void LoadMin5KLineDataByJson(std::wstring stock_id, CString* data);
		void LoadMin30KLineDataByJson(std::wstring stock_id, CString* data);
		void LoadInnerOuterData(std::string data);
		void LoadFundIOPVData(const std::wstring& key, const CString& data);
		void LoadCallAuctionData(std::string data);

		void ClearRealtimeData(const std::vector<std::wstring>& onlyCodes = {})
		{
			for (const auto& it : stocks)
			{
				// 如果指定了只清空的代码列表，则跳过不在列表中的股票
				if (!onlyCodes.empty())
				{
					bool found = false;
					for (const auto& code : onlyCodes)
					{
						if (it.first == code)
						{
							found = true;
							break;
						}
					}
					if (!found) continue;
				}

				StockInfo data;
				data.innerVolume = it.second->info.innerVolume;
				data.outerVolume = it.second->info.outerVolume;
				data.turnoverRate = it.second->info.turnoverRate;
				data.iopv = it.second->info.iopv;
				data.iopvPrevClose = it.second->info.iopvPrevClose;
				data.iopvPremiumRate = it.second->info.iopvPremiumRate;
				it.second->info = data;
			}
		}

		// 添加股票
		std::shared_ptr<StockData> addStock(const std::wstring& code)
		{
			StockData stock;
			stock.info.code = code;
			stock.InitVolumePools();
			stocks[code] = std::make_shared<StockData>(stock);
			return stocks[code];
		}

		// 获取股票数据
		std::shared_ptr<StockData> getStock(const std::wstring& code)
		{
			auto it = stocks.find(code);
			if (it != stocks.end())
			{
				return it->second;
			}
			return addStock(code);
		}
	};

	// 转换函数模板
	template <typename T>
	T convert(const std::string& str)
	{
		if (str.empty()) return T();
		if constexpr (std::is_same_v<T, double>)
		{
			return std::stod(str);
		}
		else if constexpr (std::is_same_v<T, long long>)
		{
			return std::stoll(str);
		}
		return T();
	}
}
