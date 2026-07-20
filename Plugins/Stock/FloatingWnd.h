#pragma once

#include <afxwin.h>
#include <string>
#include <vector>
#include <StockDef.h>
#include <TransparentWnd.h>
#include "SignalAnalyzer.h"

// 定义自定义消息
#define FWND_MSG_UPDATE_STATUS (WM_USER + 100)
#define FWND_MSG_SHOW_EDIT_DLG (WM_USER + 102)
#define FWND_MSG_SHOW_ADD_DLG (WM_USER + 103)
#define FWND_MSG_SHOW_TRADE_DLG (WM_USER + 104)

class CFloatingWnd : public CWnd
{
public:
	CFloatingWnd();
	virtual ~CFloatingWnd();

	BOOL Create(CFont* font, CPoint pt, std::wstring stock_id);
	const std::wstring& GetStockId() const { return m_stock_id; }
	void SetStockId(const std::wstring& stockId);
	void ToggleKLineMode(); // 切换分时/日K模式

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnDestroy();
	LRESULT OnUpdateStatus(WPARAM wParam, LPARAM lParam);
	LRESULT OnCloseWindow(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowEditDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowAddDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowTradeDialog(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedTimeLineBtn();
	afx_msg void OnBnClickedKLineBtn();
	afx_msg void OnBnClickedMin5KLineBtn();
	afx_msg void OnBnClickedMin30KLineBtn();
	afx_msg void OnBnClickedJZBtn();
	afx_msg void OnBnClickedCloseBtn();
	afx_msg void OnBnClickedMABtn();
	afx_msg void OnBnClickedBollBtn();
	afx_msg void OnBnClickedZoomOutBtn();
	afx_msg void OnBnClickedZoomInBtn();
	afx_msg void OnBnClickedIndicatorMACDBtn();
	afx_msg void OnBnClickedIndicatorKDJBtn();
	afx_msg void OnBnClickedIndicatorWRBtn();
	afx_msg void OnBnClickedIndicatorRSIBtn();
	afx_msg void OnBnClickedChipPeakBtn();
	afx_msg void OnBnClickedOrderBookBtn();
	afx_msg void OnBnClickedExpandBtn();
	afx_msg void OnBnClickedToggleStockListBtn();
	afx_msg void OnBnClickedCallAuctionBtn();

private:
	void EnsureChipPeakData();
	static void SafeSetWindowPos(CWnd& wnd, int x, int y, int cx, int cy);
	static void SafeShowWindow(CWnd& wnd, bool show);
	static void SafeSetButtonStyle(CButton& btn, UINT style);
	CPoint Stock2Point(int x, int y, int w, int h, double unitY, const STOCK::TimelinePoint& item, const STOCK::Price prevClosePrice);
	void DrawOrderBook(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData);
	void DrawVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const STOCK::StockInfo* stockInfo = nullptr, int startIndex = 0, int visibleCount = -1, int xAxisPoints = 0);

	// 分时图绘制相关
	struct TimelineDrawContext {
		int chartWidth;
		int chartLeft{ 0 };   // 绘图区左侧偏移（用于Y轴标签留白）
		int windowWidth;
		int chartHeight;
		int priceChartTop;
		int priceChartHeight;
		int volumeChartTop;
		int volumeChartHeight;
		int macdChartTop;
		double niceStep{ 0 };  // Y轴刻度步长（OnPaint计算一次，绘制函数直接用）
		int macdChartHeight;
		int positionY;
		int visibleCount{ 0 };   // 可见数据点数（≤120）
		int xAxisPoints{ 0 };   // X轴总格数（=m_timelineVisibleCount，数据不足时右侧留白）
		int startIndex{ 0 };     // 可见数据起始索引
		int scrollRange{ 0 };    // 滚动范围
		STOCK::Price maxPrice{ 0 };   // 可见区间最大价（含内边距）
		STOCK::Price minPrice{ 0 };   // 可见区间最小价（含内边距）
		double unitY{ 0 };            // Y轴缩放（像素/价格）
		STOCK::StockInfo realtimeData;
		const std::vector<STOCK::TimelinePoint>* timelinePoint;    // 可见范围子集（subTimeline）
		const std::vector<STOCK::TimelinePoint>* fullTimeline{ nullptr };  // 完整分时数据（用于布林带等需要历史回溯的指标）
		const std::vector<STOCK::KLinePoint>* klineData;
		// 滚动均价（最新数据点的值）
		STOCK::Price ma1{ 0 };    // MA1（1分钟均价 = 当前价格）
		STOCK::Price ma5{ 0 };    // MA5（5分钟滚动均价）
		STOCK::Price ma10{ 0 };   // MA10（10分钟滚动均价）
		STOCK::Price ma20{ 0 };   // MA20（20分钟滚动均价）
		// 前一分钟数据点的MA值（用于箭头方向判断）
		STOCK::Price prevMa1{ 0 };
		STOCK::Price prevMa5{ 0 };
		STOCK::Price prevMa10{ 0 };
		STOCK::Price prevMa20{ 0 };
	};

	// MACD指标数据
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

	void DrawHeader(CDC& memDC, const STOCK::StockInfo& realtimeData, int windowWidth, int headerHeight, const CString& macdTrendSignal = CString());
	void DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineMAIndicators(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineGridLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelinePriceCurve(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawMin5KLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawDayKLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineVolumeSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineMACDSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineKDJSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineWRSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineRSISection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineTitleBars(CDC& memDC, const TimelineDrawContext& ctx, int priceChartTop, int volumeChartTop, int macdChartTop, int timelineTitleHeight);
	void DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData, int startIndex = 0, int visibleCount = -1, int xAxisPoints = 0);
	void DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData, const std::vector<MACDData>& macdData, int startIndex = 0, int visibleCount = -1);
	std::vector<MACDData> CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint);
	std::vector<MACDData> CalculateKLineMACD(const std::vector<STOCK::KLinePoint>& klineData);
	// 为每个分时数据点计算MA5/MA10/MA20滚动均价（修改timelinePoint中的ma5/ma10/ma20字段）
	static void CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint);
	// 计算滚动均价：N分钟成交额/N分钟成交量（单次查询）
	static STOCK::Price CalcRollingAvgPrice(const std::vector<STOCK::TimelinePoint>& timelinePoint, int nMinutes);
	// 计算Y轴整齐刻度步长（Nice Number算法：1-2-3-4-5-10序列）
	static double CalcNiceStep(double range, double divCount, double minStep = 0.001);
	// 根据数据范围计算Y轴整齐边界（分时/竞价模式：带边距约束和负数保护）
	static void CalcNiceAxisRange(double visMin, double visMax, double divCount, double minStep, double& outAxisMin, double& outAxisMax, double& outNiceStep);
	// 根据数据范围计算Y轴整齐边界（K线模式：中心对称扩展）
	static void CalcNiceAxisRangeSymmetric(double visMin, double visMax, double divCount, double minStep, double& outMin, double& outMax, double& outNiceStep);
	std::vector<MACDCrossSignal> DetectMACDCross(const std::vector<MACDData>& macdData);
	MACDCrossSignal GetLatestMACDCross(const std::vector<MACDData>& macdData);
	CSignalAnalyzer::T0Signal DetectBuySignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData);
	CSignalAnalyzer::T0Signal DetectSellSignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData);
	void DrawTimelinePositionInfo(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineHoverOverlay(CDC& memDC, const TimelineDrawContext& ctx);

	// K线图公共数据结构
	struct KLineDrawData {
		int x, y, w, h;
		int startIndex, finalStartIndex;
		int displayCount, maxVisibleKlines, scrollRange, scrollPos;
		int barWidth;
		int gap;
		STOCK::Price maxPrice, minPrice;
		double unitY;
		const std::vector<STOCK::KLinePoint>* klineData;
		const STOCK::StockInfo* stockInfo;
	};

	struct PeriodPoint {
		int index;
		STOCK::Price price;
		std::string day;
	};

	struct LabelInfo {
		int year;
		int month;
		int barX;
	};

	// K线图绘制公共辅助函数
	KLineDrawData PrepareKLineDrawData(int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData);
	void CalculatePeriodHighsLows(const KLineDrawData& drawData, PeriodPoint periodHighs[3], PeriodPoint periodLows[3], bool useClose = false);
	std::vector<LabelInfo> DrawKLineMonthLines(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLineMonthLabels(CDC& memDC, const KLineDrawData& drawData, const std::vector<LabelInfo>& labelInfos);
	void DrawMin5HourLines(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLineGrid(CDC& memDC, const KLineDrawData& drawData);
	void DrawYearAverageLines(CDC& memDC, const KLineDrawData& drawData);
	void DrawMAIndicators(CDC& memDC, const KLineDrawData& drawData);
	void DrawCurrentPriceLine(CDC& memDC, const KLineDrawData& drawData);
	void DrawPriceLabels(CDC& memDC, const KLineDrawData& drawData);
	void DrawAverageLabels(CDC& memDC, const KLineDrawData& drawData);

	// K线图绘制
	void DrawKLineBars(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLineBuyMarkers(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLinePeriodMarkers(CDC& memDC, const KLineDrawData& drawData, const PeriodPoint periodHighs[3], const PeriodPoint periodLows[3]);
	void DrawKLineChart(CDC& memDC, int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData, const STOCK::StockInfo& stockInfo);

	void DrawKLineVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData);

	// KDJ计算与绘制
	struct KDJData {
		double k;
		double d;
		double j;
		bool valid;
	};
	std::vector<KDJData> CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int period = 9);
	std::vector<KDJData> CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period = 9);
	void DrawKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData);
	void DrawTimelineKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<KDJData>& kdjData, int startIndex = 0, int xAxisPoints = 0);

	// W&R威廉指标计算与绘制
	struct WRData {
		double wr1;   // WR6（短期）
		double wr2;   // WR14（长期）
		bool valid;
	};
	std::vector<WRData> CalculateTimelineWR(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 = 6, int period2 = 14);
	std::vector<WRData> CalculateKLineWR(const std::vector<STOCK::KLinePoint>& klineData, int period1 = 6, int period2 = 14);
	void DrawTimelineWRChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<WRData>& wrData, int startIndex = 0, int xAxisPoints = 0);

	// RSI相对强弱指标计算与绘制
	struct RSIData {
		double rsi1;   // RSI6（短期）
		double rsi2;   // RSI14（长期）
		bool valid;
	};
	std::vector<RSIData> CalculateTimelineRSI(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 = 6, int period2 = 14);
	std::vector<RSIData> CalculateKLineRSI(const std::vector<STOCK::KLinePoint>& klineData, int period1 = 6, int period2 = 14);
	void DrawTimelineRSIChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<RSIData>& rsiData, int startIndex = 0, int xAxisPoints = 0);

	// 走势图绘制
	void DrawKLineTrendCurve(CDC& memDC, const KLineDrawData& drawData, std::vector<CPoint>& outPoints);
	// 布林带绘制（5分钟K线模式）
	void DrawBollBands(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLineTrendBuyMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints);
	void DrawKLineTrendPeriodMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints, const PeriodPoint periodHighs[3], const PeriodPoint periodLows[3]);
	void DrawKLineTrendChart(CDC& memDC, int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData, const STOCK::StockInfo& stockInfo);

	void DrawKLinePositionInfo(CDC& memDC, int x, int y, int chartWidth, const STOCK::StockInfo& realtimeData);
	void DrawKLineInfoPanel(CDC& memDC, int left, int right, int bottomY, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData);
	void DrawChipPeakPanel(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo, const STOCK::ChipDistribution& chipData, const std::vector<STOCK::TimelinePoint>& timelinePoint);
	void DrawOverviewTable(CDC& memDC, int x, int y, int w, int h, int vScrollOffset = 0, int totalHeight = 0);
	void DrawIndexSection(CDC& memDC, int x, int y, int w, const std::vector<std::pair<std::wstring, STOCK::StockInfo>>& indices);
	void DrawStockListPanel(CDC& memDC, int x, int y, int w, int h, const std::wstring& currentStockId);
	void DrawCallAuctionChart(CDC& memDC, const TimelineDrawContext& ctx, const STOCK::CallAuctionData& callAuctionData);

	// 总览表行信息（用于双击处理）
	struct OverviewRowInfo {
		std::wstring code;
		int rowY = 0;
		int rowH = 0;
		int nameColWidth = 0;
		int deleteBtnStartX = 0;
		int deleteBtnEndX = 0;
	};

	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	void UpdateModeButtons();
	void UpdatePeriodComboVisibility();

	CTransparentWnd m_CTransparentWnd;
	CButton m_btnTimeLine;
	CButton m_btnKLine;
	CButton m_btnMin5KLine;
	CButton m_btnMin30KLine;
	CButton m_btnJZ;
	CButton m_btnMA;
	CButton m_btnBoll;
	CButton m_btnClose;
	CButton m_btnExpand;      // 放大按钮（隐藏副图，走势图占3/4）
	CButton m_btnToggleStockList;  // 股票列表显示/隐藏按钮
	CButton m_btnCallAuction;     // 集合竞价按钮
	CButton m_btnZoomOut;  // 缩小按钮（显示240分钟）
	CButton m_btnZoomIn;   // 放大按钮（显示60分钟）
	CButton m_btnIndicatorMACD;  // MACD指标按钮
	CButton m_btnIndicatorKDJ;   // KDJ指标按钮
	CButton m_btnIndicatorWR;    // W&R指标按钮
	CButton m_btnIndicatorRSI;   // RSI指标按钮
	CButton m_btnChipPeak;       // 筹码峰按钮
	CButton m_btnOrderBook;      // 盘口按钮（与筹码峰按钮切换）
	CFont m_chipPeakFont;        // 筹码峰按钮小字体
	CScrollBar m_hScrollBar;
	std::wstring m_stock_id;
	bool m_isKLineMode{};
	bool m_isMin5KLineMode{};  // 5分钟K线模式（m_isKLineMode为true时的子模式）
	bool m_isMin30KLineMode{};  // 30分钟K线模式（m_isKLineMode为true时的子模式）
	bool m_isOverviewMode{};
	bool m_klineDataLoaded{ false };
	int m_klinePeriodDays{ 250 };
	int m_scrollOffset{ 0 };
	int m_timelineScrollOffset{ -1 };  // 分时图水平滚动偏移，-1表示需要自动滚动到末尾
	int m_timelineVisibleCount{ 40 };  // 分时图可见数据点数
	int m_timelineLastTotalPoints{ 0 };  // 上次绘制的数据点数，用于判断新数据追加时是否自动跟随
	int m_vScrollOffset{ 0 };

	// 分时图指标类型
	enum class TimelineIndicator { MACD, KDJ, WR, RSI };
	TimelineIndicator m_timelineIndicator{ TimelineIndicator::MACD };
	bool m_indicatorBtnsInitialized{ false };

	// 分时图鼠标拖动滚动
	bool m_isTimelineDragging{ false };
	CPoint m_timelineDragStartPos;
	int m_timelineDragStartOffset{ 0 };
	// K线图鼠标拖动滚动
	bool m_isKLineDragging{ false };
	CPoint m_klineDragStartPos;
	int m_klineDragStartOffset{ 0 };
	HCURSOR m_hPrevCursor{ NULL };
	volatile BOOL m_isDestroying;
	CFont* m_pfont{};
	CString loading_state_txt;

	// 鼠标悬停数据
	CPoint m_mousePos;
	bool m_isHoveringVolume{ false };
	int m_hoveredBarIndex{ -1 };
	STOCK::TimelinePoint m_hoveredData;
	// 悬停点的MA值及前一点MA值（用于箭头方向）
	STOCK::Price m_hoverMa1{ 0 }, m_hoverMa5{ 0 }, m_hoverMa10{ 0 }, m_hoverMa20{ 0 };
	STOCK::Price m_hoverPrevMa1{ 0 }, m_hoverPrevMa5{ 0 }, m_hoverPrevMa10{ 0 }, m_hoverPrevMa20{ 0 };
	CString m_hoverTip;
	// 分时图标题栏悬停提示
	CString m_timelinePriceTitleTip;   // 走势图标题栏：现价/均价/MA5/MA10...
	CString m_timelineVolumeTitleTip;  // 量柱图标题栏：成交量/成交额
	CString m_timelineMacdTitleTip;    // MACD标题栏：DIF/DEA/MACD
	CString m_timelineKdjTitleTip;     // KDJ标题栏：K/D/J
	CString m_timelineWrTitleTip;      // WR标题栏：WR1/WR2
	CString m_timelineRsiTitleTip;     // RSI标题栏：RSI1/RSI2
	CString m_chipPeakTip;             // 筹码峰提示

	// 双击检测
	DWORD m_lastClickTime{};
	CPoint m_lastClickPos;
	std::wstring m_pendingEditStockCode;
	CString m_pendingTradeTime;
	double m_pendingTradePrice{ 0.0 };

	// 日K线鼠标悬停数据
	bool m_isHoveringKLine{ false };
	bool m_isHoveringKLineVolume{ false };
	bool m_isHoveringKDJ{ false };
	bool m_showTrendView{ false };
	bool m_showChipPeak{ false };
	bool m_expandedMode{ false };  // 放大模式：隐藏副图，走势图3/4+成交量1/4
	bool m_showStockList{ true };  // 是否显示左侧股票列表面板
	bool m_isCallAuctionMode{ false };  // 集合竞价模式
	bool m_showJZCurve{ false };  // 基金净值曲线
	bool m_showMA{ false };
	bool m_showBollBands{ true };
	bool m_showAmplitudeBands{ false };  // 振幅上下线（与布林带互斥）
	int m_klineHoveredBarIndex{ -1 };
	CString m_klineHoverTip;
	CString m_klineVolumeHoverTip;
	CString m_klineTrendHoverTip;
	CString m_kdjHoverTip;

	// 5分钟K线图整点时间标签（X轴：centerX, "h:mm"）
	std::vector<std::pair<int, CString>> m_min5HourLabels;

	// 总览表行信息（用于双击处理）
	std::vector<OverviewRowInfo> m_overviewRows;
};
