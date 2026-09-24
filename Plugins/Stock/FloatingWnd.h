#pragma once

#include <afxwin.h>
#include <string>
#include <vector>
#include <StockDef.h>
#include <TransparentWnd.h>
#include "SignalAnalyzer.h"
#include "StockIndicator.h"
#include "ChartContext.h"
#include "StockListPanel.h"
#include "CallAuctionChart.h"
#include "ChipPeakPanel.h"
#include "OrderBookPanel.h"
#include "OverviewPanel.h"
#include "IndicatorChart.h"
#include "StatusBarPanel.h"
#include "TimelineChart.h"
#include "TechnicalIndicatorModel.h"

// 定义自定义消息
#define FWND_MSG_UPDATE_STATUS (WM_USER + 100)
#define FWND_MSG_SHOW_EDIT_DLG (WM_USER + 102)
#define FWND_MSG_SHOW_ADD_DLG (WM_USER + 103)
#define FWND_MSG_SHOW_TRADE_DLG (WM_USER + 104)

// 定义时间线可见点数常量
#define TIME_LINE_VISIBLE_COUNT_1MIN 30
#define TIME_LINE_VISIBLE_COUNT_5MIN 24
#define TIME_LINE_VISIBLE_COUNT_30MIN 16
#define TIME_LINE_VISIBLE_COUNT_1DAY 20
#define TIME_LINE_VISIBLE_COUNT_STEP 10

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
	afx_msg void OnDestroy();
	LRESULT OnUpdateStatus(WPARAM wParam, LPARAM lParam);
	LRESULT OnCloseWindow(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowEditDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowAddDialog(WPARAM wParam, LPARAM lParam);
	LRESULT OnShowTradeDialog(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedTimeLineBtn();
	afx_msg void OnBnClickedMin5KLineBtn();
	afx_msg void OnBnClickedMin30KLineBtn();
	afx_msg void OnBnClickedKLineBtn();
	afx_msg void OnBnClickedCloseBtn();
	afx_msg void OnBnClickedMABtn();
	afx_msg void OnBnClickedBollBtn();
	afx_msg void OnBnClickedZoomOutBtn();
	afx_msg void OnBnClickedZoomInBtn();
	afx_msg void OnBnClickedIndicatorMACDBtn();
	afx_msg void OnBnClickedIndicatorMACDSignalBtn();
	afx_msg void OnBnClickedIndicatorKDJBtn();
	afx_msg void OnBnClickedIndicatorWRBtn();
	afx_msg void OnBnClickedIndicatorRSIBtn();
	afx_msg void OnBnClickedChipPeakBtn();
	afx_msg void OnBnClickedOrderBookBtn();
	afx_msg void OnBnClickedTickDetailBtn();
	afx_msg void OnBnClickedExpandBtn();
	afx_msg void OnBnClickedToggleStockListBtn();
	afx_msg void OnLButtonDblClkModeSwitch();  // 左键双击按顺序切到左侧界面（对称右键）
	void SwitchViewMode(UIViewMode viewMode); // 按目标模式切换界面（总览/分时/5分/30分/日K）

private:
	void EnsureChipPeakData();
	void ResetHoverState();           // 重置所有悬停状态
	void SetTimelineModeDefaults();   // 设置分时模式默认参数
	void SetDayKLineModeDefaults();   // 设置日K模式默认参数
	void SetMin5KLineModeDefaults();  // 设置5分钟K线模式默认参数
	void SetMin30KLineModeDefaults(); // 设置30分钟K线模式默认参数
	static void SafeSetWindowPos(CWnd& wnd, int x, int y, int cx, int cy);
	static void SafeShowWindow(CWnd& wnd, bool show);
	static void SafeSetButtonStyle(CButton& btn, UINT style);

	// MACDData/MACDCrossSignal/KDJData/WRData/RSIData 类型别名已移至各模块类
	// 走势图绘制已移至CTimelineChart
	// MACD/KDJ/WR/RSI/成交量绘制已移至CIndicatorChart
	// DrawHeader/DrawTimelinePositionInfo/DrawKLinePositionInfo/DrawKLineInfoPanel 已移至CStatusBarPanel
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	void UpdateModeButtons();
	void UpdatePeriodComboVisibility();
	void ApplySignalColors(COLORREF bollColor, COLORREF macdColor, COLORREF kdjColor, COLORREF wrColor, COLORREF rsiColor, COLORREF maColor);

	CTransparentWnd m_CTransparentWnd;
	CStockListPanel m_stockListPanel;
	CCallAuctionChart m_callAuctionChart;
	CChipPeakPanel m_chipPeakPanel;
	COrderBookPanel m_orderBookPanel;
	MarketOrderBook m_orderBook;   // 盘口数据模型（数据与视图分离，渲染层只读）
	COverviewPanel m_overviewPanel;
	CIndicatorChart m_indicatorChart;
	CStatusBarPanel m_statusBarPanel;
	CTimelineChart m_timelineChart;
	CButton m_btnTimeLine;
	CButton m_btnKLine;
	CButton m_btnMin5KLine;
	CButton m_btnMin30KLine;
	CButton m_btnMA;
	CButton m_btnBoll;
	CButton m_btnClose;
	CButton m_btnExpand;      // 放大按钮（隐藏副图，走势图占3/4）
	CButton m_btnToggleStockList;  // 股票列表显示/隐藏按钮
	CButton m_btnZoomOut;  		// 缩小按钮（显示240分钟）
	CButton m_btnZoomIn;   		// 放大按钮（显示60分钟）
	CButton m_btnIndicatorCJL;  // CJL指标按钮
	CButton m_btnIndicatorMACD;  // MACD信号按钮
	CButton m_btnIndicatorKDJ;   // KDJ指标按钮
	CButton m_btnIndicatorWR;    // W&R指标按钮
	CButton m_btnIndicatorRSI;   // RSI指标按钮
	CButton m_btnChipPeak;       // 筹码峰按钮
	CButton m_btnOrderBook;      // 盘口按钮（与筹码峰按钮切换）
	CButton m_btnTickDetail;     // 成交明细按钮（MX）
	CFont m_chipPeakFont;        // 筹码峰按钮小字体
	std::wstring m_stock_id;
	UIViewMode m_viewMode{ UI_VIEW_MIN5_KLINE };  // 当前界面视图模式
	TechnicalIndicatorModel m_indicatorModel;  // 技术指标数据模型（数据与视图分离，渲染层只读缓存序列）
	int m_lastIndicatorDataSize{ -1 };         // 上次刷新指标模型时的序列长度
	UIViewMode m_lastIndicatorViewMode{ UI_VIEW_MIN5_KLINE };
	// 右键切换：从K线模式切到分时前的界面模式，再右键切回用
	UIViewMode m_prevKLineViewMode{ UI_VIEW_OVERVIEW };  // UI_VIEW_OVERVIEW表示无来源记录
	bool m_indicatorLoaded{ false };
	DWORD m_lastIndicatorRefreshTick{ 0 };     // 上次定时器触发指标重算的时机(ms)，1秒节流
	int m_timelineScrollOffset{ -1 };  // 分时图水平滚动偏移，-1表示需要自动滚动到末尾
	int m_timelineVisibleCount{ 30 };  // 分时图可见数据点数
	int m_timelineLastTotalPoints{ 0 };  // 上次绘制的数据点数，用于判断新数据追加时是否自动跟随
	int m_vScrollOffset{ 0 };

	// 分时图指标类型
	enum class TimelineIndicator { CJL, MACD, KDJ, WR, RSI };
	TimelineIndicator m_timelineIndicator{ TimelineIndicator::KDJ };
	// MACD图内叠加KDJ的J线开关（点击KDJ按钮切换；模型允许时叠加，再点恢复）
	bool m_overlayKDJInMacd{ true };
	bool m_indicatorBtnsInitialized{ false };

	// 分时图鼠标拖动滚动
	bool m_isTimelineDragging{ false };
	CPoint m_timelineDragStartPos;
	int m_timelineDragStartOffset{ 0 };
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

	// 视图选项
	bool m_showTrendView{ false };
	bool m_showChipPeak{ false };
	bool m_showTickDetail{ false };  // 明细(MX)模式：盘口区域显示最近20条成交明细
	bool m_expandedMode{ true };  // 放大模式：隐藏副图，走势图3/4+成交量1/4
	bool m_showKdjChart{ false };  // 下方KDJ/WR/RSI指标图形区域显示开关（MACD按钮切换，默认隐藏）
	bool m_showStockList{ false };  // 是否显示左侧股票列表面板
	bool m_showJZCurve{ false };  // 基金净值曲线
	bool m_showMA{ false };
	bool m_showBollBands{ true };
	volatile bool m_chartDirty{ false };      // 图表数据更新标识（走势图/K线/MACD等），由PostMessage设置
	volatile bool m_orderBookDirty{ false };  // 盘口数据更新标识（五档/成交/净比等），由共享内存回调设置

	// 总览表行信息（用于双击处理）
	std::vector<OverviewRowInfo> m_overviewRows;

	// 信号颜色（由ApplySignalColors设置，供OnDrawItem使用）
	COLORREF m_bollSignalColor{ CLR_INVALID };
	COLORREF m_macdSignalColor{ CLR_INVALID };
	COLORREF m_kdjSignalColor{ CLR_INVALID };
	COLORREF m_wrSignalColor{ CLR_INVALID };
	COLORREF m_rsiSignalColor{ CLR_INVALID };
	COLORREF m_maSignalColor{ CLR_INVALID };
};
