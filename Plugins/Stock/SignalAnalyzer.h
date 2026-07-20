#pragma once
#include "StockDef.h"

class CSignalAnalyzer
{
public:
	// ========== 全局指标参数配置 ==========
	// 统一管理所有指标周期、平滑模式等，替代硬编码常量
	struct SignalParam
	{
		int bollPeriod;      // BOLL周期（默认20）
		int kdjPeriod;       // KDJ周期（默认9）
		int rsiPeriod;       // RSI周期（默认6，5分钟用）
		int rsiPeriod30m;    // RSI周期（默认14，30分钟用）
		int atrPeriod;       // ATR周期（默认14）
		int wrPeriod;        // WR周期（默认6）
		bool rsiUseEma;      // RSI平滑模式：true=EMA（默认），false=SMA

		SignalParam()
			: bollPeriod(20), kdjPeriod(9), rsiPeriod(6), rsiPeriod30m(14)
			, atrPeriod(14), wrPeriod(6), rsiUseEma(true) {}
	};

	// 获取/设置全局参数
	static SignalParam& GetParam();
	static void SetParam(const SignalParam& param);

	// T+0日内买卖信号
	struct T0Signal {
		bool valid;         // 是否有有效信号
		bool isBuy;         // true=买点, false=卖点
		int strength;       // 信号强度 1-3
		CString reason;     // 信号原因描述
		double price;       // 信号价格
		CString time;       // 信号时间
		STOCK::TrendState30m trendState; // 30分钟趋势状态
		bool isForbid;      // 风控禁止交易标志（兼容旧字段）
		CString forbidReason; // 风控禁止原因

		T0Signal() : valid(false), isBuy(false), strength(0), price(0),
			trendState(STOCK::TrendState30m::STATE_SHAKE), isForbid(false) {
		}
	};

	// 风控结果：区分禁止买入/禁止卖出双通道
	struct ForbidResult {
		bool forbidBuy;         // 禁止买入
		bool forbidSell;        // 禁止卖出
		CString forbidBuyReason;  // 禁止买入原因（4字简短描述）
		CString forbidSellReason; // 禁止卖出原因（4字简短描述）

		ForbidResult() : forbidBuy(false), forbidSell(false) {}
		bool HasAnyForbid() const { return forbidBuy || forbidSell; }
	};

	// 批量信号点
	struct SmartSignalPoint {
		int barIndex;               // bars5中的索引
		bool isBuy;                 // true=买, false=卖
		bool isForbid;              // 风控禁止
		STOCK::TrendState30m trendState; // 30分钟趋势
		CString reason;             // 信号原因
	};

	// 所有函数都是 static，纯计算无状态

	// ========== 基础工具函数 ==========
	static double CalcMA(const std::vector<double>& values, int N);
	static double CalcEMA(const std::vector<double>& values, int N);
	static void CalcMACDSeries(const std::vector<STOCK::Bar>& bars, std::vector<double>& difSeq, std::vector<double>& deaSeq, std::vector<double>& barSeq);
	static double CalcSMA(const std::vector<double>& values, int N, double M = 1.0);
	static double CalcStdDev(const std::vector<double>& values);

	// ========== 5个核心指标计算函数 ==========
	static STOCK::BollResult CalcBoll(const std::vector<STOCK::Bar>& bars, int N = 20);
	static STOCK::MACDResult CalcMACD(const std::vector<STOCK::Bar>& bars);
	static STOCK::KDJResult CalcKDJ(const std::vector<STOCK::Bar>& bars, int N = 9);
	static double CalcRSI(const std::vector<STOCK::Bar>& bars, int N);
	static double CalcWR(const std::vector<STOCK::Bar>& bars, int N);
	static double CalcWR_VolumeWeighted(const std::vector<STOCK::Bar>& bars, int N);

	// ========== 30分钟趋势判定 ==========
	// 返回 TrendStateResult：含趋势状态、置信度、加权得分
	static STOCK::TrendStateResult Get30mTrendState(const std::vector<STOCK::Bar>& bars30);
	// 趋势防抖：缓存最近N根K线的趋势状态，连续2根同状态才正式切换
	static STOCK::TrendState30m DebounceTrendState(const std::vector<STOCK::TrendStateResult>& recentResults, STOCK::TrendState30m currentState);

