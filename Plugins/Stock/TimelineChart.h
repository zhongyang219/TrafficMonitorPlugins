#pragma once

#include "ChartContext.h"
#include "Common.h"
#include <StockDef.h>

// 走势图绘制模块
// 职责：绘制分时走势图、5分钟/30分钟/日K价格图、悬停覆盖层、价格区域标题栏等
class CTimelineChart
{
public:
	// 走势图悬停/交互状态
	struct HoverState {
		UIViewMode viewMode{ UI_VIEW_MIN5_KLINE };
		bool isHoveringVolume{ false };
		int hoveredBarIndex{ -1 };
		STOCK::TimelinePoint hoveredData;
		STOCK::Price hoverMa1{ 0 }, hoverMa5{ 0 }, hoverMa10{ 0 }, hoverMa20{ 0 };
		STOCK::Price hoverPrevMa1{ 0 }, hoverPrevMa5{ 0 }, hoverPrevMa10{ 0 }, hoverPrevMa20{ 0 };
		CString hoverTip;
		CString timelinePriceTitleTip;
		CString timelineVolumeTitleTip;
		CString timelineMacdTitleTip;
		CString timelineKdjTitleTip;
		CString timelineWrTitleTip;
		CString timelineRsiTitleTip;
		bool showMA{ false };
		bool showBollBands{ true };
		bool showTrendView{ false };
		bool showChipPeak{ false };
		bool expandedMode{ false };
		int timelineScrollOffset{ -1 };
		int timelineVisibleCount{ 40 };
		int timelineLastTotalPoints{ 0 };
		std::wstring stockId;
		CPoint mousePos;
		// 信号颜色（由DrawPriceChartArea设置，供CFloatingWnd更新按钮文字颜色）
		COLORREF bollSignalColor{ CLR_INVALID };   // BL按钮信号颜色：超买=绿色，超卖=红色
		COLORREF macdSignalColor{ CLR_INVALID };   // MACD按钮信号颜色：金叉=红色，死叉=绿色
		COLORREF kdjSignalColor{ CLR_INVALID };     // KDJ按钮信号颜色
		COLORREF wrSignalColor{ CLR_INVALID };      // W&R按钮信号颜色
		COLORREF rsiSignalColor{ CLR_INVALID };     // RSI按钮信号颜色
		COLORREF maSignalColor{ CLR_INVALID };      // MA按钮信号颜色：金叉=红色，死叉=绿色
	};

	// 分时图绘制
	void DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	void DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx);
	// 在指定区域绘制分时背景高亮（5分钟交替），供价格区/成交量/MACD/KDJ等区域复用
	static void DrawTimelineBackgroundHighlightsForArea(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight);
	void DrawTimelineGridLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	void DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	void DrawTimelinePriceCurve(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	void DrawTimelineHoverOverlay(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);

	// K线模式价格图
	void DrawMin5KLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	void DrawDayKLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);
	// 5/30分钟K线交易日交替底色：主图上不同交易日以 #f8f8f8 / 白色 交替，方便分辨同一天的区间
	void DrawTradingDayBands(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover);

	// 价格区域（标题栏+走势图一体化）
	void DrawPriceChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, HoverState& hover);
};
