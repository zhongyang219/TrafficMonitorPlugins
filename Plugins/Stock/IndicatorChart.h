#pragma once

#include "ChartContext.h"
#include "Common.h"
#include "StockIndicator.h"
#include <StockDef.h>

// 技术指标图表绘制（MACD/KDJ/WR/RSI）
// 职责：绘制分时/K线模式下的各类技术指标图表，包括标题栏、指标曲线、网格线、时间标签
// 数据来源：CStockIndicator 计算的指标数据，由调用方传入
class CIndicatorChart
{
public:
	// 指标数据类型别名（来自CStockIndicator，便于代码简洁）
	using MACDData = CStockIndicator::MACDData;
	using MACDCrossSignal = CStockIndicator::MACDCrossSignal;
	using KDJData = CStockIndicator::KDJData;
	using WRData = CStockIndicator::WRData;
	using RSIData = CStockIndicator::RSIData;

	// 分时图指标类型（与CFloatingWnd::TimelineIndicator对应）
	enum class TimelineIndicator { CJL, MACD, KDJ, WR, RSI };

	// 分时图指标绘制所需的悬停状态
	struct HoverState {
		bool isHoveringVolume{ false };
		int hoveredBarIndex{ -1 };
		UIViewMode viewMode{ UI_VIEW_MIN5_KLINE };
		CString timelineMacdTitleTip;
		CString timelineKdjTitleTip;
		CString timelineWrTitleTip;
		CString timelineRsiTitleTip;
		CString timelineVolumeTitleTip;
	};

	// ========== 分时图指标绘制 ==========

	// 绘制MACD图表（分时数据版本）
	void DrawMACDChart(CDC& memDC, int x, int y, int width, int height,
		const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<MACDData>& macdData,
		int startIndex = 0, int visibleCount = -1, int xAxisPoints = 0,
		const TimelineDrawContext* ctx = nullptr);

	// 绘制分时KDJ区域
	void DrawTimelineKDJSection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);

	// 绘制分时KDJ图表（纯曲线）
	void DrawTimelineKDJChart(CDC& memDC, int x, int y, int width, int height,
		const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<KDJData>& kdjData,
		int startIndex = 0, int xAxisPoints = 0,
		const TimelineDrawContext* ctx = nullptr);

	// 绘制分时WR图表（纯曲线）
	void DrawTimelineWRChart(CDC& memDC, int x, int y, int width, int height,
		const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<WRData>& wrData,
		int startIndex = 0, int xAxisPoints = 0);

	// 绘制分时WR区域
	void DrawTimelineWRSection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);

	// 绘制分时RSI图表（纯曲线）
	void DrawTimelineRSIChart(CDC& memDC, int x, int y, int width, int height,
		const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const std::vector<RSIData>& rsiData,
		int startIndex = 0, int xAxisPoints = 0);

	// 绘制分时RSI区域
	void DrawTimelineRSISection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);

	// ========== 区域绘制（标题栏+图表一体化） ==========

	// 绘制MACD区域（标题栏+图表+网格，不含时间标签）
	void DrawMacdChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight,
		const CString& macdTitleTip, const HoverState& hover);

	// 绘制指标区域（根据指标类型选择KDJ/WR/RSI/成交量，含标题栏+图表+网格+时间标签）
	void DrawIndicatorChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight,
		bool drawTimeLabels, TimelineIndicator indicator, const HoverState& hover);

	// 绘制成交量区域（标题栏+量柱图+网格+时间标签）
	void DrawVolumeChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight,
		bool drawTimeLabels, const HoverState& hover);

	// 绘制X轴固定时间标签（6等分线位置，位置由ctx.positionY决定；独立于指标区域显示开关）
	void DrawTimeLabels(CDC& memDC, const TimelineDrawContext& ctx);

	// 绘制X轴固定时间标签（labelY由调用方指定；隔天标签useCrossDayColor变蓝，仅5/30分钟K线模式生效）
	void DrawTimeLabelsAt(CDC& memDC, const std::vector<STOCK::TimelinePoint>& timelinePoint,
		int xAxisWidth, int labelY, int xAxisPts = 0);

	// 在DIF线极值点（最高/最低）绘制圆点并标注H/L（红H/绿L）
	void DrawDifExtremeMark(CDC& memDC, int markX, int markY, bool isHigh);

	// 绘制成交量柱状图（纯量柱，不含标题栏和网格）
	void DrawVolumeChart(CDC& memDC, int x, int y, int width, int height,
		const std::vector<STOCK::TimelinePoint>& timelinePoint,
		const STOCK::StockInfo* stockInfo = nullptr,
		int startIndex = 0, int visibleCount = -1, int xAxisPoints = 0);

private:
	// 绘制网格线和时间标签（分时指标区域共用）
	void DrawSectionGridAndTimeLabels(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, bool drawTimeLabels);

	// 绘制金叉死叉箭头标记（从圆点边缘开始，箭头头+竖线杆）
	// markX/markY: 标记中心坐标  dotRadius: 圆点半径  isGolden: 金叉true/死叉false
	void DrawCrossArrow(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden);

	// 绘制金叉死叉箭头（0轴上/下半区变体）
	// markY: 图表上/下边缘基线  arrowAtTop: true=贴顶边向下箭头, false=贴底边向上箭头
	// 颜色由isGolden决定（金叉红/死叉绿），几何方向由arrowAtTop决定
	void DrawCrossArrow2(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden, bool arrowAtTop);
};

// ========== 隔天交替上色公共逻辑（时间标签+竖向虚线共用，实现在IndicatorChart.cpp） ==========
// "yyyy-mm-dd" → 天序号；解析失败返回-1
int DayNumberFromYMD(const std::string& ymd);

// 隔天交替上色的公共上下文：K线模式有效性 + 哪个天数奇偶为标色日
struct DayColorCtx
{
	bool klineMode{ false };   // 5/30分钟K线模式（fullTime含日期）
	bool blueIfOdd{ true };    // true: 天序号为奇数的日期画蓝
};

// 由可视区间末根（最新交易日）日期标定蓝色归属的半天（最新交易日的前一天起标色）
DayColorCtx CalcDayColorCtx(const std::vector<STOCK::TimelinePoint>& timelinePoint);

// 第idx个刻度点（标签/竖线）所在日期是否为蓝色日
bool IsBlueDayAt(const std::vector<STOCK::TimelinePoint>& timelinePoint, int idx, const DayColorCtx& ctx);