#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <iostream>
#include <limits>
#include <ctime>

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
		Volume volume;         // 成交量
		Price price;           // 价格
		Price averagePrice;    // 均价（全天累计）
		Amount amount;         // 成交额（price * volume）
		Price ma5;             // 5分钟滚动均价
		Price ma10;            // 10分钟滚动均价
		Price ma20;            // 20分钟滚动均价

		TimelinePoint() : volume(0), price(0), averagePrice(0), amount(0), ma5(0), ma10(0), ma20(0) {}
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
		// 获取最近N天的最高价
		double GetMaxPrice(int days) const;
		// 获取最近N天的最低价
		double GetMinPrice(int days) const;
		// 获取最近N天的平均收盘价
		double GetAverageClosePrice(int days) const;

		// 周期高低点统计
		struct PeriodStats {
			double maxPrice;
			double minPrice;
			std::string maxDate;
			std::string minDate;
			double avgPrice;
			bool isValid;

			PeriodStats() : maxPrice(0), minPrice(0), avgPrice(0), isValid(false) {}
		};
		PeriodStats GetPeriodStats(int days) const;
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

		std::wstring displayPrice = L" --";
		std::wstring displayFluctuationDiff = L"+--";  // 涨跌额
		std::wstring displayFluctuation = L"--% ";      // 涨跌幅百分比

		Price priceLimit; // 价格限制

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
			priceLimit(0.0)
		{
			// bidLevels.resize(MAX_LEVEL);
			// askLevels.resize(MAX_LEVEL);
		}

		void Load(std::wstring key, std::vector<std::string> data);
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
	};

	// 股票数据结构
	class StockData
	{
	public:
		StockInfo info;

		// 买一/卖一累计停留时间（秒）
		int ask1AccumSeconds{ 0 };    // 现价等于卖一价的累计秒数
		int bid1AccumSeconds{ 0 };    // 现价等于买一价的累计秒数
		Price prevAsk1Price{ 0 };     // 上一次卖一价（用于检测变化后清零）
		Price prevBid1Price{ 0 };     // 上一次买一价（用于检测变化后清零）
		time_t lastAccumUpdateTime{ 0 };  // 上次累计时间更新的时间戳

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

		std::wstring GetCurrentDisplay(bool include_name) const;

		// 买一/卖一等于现价的累计计时（持久保存，窗口关闭不清零）
		int ask1EqualSec{ 0 };       // 卖一等于现价的累计秒数
		int bid1EqualSec{ 0 };       // 买一等于现价的累计秒数
		bool isAsk1EqualCurrent{ false };  // 卖一当前是否等于现价
		bool isBid1EqualCurrent{ false };  // 买一当前是否等于现价
		Price lastAsk1Price{ 0 };    // 上次卖一价格（变化时清零）
		Price lastBid1Price{ 0 };    // 上次买一价格（变化时清零）
		DWORD lastTickTime{ 0 };     // 上次计时的时间戳

		// 内外盘历史快照（按时间保存，用于计算1分钟/5分钟内外盘差值）
		struct VolumeSnapshot {
			time_t timestamp;
			Volume innerVolume;
			Volume outerVolume;
		};
		std::vector<VolumeSnapshot> volumeSnapshots;  // 按时间排序的快照
		void addVolumeSnapshot(time_t t, Volume inner, Volume outer)
		{
			volumeSnapshots.push_back({ t, inner, outer });
			// 只保留最近10分钟的数据
			time_t cutoff = t - 600;
			while (!volumeSnapshots.empty() && volumeSnapshots.front().timestamp < cutoff)
				volumeSnapshots.erase(volumeSnapshots.begin());
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
	};

	// 股票市场类，管理多个股票
	class StockMarket
	{
	private:
		std::map<std::wstring, std::shared_ptr<StockData>> stocks; // 以股票代码为键的股票数据映射

	public:
		void LoadRealtimeDataByJson(std::string data);
		void LoadTimelineDataByJson(std::wstring stock_id, CString* data);
		void LoadKLineDataByJson(std::wstring stock_id, CString* data);
		void LoadMin5KLineDataByJson(std::wstring stock_id, CString* data);
		void LoadMin30KLineDataByJson(std::wstring stock_id, CString* data);
		void LoadInnerOuterData(std::string data);

		void ClearRealtimeData()
		{
			for (const auto& it : stocks)
			{
				StockInfo data;
				it.second->info = data;
			}
		}

		// 添加股票
		std::shared_ptr<StockData> addStock(const std::wstring& code)
		{
			StockData stock;
			stock.info.code = code;
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
