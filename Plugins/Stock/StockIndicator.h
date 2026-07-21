#pragma once

#include "StockDef.h"
#include "SignalAnalyzer.h"
#include <string>
#include <vector>

// ============================================================================
// CStockIndicator: 股票指标计算类（纯计算，无UI依赖）
// ----------------------------------------------------------------------------
// 从CFloatingWnd抽取出来的所有股票指标计算逻辑，包括：
//   - 滚动均价（MA5/MA10/MA20）
//   - Y轴整齐刻度（Nice Number算法）
//   - MACD指标（分时/K线）
//   - KDJ指标（分时/K线）
//   - W&R威廉指标（分时/K线）
//   - RSI相对强弱指标（分时/K线）
//   - MACD金叉死叉检测
//   - T+0日内买卖点判定
//   - K线周期高低点统计
// 所有函数都是static，无状态，可在任意线程调用。
// ============================================================================
class CStockIndicator
{
public:
	// ========== 数据结构 ==========

	// MACD指标数据（单根K线/分时点）
	struct MACDData {
		double dif;
		double dea;
		double bar;
		bool valid;
	};

	// MACD金叉死叉信号
	enum class MACDCrossSignal {
		None,       // 无信号
		GoldenCross,// 金叉：DIF从下往上穿过DEA
		DeathCross  // 死叉：DIF从上往下跌破DEA
	};

	// KDJ指标数据
	struct KDJData {
		double k;
		double d;
		double j;
		bool valid;
	};

	// W&R威廉指标数据
	struct WRData {
		double wr1;   // WR6（短期）
		double wr2;   // WR14（长期）
		bool valid;
	};

	// RSI相对强弱指标数据
	struct RSIData {
		double rsi1;   // RSI6（短期）
		double rsi2;   // RSI14（长期）
		bool valid;
	};

	// K线周期高低点信息
	struct PeriodPoint {
		int index;
		STOCK::Price price;
		std::string day;
	};

	// ========== 滚动均价计算 ==========

	// 为每个分时数据点计算MA5/MA10/MA20滚动均价（滑动窗口，修改timelinePoint中的ma5/ma10/ma20字段）
	static void CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint);

	// 计算滚动均价：最近nMinutes分钟的成交额/成交量（单次查询）
	static STOCK::Price CalcRollingAvgPrice(const std::vector<STOCK::TimelinePoint>& timelinePoint, int nMinutes);

	// ========== Y轴整齐刻度计算（Nice Number算法） ==========

	// 计算Y轴整齐刻度步长（1-2-3-4-5-10序列）
	static double CalcNiceStep(double range, double divCount, double minStep = 0.001);

	// 根据数据范围计算Y轴整齐边界（分时/竞价模式：带边距约束和负数保护）
	static void CalcNiceAxisRange(double visMin, double visMax, double divCount, double minStep,
		double& outAxisMin, double& outAxisMax, double& outNiceStep);

	// 根据数据范围计算Y轴整齐边界（K线模式：中心对称扩展）
	static void CalcNiceAxisRangeSymmetric(double visMin, double visMax, double divCount, double minStep,
		double& outMin, double& outMax, double& outNiceStep);

	// ========== MACD指标计算 ==========

	// 计算分时MACD序列（基于TimelinePoint.price）
	static std::vector<MACDData> CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint);

	// 计算K线MACD序列（基于KLinePoint.close）
	static std::vector<MACDData> CalculateKLineMACD(const std::vector<STOCK::KLinePoint>& klineData);

	// 检测MACD金叉死叉序列（返回每个位置的信号）
	static std::vector<MACDCrossSignal> DetectMACDCross(const std::vector<MACDData>& macdData);

	// 获取最近一次MACD金叉死叉信号
	static MACDCrossSignal GetLatestMACDCross(const std::vector<MACDData>& macdData);

	// ========== T+0日内买卖点判定 ==========

	// 检测买点信号（基于分时数据+MACD）
	static CSignalAnalyzer::T0Signal DetectBuySignal(const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<MACDData>& macdData);

	// 检测卖点信号（基于分时数据+MACD）
	static CSignalAnalyzer::T0Signal DetectSellSignal(const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<MACDData>& macdData);

	// ========== KDJ指标计算 ==========

	// 计算K线KDJ序列
	static std::vector<KDJData> CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int period = 9);

	// 计算分时KDJ序列
	static std::vector<KDJData> CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period = 9);

	// ========== W&R威廉指标计算 ==========

	// 计算分时W&R序列
	static std::vector<WRData> CalculateTimelineWR(const std::vector<STOCK::TimelinePoint>& timelinePoint,
		int period1 = 6, int period2 = 14);

	// 计算K线W&R序列
	static std::vector<WRData> CalculateKLineWR(const std::vector<STOCK::KLinePoint>& klineData,
		int period1 = 6, int period2 = 14);

	// ========== RSI相对强弱指标计算 ==========

	// 计算分时RSI序列
	static std::vector<RSIData> CalculateTimelineRSI(const std::vector<STOCK::TimelinePoint>& timelinePoint,
		int period1 = 6, int period2 = 14);

	// 计算K线RSI序列
	static std::vector<RSIData> CalculateKLineRSI(const std::vector<STOCK::KLinePoint>& klineData,
		int period1 = 6, int period2 = 14);

	// ========== K线周期高低点统计 ==========

	// 计算1年/2年/3年的高低点（用于K线图周期标记）
	// klineData: 完整K线数据；startIndex: 起始索引（限制回看范围）
	// periodHighs/periodLows: 输出数组，长度3，分别对应1年/2年/3年
	// useClose: true=使用收盘价，false=使用最高/最低价
	static void CalculatePeriodHighsLows(const std::vector<STOCK::KLinePoint>& klineData, int startIndex,
		PeriodPoint periodHighs[3], PeriodPoint periodLows[3], bool useClose = false);
};
