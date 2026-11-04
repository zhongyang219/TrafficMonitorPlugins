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

	// ===== 标题栏绘制（与区域正文分离，各标题栏只负责自身文字） =====
	// MACD标题栏：DIF/DEA数值（悬停读tip，否则读模型缓存） + DIF趋势箭头
	void DrawMacdTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover);

	// KDJ标题栏：K/D/J数值 + J值趋势箭头
	void DrawKDJTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover);

	// W&R标题栏：WR6/WR14数值
	void DrawWRTitleBar(CDC& memDC, int areaTop, int titleH, const HoverState& hover);

	// RSI标题栏：RSI6/RSI14数值
	void DrawRSITitleBar(CDC& memDC, int areaTop, int titleH, const HoverState& hover);

	// 成交量标题栏：分量/分额 + 5/10分钟量均线（CJL区域与成交量区域共用）
	void DrawVolumeTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover);

	// ===== 成交量区域正文（量柱 + 量均线 + 网格，不含标题栏/时间标签） =====
	void DrawVolumeChartBody(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, const HoverState& hover);

	// 5/10分钟成交量均线叠加（仅5分钟K线模式）
	void DrawVolumeMALines(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, const HoverState& hover);

	// 成交量网格线（含左侧刻度标签与隔天交替竖向虚线）
	void DrawVolumeGrid(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight);

	// 绘制金叉死叉箭头标记（从圆点边缘开始，箭头头+竖线杆）
	// markX/markY: 标记中心坐标  dotRadius: 圆点半径  isGolden: 金叉true/死叉false
	void DrawCrossArrow(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden);

	// 绘制金叉死叉箭头（0轴上/下半区变体）
	// markY: 图表上/下边缘基线  arrowAtTop: true=贴顶边向下箭头, false=贴底边向上箭头
	// 颜色由isGolden决定（金叉红/死叉绿），几何方向由arrowAtTop决定
	void DrawCrossArrow2(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden, bool arrowAtTop);
};

// ========== 隔天交替上色公共逻辑（时间标签+竖向虚线共用，实现在IndicatorChart.cpp） ==========

// 隔天交替上色的公共上下文：K线模式有效性 + 每个数据点所属的交易日序号
struct DayColorCtx
{
	bool klineMode{ false };   // 5/30分钟K线模式（fullTime含日期）
	std::vector<int> dayIdx;   // 逐点交易日序号：基准=可视区间首点日期（0），每切换一个真实交易日+1
	bool flipColor{ false };   // 奇偶翻转标志：保证最新交易日不标色
};

// 以可视区间首点日期为基准统计交易日序号（相邻数据日期不同即+1，假期不占序号）
DayColorCtx CalcDayColorCtx(const std::vector<STOCK::TimelinePoint>& timelinePoint);

// 第idx个点所属交易日是否标色（交易日序号奇偶交替，最新交易日不标色）
bool IsBlueDayAt(const std::vector<STOCK::TimelinePoint>& timelinePoint, int idx, const DayColorCtx& ctx);

// 绘制6等分竖向虚线：X坐标对齐柱中心（与K线/鼠标十字线一致），
// 5/30分钟K线模式下按真实交易日序号奇偶交替黑色虚线，其余用grayPen
void DrawDayColoredVLines(CDC& memDC, const std::vector<STOCK::TimelinePoint>& timelinePoint,
	int chartWidth, int xAxisPoints, int top, int height, CPen& grayPen);