#pragma once

#include "StockDef.h"
#include "SignalAnalyzer.h"
#include "DataManager.h"
#include <string>
#include <vector>

// ============================================================================
// CStockIndicator: 股票指标兼容层（纯计算，无UI依赖）
// ----------------------------------------------------------------------------
// 指标计算逻辑已统一迁移到 CSignalAnalyzer，本类仅保留：
//   - 滚动均价（MA5/MA10/MA20，依赖TimelinePoint的amount/volume，非标准指标）
//   - Y轴整齐刻度（Nice Number算法，纯UI辅助）
//   - K线周期高低点统计（UI标记用）
//   - 兼容旧调用的委托函数（内部调用CSignalAnalyzer统一实现）
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
		None,                  // 无信号
		GoldenCross,           // 金叉：DIF从下往上穿过DEA（同方向5根K线内第一个，显示标签）
		DeathCross,            // 死叉：DIF从上往下跌破DEA（同方向5根K线内第一个，显示标签）
		RepeatedGoldenCross,   // 重复金叉：同方向5根K线内后续的金叉，只绘制圆点不显示标签
		RepeatedDeathCross     // 重复死叉：同方向5根K线内后续的死叉，只绘制圆点不显示标签
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

	// ========== 滚动均价计算 ==========

	// 为每个分时数据点计算MA5/MA10/MA20滚动均价（滑动窗口，修改timelinePoint中的ma5/ma10/ma20字段）
	static void CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint);

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
	// shortPeriod/longPeriod/signalPeriod: MACD三参数，默认12/26/9
	static std::vector<MACDData> CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint,
		int shortPeriod = 12, int longPeriod = 26, int signalPeriod = 9);

	// 检测MACD金叉死叉序列（返回每个位置的信号）
	static std::vector<MACDCrossSignal> DetectMACDCross(const std::vector<MACDData>& macdData);

	// 计算DIF趋势（线性回归斜率），用于判断DIF线方向
	// macdData: MACD序列；lookback: 回看窗口长度，默认6
	// 返回 RegResult（slope>0 表示DIF上升趋势，slope<0 表示下降趋势）
	// 当DIF值太小时，内部自动放大为整数计算，不影响原始MACD数据
	static RegResult CalcDIFTrend(const std::vector<MACDData>& macdData, int lookback = 6);

	// 检测KDJ金叉死叉序列（金叉：K上穿D且前一根K<=30，死叉：K下穿D且前一根K>=70）
	static std::vector<MACDCrossSignal> DetectKDJCross(const std::vector<KDJData>& kdjData);

	// ========== KDJ指标计算 ==========

	// 计算K线KDJ序列
	// n: RSV周期, m1: K平滑系数, m2: D平滑系数，默认9,3,3
	static std::vector<KDJData> CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int n = 9, int m1 = 3, int m2 = 3);

	// 计算分时KDJ序列
	// n: RSV周期, m1: K平滑系数, m2: D平滑系数，默认9,3,3
	static std::vector<KDJData> CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int n = 9, int m1 = 3, int m2 = 3);

	// 计算KDJ趋势（基于K值的线性回归斜率），用于判断K线方向
	// kdjData: KDJ序列；lookback: 回看窗口长度，默认6
	// 返回 RegResult（slope>0 表示K值上升趋势，slope<0 表示下降趋势）
	static RegResult CalcKDJTrend(const std::vector<KDJData>& kdjData, int lookback = 6);

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
};