	// ========== 双周期共振趋势判定（5分钟+30分钟K线） ==========
	// 30分钟波段结构判定
	static bool Calc30UpStruct(const std::vector<STOCK::Bar>& bars30);
	static bool Calc30DownStruct(const std::vector<STOCK::Bar>& bars30);
	static bool Calc30SideStruct(const std::vector<STOCK::Bar>& bars30);
	// 5分钟短线多空判定
	static bool Calc5MinUp(const std::vector<STOCK::Bar>& bars5);
	static bool Calc5MinDown(const std::vector<STOCK::Bar>& bars5);
	// 内外盘净比计算
	static double CalcOuterInnerRatio(STOCK::Volume outerVol, STOCK::Volume innerVol);
	// 完整趋势判定主函数
	static STOCK::TrendResult CalcTrend(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30,
		STOCK::Volume outerVol = 0, STOCK::Volume innerVol = 0);

	// ========== 跨周期时间对齐工具 ==========
	// 每根5min Bar匹配对应时间区间的30min Bar索引
	// 返回：vector<size_t>，长度=bars5.size()，每个元素为对应30min Bar在bars30中的索引
	// 若某根5min Bar无法匹配，对应值为SIZE_MAX
	static std::vector<size_t> Align5mTo30m(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30);

	// ========== 回测模式控制 ==========
	// 实盘模式：禁用未来函数逻辑（右侧确认、历史分位等）
	// 回测模式：允许使用完整历史数据
	static void SetBacktestMode(bool enabled);
	static bool IsBacktestMode();

	// ========== 5分钟共振买卖判定 ==========
	static STOCK::Signal5m Get5mSignal(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState);

	// ========== 双通道风控过滤器 ==========
	// 返回 ForbidResult：forbidBuy/forbidSell 分别标记，附带原因描述
	static ForbidResult CalcForbidResult(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState);

	// ========== 完整买卖点判定函数 ==========
	static T0Signal DetectSmartSignal(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30, double lastSignalPrice = 0);

	// ========== 批量信号检测 ==========
	static std::vector<SmartSignalPoint> BatchDetectSignals(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30);

	// ========== 分时图信号检测（1分钟粒度） ==========
	// 将分时TimelinePoint转为1分钟Bar序列（open=high=low=close=price）
	static std::vector<STOCK::Bar> ConvertTimelineToBars(const std::vector<STOCK::TimelinePoint>& timeline);
	// 基于分时1分钟Bar计算信号（复用BatchDetectSignals核心逻辑，使用30分钟K线判断趋势）
	static std::vector<SmartSignalPoint> BatchDetectSignalsFromTimeline(const std::vector<STOCK::Bar>& bars1m, const std::vector<STOCK::Bar>& bars30);

	// ========== 统一信号分析结果（供走势图绘制和弹窗共用） ==========
	struct SignalAnalysisResult {
		// 批量信号（与走势图绘制一致）
		std::vector<SmartSignalPoint> batchSignals;
		// 点击位置对应的批量信号
		const SmartSignalPoint* clickedSignal;  // 可能为nullptr
		// 当前K线指标值（截取到点击位置，无未来函数）
		STOCK::BollResult boll;
		STOCK::MACDResult macd;
		STOCK::MACDResult prevMacd;
		STOCK::KDJResult kdj;
		STOCK::KDJResult prevKdj;
		double rsi;
		double wr;
		double atr;
		// 5分钟信号和风控
		STOCK::Signal5m sig5m;
		ForbidResult forbidResult;
		T0Signal smartSignal;
		STOCK::TrendStateResult trendResult;
		// 卖出条件诊断
		bool sellS1, sellS2_redShrink, sellNecessary;
		bool sellA1, sellA2, sellA3, sellA4;
		int sellAuxCount;
		// 买入条件诊断
		bool buyB1, buyB2_greenShrink, buyNecessary;
		bool buyA1, buyA2, buyA3, buyA4;
		int buyAuxCount;
		// BatchDetectSignals完整判定链诊断
		bool batchHasSell, batchHasBuy;
		bool batchForbidBuy, batchForbidSell;
		bool batchKdjTopPassiveExempt, batchStrongTrendUp;
		bool batchSellFilteredByForbid, batchBuyFilteredByForbid;
		bool batchSellFilteredByGap;  // 信号去重过滤
		CString batchForbidBuyReason, batchForbidSellReason;
		CString batchFilterReason;  // 综合过滤原因

