#pragma once

#include <afxwin.h>
#include <string>
#include <vector>
#include <StockDef.h>
#include <TransparentWnd.h>

// 定义自定义消息
#define FWND_MSG_UPDATE_STATUS (WM_USER + 100)
#define FWND_MSG_REQUEST_DATA (WM_USER + 101)
#define FWND_MSG_SHOW_EDIT_DLG (WM_USER + 102)
#define FWND_MSG_SHOW_ADD_DLG (WM_USER + 103)
#define FWND_MSG_SHOW_TRADE_DLG (WM_USER + 104)

class CFloatingWnd : public CWnd
{
public:
	CFloatingWnd();
	virtual ~CFloatingWnd();

	BOOL Create(CFont* font, CPoint pt, std::wstring stock_id);
	void RequestData();
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
	LRESULT OnRequestData(WPARAM wParam, LPARAM lParam);
	LRESULT OnCloseWindow(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowEditDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowAddDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowTradeDialog(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedTimeLineBtn();
	afx_msg void OnBnClickedKLineBtn();
	afx_msg void OnBnClickedMin5KLineBtn();
	afx_msg void OnCbnSelChangeKLinePeriod();
	afx_msg void OnBnClickedTrendBtn();
	afx_msg void OnBnClickedCloseBtn();
	afx_msg void OnBnClickedMABtn();
	afx_msg void OnBnClickedBollBtn();
	afx_msg void OnBnClickedZoomOutBtn();
	afx_msg void OnBnClickedZoomInBtn();
	afx_msg void OnBnClickedOverviewBtn();

private:
	static UINT NetworkThreadProc(LPVOID pParam); // 线程函数
	CPoint Stock2Point(int x, int y, int w, int h, float unitY, const STOCK::TimelinePoint& item, const STOCK::Price prevClosePrice);
	void DrawOrderBook(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData);
	void DrawVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const STOCK::StockInfo* stockInfo = nullptr, int startIndex = 0, int visibleCount = -1);

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
		int startIndex{ 0 };     // 可见数据起始索引
		int scrollRange{ 0 };    // 滚动范围
		STOCK::Price maxPrice{ 0 };   // 可见区间最大价（含内边距）
		STOCK::Price minPrice{ 0 };   // 可见区间最小价（含内边距）
		float unitY{ 0 };             // Y轴缩放（像素/价格）
		STOCK::StockInfo realtimeData;
		const std::vector<STOCK::TimelinePoint>* timelinePoint;
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

	// T+0日内买卖信号
	struct T0Signal {
		bool valid;         // 是否有有效信号
		bool isBuy;         // true=买点, false=卖点
		int strength;       // 信号强度 1-3
		CString reason;     // 信号原因描述
		double price;       // 信号价格
		CString time;       // 信号时间
	};

	void DrawHeader(CDC& memDC, const STOCK::StockInfo& realtimeData, int windowWidth, int headerHeight);
	void DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineMAIndicators(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineGridLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelinePriceCurve(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineVolumeSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineMACDSection(CDC& memDC, const TimelineDrawContext& ctx);
	void DrawTimelineTitleBars(CDC& memDC, const TimelineDrawContext& ctx, int priceChartTop, int volumeChartTop, int macdChartTop, int timelineTitleHeight);
	void DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData, int startIndex = 0, int visibleCount = -1);
	void DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData, const std::vector<MACDData>& macdData, int startIndex = 0, int visibleCount = -1);
	std::vector<MACDData> CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint);
	std::vector<MACDData> CalculateKLineMACD(const std::vector<STOCK::KLinePoint>& klineData);
	// 为每个分时数据点计算MA5/MA10/MA20滚动均价（修改timelinePoint中的ma5/ma10/ma20字段）
	static void CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint);
	// 计算滚动均价：N分钟成交额/N分钟成交量（单次查询）
	static STOCK::Price CalcRollingAvgPrice(const std::vector<STOCK::TimelinePoint>& timelinePoint, int nMinutes);
	std::vector<MACDCrossSignal> DetectMACDCross(const std::vector<MACDData>& macdData);
	MACDCrossSignal GetLatestMACDCross(const std::vector<MACDData>& macdData);
	T0Signal DetectBuySignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData);
	T0Signal DetectSellSignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData);
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
		float unitY;
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
	void DrawKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData);

	// 走势图绘制
	void DrawKLineTrendCurve(CDC& memDC, const KLineDrawData& drawData, std::vector<CPoint>& outPoints);
	// 布林带绘制（5分钟K线模式）
	void DrawBollBands(CDC& memDC, const KLineDrawData& drawData);
	void DrawKLineTrendBuyMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints);
	void DrawKLineTrendPeriodMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints, const PeriodPoint periodHighs[3], const PeriodPoint periodLows[3]);
	void DrawKLineTrendChart(CDC& memDC, int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData, const STOCK::StockInfo& stockInfo);

	void DrawKLinePositionInfo(CDC& memDC, int x, int y, int chartWidth, const STOCK::StockInfo& realtimeData);
	void DrawKLineInfoPanel(CDC& memDC, int left, int right, int bottomY, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData);
	void DrawOverviewTable(CDC& memDC, int x, int y, int w, int h, int vScrollOffset = 0, int totalHeight = 0);
	void DrawIndexSection(CDC& memDC, int x, int y, int w, const std::vector<std::pair<std::wstring, STOCK::StockInfo>>& indices);

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
	CButton m_btnOverview;
	CButton m_btnTimeLine;
	CButton m_btnKLine;
	CButton m_btnMin5KLine;
	CButton m_btnTrend;
	CButton m_btnMA;
	CButton m_btnBoll;
	CButton m_btnClose;
	CButton m_btnZoomOut;  // 缩小按钮（显示240分钟）
	CButton m_btnZoomIn;   // 放大按钮（显示60分钟）
	CComboBox m_comboKLinePeriod;
	CScrollBar m_hScrollBar;
	std::wstring m_stock_id;
	bool m_is_thread_running{};
	bool m_isKLineMode{};
	bool m_isMin5KLineMode{};  // 5分钟K线模式（m_isKLineMode为true时的子模式）
	bool m_isOverviewMode{};
	bool m_klineDataLoaded{ false };
	int m_klinePeriodDays{ 250 };
	int m_scrollOffset{ 0 };
	int m_timelineScrollOffset{ 0 };  // 分时图水平滚动偏移
	int m_timelineVisibleCount{ 60 };  // 分时图可见数据点数（240=1天，最小60）
	int m_vScrollOffset{ 0 };

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

	unsigned __int64 m_last_request_time{};

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
	bool m_showMA{ false };
	bool m_showBollBands{ false };
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
