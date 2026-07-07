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
		TimePoint fullTime;    // 完整日期时间（5分/30分K线模式：yyyy-mm-dd hh:mm）
		Volume volume;         // 成交量
		Price price;           // 价格
		Price averagePrice;    // 均价（全天累计）
		Amount amount;         // 成交额（price * volume）
		Price ma5;             // 5分钟滚动均价
		Price ma10;            // 10分钟滚动均价
		Price ma20;            // 20分钟滚动均价
		Price iopv;            // IOPV 基金份额参考净值

		TimelinePoint() : volume(0), price(0), averagePrice(0), amount(0), ma5(0), ma10(0), ma20(0), iopv(0) {}
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
		STATE_WEAK,    // 弱势：仅反T，禁止加仓正T
		STATE_STRONG,  // 强势：回踩可低吸正T
		STATE_SHAKE    // 震荡：正反T均可
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
		Volume accumSellVolume{ 0 };  // 该价格挂盘数量减少累计
		bool isAskSide{ true };       // true=卖方，false=买方
	};

	// 股票数据结构
	class StockData
	{
	public:
		StockInfo info;
		ChipDistribution chipDistribution;
		CallAuctionData callAuctionData;  // 集合竞价数据

		// 买一到买五、卖一到卖五按价格跟踪挂盘减少累计量（股）
		std::map<Price, OrderPriceAccum> orderPriceAccumMap;

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

		std::wstring GetCurrentDisplay(bool include_name) const;

		// 买一/卖一等于现价的累计计时（持久保存，窗口关闭不清零）
		int ask1EqualSec{ 0 };       // 卖一等于现价的累计秒数
		int bid1EqualSec{ 0 };       // 买一等于现价的累计秒数
		bool isAsk1EqualCurrent{ false };  // 卖一当前是否等于现价
		bool isBid1EqualCurrent{ false };  // 买一当前是否等于现价
		Price lastAsk1Price{ 0 };    // 上次卖一价格（变化时清零）
		Price lastBid1Price{ 0 };    // 上次买一价格（变化时清零）
		DWORD lastTickTime{ 0 };     // 上次计时的时间戳

		// 内外盘5秒快照环形缓存池（20分钟=240条）
		// 1分钟取最新12条, 5分钟取最新60条, 10分钟取最新120条, 20分钟取最新240条
		struct VolumeSample {
			time_t timestamp;       // 真实时间戳
			Volume innerVolume;
			Volume outerVolume;
		};

		struct VolumePool {
			std::vector<VolumeSample> samples;
			int head{ 0 };       // 环形缓冲区头指针（最旧数据位置）
			int count{ 0 };      // 当前有效数据量
			int capacity{ 0 };   // 缓冲区容量
			void Init(int cap)
			{
				capacity = cap;
				samples.resize(capacity);
				head = 0;
				count = 0;
			}
			void Clear()
			{
				head = 0;
				count = 0;
			}
			void Push(const VolumeSample& sample)
			{
				if (count < capacity)
				{
					samples[count] = sample;
					count++;
				}
				else
				{
					samples[head] = sample;
					head = (head + 1) % capacity;
				}
			}
			// 获取最新样本
			const VolumeSample* Newest() const
			{
				if (count == 0) return nullptr;
				int idx = (head + count - 1) % capacity;
				return &samples[idx];
			}
			// 获取倒数第N个样本（0=最新，1=次新，...）
			const VolumeSample* FromEnd(int n) const
			{
				if (n < 0 || n >= count) return nullptr;
				int idx = (head + count - 1 - n) % capacity;
				if (idx < 0) idx += capacity;
				return &samples[idx];
			}
		};

		VolumePool volumePool;  // 缓存池（240条=20分钟5秒采样）
		time_t lastSampleTime{ 0 }; // 上次采样时间（5秒间隔去重）

		void InitVolumePools()
		{
			volumePool.Init(240);
			lastSampleTime = 0;
		}
		void ClearVolumePools()
		{
			volumePool.Clear();
			lastSampleTime = 0;
		}
		// 添加5秒采样数据
		bool AddVolumeSample(time_t t, Volume inner, Volume outer)
		{
			// 5秒间隔去重
			time_t sampleTime = t - t % 5;
			if (sampleTime <= 0 || sampleTime == lastSampleTime)
				return false;
			lastSampleTime = sampleTime;

			volumePool.Push({ sampleTime, inner, outer });
			return true;
		}

		// 从缓存池获取净差和净比（minutes: 1/5/10/20）
		// 不足目标条数时有多少根就计算多少根
		bool GetInnerOuterNetDiff(int minutes, Volume& diff, double& ratio) const
		{
			if (volumePool.count < 2)
				return false;

			int sampleCount = min(minutes * 12, volumePool.count);
			const VolumeSample* newest = volumePool.FromEnd(0);
			const VolumeSample* oldest = volumePool.FromEnd(sampleCount - 1);
			if (!newest || !oldest)
				return false;

			Volume inner = newest->innerVolume - oldest->innerVolume;
			Volume outer = newest->outerVolume - oldest->outerVolume;
			if (inner < 0 || outer < 0)
				return false;

			diff = outer - inner;
			Volume total = inner + outer;
			ratio = total > 0 ? static_cast<double>(diff) / total * 100 : 0;
			return total > 0;
		}

		// 获取前一次净差
		bool GetPreviousInnerOuterNetDiff(int minutes, Volume& diff, double& ratio) const
		{
			if (volumePool.count < 3)
				return false;

			int sampleCount = min(minutes * 12, volumePool.count - 1);
			const VolumeSample* prevNewest = volumePool.FromEnd(1);
			const VolumeSample* prevOldest = volumePool.FromEnd(sampleCount);
			if (!prevNewest || !prevOldest)
				return false;

			Volume inner = prevNewest->innerVolume - prevOldest->innerVolume;
			Volume outer = prevNewest->outerVolume - prevOldest->outerVolume;
			if (inner < 0 || outer < 0)
				return false;

			diff = outer - inner;
			Volume total = inner + outer;
			ratio = total > 0 ? static_cast<double>(diff) / total * 100 : 0;
			return total > 0;
		}

		bool GetPreviousInnerOuterTotalRatio(double& ratio) const
		{
			const VolumeSample* prev = volumePool.FromEnd(1);
			if (!prev) return false;
			Volume total = prev->innerVolume + prev->outerVolume;
			if (total <= 0) return false;
			ratio = static_cast<double>(prev->outerVolume - prev->innerVolume) / total * 100;
			return true;
		}

		// 获取最近N根5秒净比（每根=当前样本-前一个样本的差值对应的净比）
		// 返回值从新到旧排列（index 0=最新, N-1=最旧）
		bool GetRecentNetRatios(int count, std::vector<double>& ratios) const
		{
			ratios.clear();
			if (volumePool.count < count + 1)
				return false;
			ratios.resize(count);
			for (int i = 0; i < count; i++)
			{
				// 从新到旧：FromEnd(i+1) 和 FromEnd(i)
				const VolumeSample* older = volumePool.FromEnd(i + 1);
				const VolumeSample* newer = volumePool.FromEnd(i);
				if (!older || !newer)
				{
					ratios.clear();
					return false;
				}
				Volume inner = newer->innerVolume - older->innerVolume;
				Volume outer = newer->outerVolume - older->outerVolume;
				Volume total = inner + outer;
				ratios[i] = total > 0 ? static_cast<double>(outer - inner) / total * 100 : 0;
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
		void LoadFundIOPVData(const std::wstring& key, const CString& data);
		void LoadFundTimelineData(const std::wstring& key, const CString& data);
		void LoadCallAuctionData(std::string data);

		void ClearRealtimeData()
		{
			for (const auto& it : stocks)
			{
				StockInfo data;
				data.innerVolume = it.second->info.innerVolume;
				data.outerVolume = it.second->info.outerVolume;
				data.turnoverRate = it.second->info.turnoverRate;
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