		// 批量计算的指标序列（由AnalyzeSignalAt/AnalyzeSignalAtFromTimeline填充，与BatchDetectSignals完全相同）
		std::vector<double> bollUpSeq, bollMidSeq, bollDnSeq, bollBandSeq;
		std::vector<double> difSeq, deaSeq, macdBarSeq;
		std::vector<double> kSeq, dSeq, jSeq;
		std::vector<double> rsi6Seq, wr6Seq, atrSeq;
		std::vector<STOCK::TrendState30m> trendStateSeq;

		SignalAnalysisResult()
			: clickedSignal(nullptr), rsi(0), wr(0), atr(0)
			, sig5m(STOCK::Signal5m::SIG_NONE)
			, sellS1(false), sellS2_redShrink(false), sellNecessary(false)
			, sellA1(false), sellA2(false), sellA3(false), sellA4(false), sellAuxCount(0)
			, buyB1(false), buyB2_greenShrink(false), buyNecessary(false)
			, buyA1(false), buyA2(false), buyA3(false), buyA4(false), buyAuxCount(0)
			, batchHasSell(false), batchHasBuy(false)
			, batchForbidBuy(false), batchForbidSell(false)
			, batchKdjTopPassiveExempt(false), batchStrongTrendUp(false)
			, batchSellFilteredByForbid(false), batchBuyFilteredByForbid(false)
			, batchSellFilteredByGap(false) {}
	};
	// 统一信号分析：bars5/bars30为完整K线数据，barIndex为点击位置
	// 批量信号用完整数据（逐根递推无未来函数），指标值用截取子序列（无未来函数）
	static SignalAnalysisResult AnalyzeSignalAt(const std::vector<STOCK::Bar>& bars5,
		const std::vector<STOCK::Bar>& bars30, int barIndex);
	// 分时图统一信号分析（1分钟粒度版AnalyzeSignalAt）
	static SignalAnalysisResult AnalyzeSignalAtFromTimeline(const std::vector<STOCK::Bar>& bars1m,
		const std::vector<STOCK::Bar>& bars30, int barIndex);

	// ========== 实时指标信号检测（基于当前价格判断BOLL/MACD/RSI/KDJ/W&R买卖状态） ==========
	// 信号方向：-1=买入信号(↓)，1=卖出信号(↑)，0=无信号
	// 信号强度：1=弱，2=中，3=强
	struct RealtimeSignal {
		int boll;   int bollStr;   // BOLL：触碰上轨=卖出(1)，触碰下轨=买入(-1)
		int macd;   int macdStr;   // MACD：金叉=买入(-1)，死叉=卖出(1)
		int rsi;    int rsiStr;    // RSI：超买(>70)=卖出(1)，超卖(<30)=买入(-1)
		int kdj;    int kdjStr;    // KDJ：超买(K>80)=卖出(1)，超卖(K<20)=买入(-1)
		int wr;     int wrStr;     // W&R：超买(<20)=卖出(1)，超卖(>80)=买入(-1)
		RealtimeSignal() : boll(0), bollStr(0), macd(0), macdStr(0), rsi(0), rsiStr(0), kdj(0), kdjStr(0), wr(0), wrStr(0) {}
	};
	static RealtimeSignal CalcRealtimeSignals(const std::vector<STOCK::Bar>& bars5, int endIndex = -1);
	static RealtimeSignal CalcRealtimeSignalsFromTimeline(const std::vector<STOCK::TimelinePoint>& timeline, int endIndex = -1);

