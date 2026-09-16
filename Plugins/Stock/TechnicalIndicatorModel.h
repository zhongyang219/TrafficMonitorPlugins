#pragma once

#include "StockIndicator.h"
#include <StockDef.h>
#include <vector>

// ============================================================================
// 技术指标数据模型（纯数据层，不含任何绘图代码）
// ----------------------------------------------------------------------------
// 与盘口 MarketOrderBook 同构的 MV 分离：
//   - 数据/指标更新入口：UpdateTimeline(...) / UpdateKLine(...)（K线序列变化时调用）
//   - 缓存派生结果：MACD/KDJ/W&R/RSI各周期参数的指标序列、金叉死叉信号、
//     趋势箭头、BOLL上中下轨、成交量MA5/MA10等
//   - 渲染层只读访问 Get* 接口，不做任何指标计算
//
// 与盘口的差异：盘口由单条Tick驱动（只有一组数据），这里由K线/分时序列驱动，
// 输出与序列等长的指标数组。
// 参数随视图模式（分时/5分钟/30分钟/日K）变化，Update时按 viewMode 指定。
// ============================================================================
class TechnicalIndicatorModel
{
public:
	using MACDData = CStockIndicator::MACDData;
	using MACDCrossSignal = CStockIndicator::MACDCrossSignal;
	using KDJData = CStockIndicator::KDJData;
	using WRData = CStockIndicator::WRData;
	using RSIData = CStockIndicator::RSIData;
	using RegResult = ::RegResult;

	// 分时图指标类型（与CFloatingWnd::TimelineIndicator对应）
	enum class TimelineIndicator { CJL, MACD, KDJ, WR, RSI };

public:
	// ============ 数据更新入口（序列变化时调用一次，内部完成全部指标计算）============

	// 全量更新：timelinePoint为完整分时/转分时序列（所有副图指标基于此序列），
	// klineData为对应K线序列（可空），viewMode决定各指标参数及W&R/RSI的计算版本
	void UpdateAll(const std::vector<STOCK::TimelinePoint>& fullTimeline,
		const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode);

	// 切换股票/切换周期时清空全部缓存
	void Reset();

	// ============ 渲染层只读接口（全部为缓存序列，无计算）============

	// MACD（按当前视图模式的参数计算）
	const std::vector<MACDData>& GetMACD() const { return m_macd; }
	// KDJ
	const std::vector<KDJData>& GetKDJ() const { return m_kdj; }
	// W&R
	const std::vector<WRData>& GetWR() const { return m_wr; }
	// RSI
	const std::vector<RSIData>& GetRSI() const { return m_rsi; }

	// MACD金叉死叉信号序列（与m_macd等长，仅数据变化时重算一次）
	const std::vector<MACDCrossSignal>& GetMACDCross() const { return m_macdCross; }
	// KDJ金叉死叉信号序列
	const std::vector<MACDCrossSignal>& GetKDJCross() const { return m_kdjCross; }

	// 成交量MA序列（与fullTimeline等长，分时模式MIN5下绘制均线用）
	// Returns empty vector when timeline data isn't loaded yet.
	const std::vector<double>& GetVolumeMA5() const { return m_volMa5; }
	const std::vector<double>& GetVolumeMA10() const { return m_volMa10; }

	// BOLL上/中/下轨序列（与fullTimeline等长，0表示未形成/无效）
	const std::vector<double>& GetBollUpper() const { return m_bollUpper; }
	const std::vector<double>& GetBollMid() const { return m_bollMid; }
	const std::vector<double>& GetBollLower() const { return m_bollLower; }

	// DIF上行区间序列（全局索引对 [起始,结束]，区间内DIF持续上行；Update时算一次）
	const std::vector<std::pair<int, int>>& GetDIFUpIntervals() const { return m_difUpIntervals; }

	// 当前视图模式
	UIViewMode GetViewMode() const { return m_viewMode; }
	// 模型是否已有效加载数据
	bool IsValid() const { return m_valid; }

	// ============ 标题栏需要的小数据（缓存单值，不再由渲染层查序列）============

	// 最新有效MACD指标（缓存 DIF/DEA 值）；hasValue=False表示无数据
	bool GetLastMACD(double& dif, double& dea) const;
	// 最新有效KDJ指标
	bool GetLastKDJ(double& k, double& d, double& j) const;
	// 最新有效W&R指标
	bool GetLastWR(double& wr1, double& wr2) const;
	// 最新有效RSI指标
	bool GetLastRSI(double& rsi1, double& rsi2) const;

	// DIF趋势箭头（在序列末尾缓存好）：slope 强弱 + 方向，返回有效性与箭头数量
	// isUp: 1=上升, -1=下降, 0=无效
	bool GetDIFTrend(int index, int& arrowCount, int& isUp, COLORREF& arrowColorRef_unused) const;

private:
	// 按视图模式选择MACD参数（与原IndicatorChart中的取参逻辑一致）
	void GetMACDParams(UIViewMode viewMode, int& shortP, int& longP, int& signalP) const;
	void GetKDJParams(UIViewMode viewMode, int& n, int& m1, int& m2) const;

	// 各指标的计算与缓存（UpdateAll按职责拆分的私有步骤）
	void UpdateMACD(const std::vector<STOCK::TimelinePoint>& fullTimeline, UIViewMode viewMode);
	void UpdateKDJ(const std::vector<STOCK::TimelinePoint>& fullTimeline, UIViewMode viewMode);
	void UpdateWR(const std::vector<STOCK::TimelinePoint>& fullTimeline,
		const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode);
	void UpdateRSI(const std::vector<STOCK::TimelinePoint>& fullTimeline,
		const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode);
	void UpdateVolumeMA(const std::vector<STOCK::TimelinePoint>& fullTimeline);
	void UpdateBOLL(const std::vector<STOCK::TimelinePoint>& fullTimeline);
	void UpdateDIFTrend(UIViewMode viewMode);
	void UpdateDIFUpIntervals();

private:
	bool m_valid{ false };
	UIViewMode m_viewMode{ UI_VIEW_TIMELINE };

	// ===== 指标缓存序列（与fullTimeline等长）=====
	std::vector<MACDData> m_macd;
	std::vector<MACDCrossSignal> m_macdCross;
	std::vector<KDJData> m_kdj;
	std::vector<MACDCrossSignal> m_kdjCross;
	std::vector<WRData> m_wr;
	std::vector<RSIData> m_rsi;

	// 成交量均线
	std::vector<double> m_volMa5;
	std::vector<double> m_volMa10;

	// BOLL轨道
	std::vector<double> m_bollUpper;
	std::vector<double> m_bollMid;
	std::vector<double> m_bollLower;

	// 标题栏缓存（最新指标值，Update时一次性提取）
	double m_lastDif{ 0 }, m_lastDea{ 0 };
	bool m_lastMacdValid{ false };
	double m_lastK{ 0 }, m_lastD{ 0 }, m_lastJ{ 0 };
	bool m_lastKdjValid{ false };
	double m_lastWr1{ 0 }, m_lastWr2{ 0 };
	bool m_lastWrValid{ false };
	double m_lastRsi1{ 0 }, m_lastRsi2{ 0 };
	bool m_lastRsiValid{ false };

	// DIF尾部趋势（缓存，避免每帧重算线性回归）
	int m_difTrendArrows{ 0 };
	int m_difTrendDir{ 0 };   // 1=升, -1=降, 0=无
	COLORREF m_difTrendColor{ CLR_INVALID };

	// DIF上行区间缓存（MVC：区间检测计算放模型，渲染层只画背景）
	std::vector<std::pair<int, int>> m_difUpIntervals;
};