	// ========== 多周期MACD趋势判定（日线→30min→5min→1min联动） ==========

	// 单根K线MACD数据
	struct MacdData {
		double dif;
		double dea;
		double bar;          // BAR = DIF - DEA
		bool isAboveZero;    // 是否在零轴上方（DIF > 0）

		MacdData() : dif(0), dea(0), bar(0), isAboveZero(false) {}
	};

	// 周期类型
	enum class PeriodType {
		DAY,    // 日线
		M30,    // 30分钟
		M5,     // 5分钟
		M1      // 1分钟
	};

	// 一个周期完整序列（用来判断背离、柱子变长变短）
	struct PeriodMacdSeq {
		PeriodType type;
		std::vector<MacdData> data; // 历史K线MACD序列，最新一根在末尾

		// 派生状态，由工具函数计算
		bool isAboveZero;       // 最新DIF是否在零轴上方
		bool isLongTrend;       // 大趋势多头（整体水上+红柱为主）
		bool isShortTrend;      // 大趋势空头（整体水下+绿柱为主）
		bool topDivergence;     // 顶背离
		bool botDivergence;     // 底背离
		bool barGrowingRed;     // 红柱持续变长
		bool barShrinkRed;      // 红柱缩短
		bool barGrowingGreen;   // 绿柱变长
		bool barShrinkGreen;    // 绿柱缩短

		PeriodMacdSeq() : type(PeriodType::DAY), isAboveZero(false)
			, isLongTrend(false), isShortTrend(false)
			, topDivergence(false), botDivergence(false)
			, barGrowingRed(false), barShrinkRed(false)
			, barGrowingGreen(false), barShrinkGreen(false) {}
	};

	// T+0操作信号类型
	enum class TradeSignal {
		BUY_T,     // 低吸正T
		SELL_T,    // 冲高反T卖出
		HOLD,      // 持有观望
		NO_OP      // 禁止操作，观望
	};

	// 最终决策结果
	struct TradeResult {
		TradeSignal sig;
		CString desc; // 逻辑说明

		TradeResult() : sig(TradeSignal::NO_OP) {}
	};

	// 工具函数：判断当前周期整体多空趋势
	static void CalcPeriodTrend(PeriodMacdSeq& seq);
	// 工具函数：判断柱子变化（变长/缩短）
	static void CalcPeriodBarChange(PeriodMacdSeq& seq);
	// 工具函数：简易背离判断（价格创新高但BAR峰值降低=顶背离，价格创新低但绿柱峰值缩小=底背离）
	static void CalcPeriodDivergence(PeriodMacdSeq& seq, const std::vector<double>& priceSeq);
	// 统一刷新单个周期所有状态
	static void RefreshPeriodStatus(PeriodMacdSeq& seq, const std::vector<double>& price);

	// 从K线Bar序列构建PeriodMacdSeq（复用CalcMACDSeries）
	static PeriodMacdSeq BuildPeriodMacdSeq(const std::vector<STOCK::Bar>& bars, PeriodType type);

	// 核心多层周期联动决策函数
	// 严格遵循判断顺序：日线→30分钟→5分钟→1分钟
	static TradeResult JudgeT0Trade(
		PeriodMacdSeq& daySeq,
		PeriodMacdSeq& m30Seq,
		PeriodMacdSeq& m5Seq,
		PeriodMacdSeq& m1Seq,
		const std::vector<double>& dayPrice,
		const std::vector<double>& m30Price,
		const std::vector<double>& m5Price,
		const std::vector<double>& m1Price
	);

	// 便捷函数：从K线数据（日K/30min/5min/分时1min）直接计算多周期MACD趋势信号
	// 任意周期数据不足时返回NO_OP
	static TradeResult CalcMultiPeriodMacdSignal(
		const std::vector<STOCK::Bar>& dayBars,
		const std::vector<STOCK::Bar>& m30Bars,
		const std::vector<STOCK::Bar>& m5Bars,
		const std::vector<STOCK::Bar>& m1Bars
	);
};
