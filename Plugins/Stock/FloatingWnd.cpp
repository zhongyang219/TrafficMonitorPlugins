#include "pch.h"
#include "FloatingWnd.h"
#include <afxinet.h>
#include <memory>
#include <map>
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>
#include <algorithm>
#include <set>
#include <cstdlib>
#include "OptionsDlg.h"
#include "TradeRecordDialog.h"
#include "EstVolumeDialog.h"
#include "StockFetchThread.h"
#include "SmartSignalTestDlg.h"
#include "ChartColors.h"
#include "StockListPanel.h"
#include "CallAuctionChart.h"

// 大盘指数优先级列表与 GetStockPriority 已移至 Stock.h/Stock.cpp，供各模块共享

// 颜色常量已移至 ChartColors.h，供各图表模块共享

void DrawPricePointLabel(CDC& memDC, int pointX, int pointY, int chartLeft, int chartTop, int chartWidth, int chartHeight,
	STOCK::Price price, bool isHigh, COLORREF color)
{
	CString label = CCommon::FormatFloat(price);
	CSize labelSize = memDC.GetTextExtent(label);
	const int gap = g_data.RDPI(4);
	const int arrowGap = g_data.RDPI(10);
	const int chartRight = chartLeft + chartWidth;
	const int chartBottom = chartTop + chartHeight;

	int labelX = pointX - labelSize.cx / 2;
	int labelY = isHigh ? pointY - labelSize.cy - arrowGap : pointY + arrowGap;
	bool useSideLabel = labelX < chartLeft || labelX + labelSize.cx > chartRight;

	if (useSideLabel)
	{
		if (pointX < chartLeft + chartWidth / 2)
		{
			label.Format(_T("\u2190%s"), CCommon::FormatFloat(price));
			labelSize = memDC.GetTextExtent(label);
			labelX = pointX + gap;
		}
		else
		{
			label.Format(_T("%s\u2192"), CCommon::FormatFloat(price));
			labelSize = memDC.GetTextExtent(label);
			labelX = pointX - labelSize.cx - gap;
		}
		labelY = pointY - labelSize.cy / 2;
		labelX = max(chartLeft, min(labelX, chartRight - labelSize.cx));
		labelY = max(chartTop, min(labelY, chartBottom - labelSize.cy));
		memDC.SetTextColor(color);
		memDC.TextOut(labelX, labelY, label);
		return;
	}

	labelY = max(chartTop, min(labelY, chartBottom - labelSize.cy));
	memDC.SetTextColor(color);
	memDC.TextOut(labelX, labelY, label);

	int fromX = labelX + labelSize.cx / 2;
	int fromY = isHigh ? labelY + labelSize.cy : labelY;
	if (abs(pointY - fromY) > g_data.RDPI(2))
	{
		CPen pen(PS_SOLID, 1, color);
		CPen* pOldPen = memDC.SelectObject(&pen);
		memDC.MoveTo(fromX, fromY);
		memDC.LineTo(pointX, pointY);

		int dir = (pointY >= fromY) ? 1 : -1;
		int arrowLen = g_data.RDPI(4);
		int arrowHalf = g_data.RDPI(3);
		memDC.MoveTo(pointX, pointY);
		memDC.LineTo(pointX - arrowHalf, pointY - dir * arrowLen);
		memDC.MoveTo(pointX, pointY);
		memDC.LineTo(pointX + arrowHalf, pointY - dir * arrowLen);
		memDC.SelectObject(pOldPen);
	}
}

#define ORDER_BOOK_WIDTH          g_data.RDPI(168)     // 右侧信息面板宽度

enum {
	IDC_TIMELINE_BTN = 1001,
	IDC_KLINE_BTN = 1002,
	IDC_CLOSE_BTN = 1005,
	IDM_CLOSE_WINDOW = 1006,
	IDC_MA_BTN = 1007,
	IDC_MIN5_KLINE_BTN = 1008,
	IDC_BOLL_BTN = 1009,
	IDC_ZOOM_OUT_BTN = 1010,
	IDC_ZOOM_IN_BTN = 1011,
	IDC_INDICATOR_MACD_BTN = 1012,
	IDC_INDICATOR_KDJ_BTN = 1013,
	IDC_MIN30_KLINE_BTN = 1014,
	IDC_INDICATOR_WR_BTN = 1015,
	IDC_INDICATOR_RSI_BTN = 1016,
	IDC_INDICATOR_MACD_SIGNAL_BTN = 1017,
	IDC_CHIP_PEAK_BTN = 1018,
	IDC_ORDER_BOOK_BTN = 1019,
	IDC_EXPAND_BTN = 1020,
	IDC_TOGGLE_STOCK_LIST_BTN = 1021,
	IDC_REFRESH_TIMER = 1023,
	IDC_TICK_DETAIL_BTN = 1024
};

BEGIN_MESSAGE_MAP(CFloatingWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_CTLCOLOR()
	ON_WM_DRAWITEM()
	ON_MESSAGE((WM_USER + 100), OnUpdateStatus)
	ON_MESSAGE((WM_USER + 102), OnShowEditDialog)
	ON_MESSAGE((WM_USER + 103), OnShowAddDialog)
	ON_MESSAGE((WM_USER + 104), OnShowTradeDialog)
	ON_MESSAGE(IDM_CLOSE_WINDOW, OnCloseWindow)
	ON_BN_CLICKED(IDC_TIMELINE_BTN, &CFloatingWnd::OnBnClickedTimeLineBtn)
	ON_BN_CLICKED(IDC_KLINE_BTN, &CFloatingWnd::OnBnClickedKLineBtn)
	ON_BN_CLICKED(IDC_CLOSE_BTN, &CFloatingWnd::OnBnClickedCloseBtn)
	ON_BN_CLICKED(IDC_MA_BTN, &CFloatingWnd::OnBnClickedMABtn)
	ON_BN_CLICKED(IDC_MIN5_KLINE_BTN, &CFloatingWnd::OnBnClickedMin5KLineBtn)
	ON_BN_CLICKED(IDC_MIN30_KLINE_BTN, &CFloatingWnd::OnBnClickedMin30KLineBtn)
	ON_BN_CLICKED(IDC_BOLL_BTN, &CFloatingWnd::OnBnClickedBollBtn)
	ON_BN_CLICKED(IDC_ZOOM_OUT_BTN, &CFloatingWnd::OnBnClickedZoomOutBtn)
	ON_BN_CLICKED(IDC_ZOOM_IN_BTN, &CFloatingWnd::OnBnClickedZoomInBtn)
	ON_BN_CLICKED(IDC_INDICATOR_MACD_BTN, &CFloatingWnd::OnBnClickedIndicatorMACDBtn)
	ON_BN_CLICKED(IDC_INDICATOR_MACD_SIGNAL_BTN, &CFloatingWnd::OnBnClickedIndicatorMACDSignalBtn)
	ON_BN_CLICKED(IDC_INDICATOR_KDJ_BTN, &CFloatingWnd::OnBnClickedIndicatorKDJBtn)
	ON_BN_CLICKED(IDC_INDICATOR_WR_BTN, &CFloatingWnd::OnBnClickedIndicatorWRBtn)
	ON_BN_CLICKED(IDC_INDICATOR_RSI_BTN, &CFloatingWnd::OnBnClickedIndicatorRSIBtn)
	ON_BN_CLICKED(IDC_CHIP_PEAK_BTN, &CFloatingWnd::OnBnClickedChipPeakBtn)
	ON_BN_CLICKED(IDC_ORDER_BOOK_BTN, &CFloatingWnd::OnBnClickedOrderBookBtn)
	ON_BN_CLICKED(IDC_TICK_DETAIL_BTN, &CFloatingWnd::OnBnClickedTickDetailBtn)
	ON_BN_CLICKED(IDC_EXPAND_BTN, &CFloatingWnd::OnBnClickedExpandBtn)
	ON_BN_CLICKED(IDC_TOGGLE_STOCK_LIST_BTN, &CFloatingWnd::OnBnClickedToggleStockListBtn)
END_MESSAGE_MAP()

int CFloatingWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	const int btnWidth = g_data.RDPI(40);
	const int btnHeight = g_data.RDPI(22);
	const int btnGap = 0;  // 按钮之间不留缝隙

	const int closeBtnWidth = g_data.RDPI(20);
	const int closeBtnHeight = g_data.RDPI(18);
	const int cx = lpCreateStruct->cx;
	// 顶部按钮统一靠右排列：分时、5分、30分、日K、股票列表、放大、X（与关闭按钮挨着，
	// 自绘按钮高亮当前模式背景）；标题文字改为从最左侧开始显示
	const int toggleStockListBtnWidth = closeBtnWidth;
	const int toggleStockListBtnHeight = closeBtnHeight;
	const int expandBtnWidth = closeBtnWidth;
	const int expandBtnHeight = closeBtnHeight;
	// 静态布局仅作占位，OnPaint 中会统一重新定位
	CRect klineRect(0, 0, btnWidth, btnHeight);
	m_btnKLine.Create(_T("日K"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, klineRect, this, IDC_KLINE_BTN);

	CRect timelineRect(0, 0, btnWidth, btnHeight);
	m_btnTimeLine.Create(_T("分时"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, timelineRect, this, IDC_TIMELINE_BTN);

	CRect min5KLineRect(0, 0, btnWidth, btnHeight);
	m_btnMin5KLine.Create(_T("5分"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, min5KLineRect, this, IDC_MIN5_KLINE_BTN);

	CRect min30KLineRect(0, 0, btnWidth, btnHeight);
	m_btnMin30KLine.Create(_T("30分"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, min30KLineRect, this, IDC_MIN30_KLINE_BTN);

	CRect toggleStockListBtnRect(0, 0, toggleStockListBtnWidth, toggleStockListBtnHeight);
	m_btnToggleStockList.Create(_T("<|"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, toggleStockListBtnRect, this, IDC_TOGGLE_STOCK_LIST_BTN);

	CRect expandBtnRect(0, 0, expandBtnWidth, expandBtnHeight);
	m_btnExpand.Create(_T("△"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, expandBtnRect, this, IDC_EXPAND_BTN);

	CRect closeBtnRect(cx - closeBtnWidth, g_data.RDPI(2), cx, g_data.RDPI(2) + closeBtnHeight);
	m_btnClose.Create(_T("X"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, closeBtnRect, this, IDC_CLOSE_BTN);

	const int rightBtnWidth = g_data.RDPI(40);

	CRect chipPeakBtnRect(closeBtnRect.left - rightBtnWidth, g_data.RDPI(2), closeBtnRect.left, g_data.RDPI(2) + btnHeight);
	m_btnChipPeak.Create(_T("CM"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, chipPeakBtnRect, this, IDC_CHIP_PEAK_BTN);

	// 明细按钮（MX）在 PK 与 CM 之间
	CRect tickDetailBtnRect(closeBtnRect.left - rightBtnWidth * 2, g_data.RDPI(2), closeBtnRect.left - rightBtnWidth, g_data.RDPI(2) + btnHeight);
	m_btnTickDetail.Create(_T("MX"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, tickDetailBtnRect, this, IDC_TICK_DETAIL_BTN);

	CRect orderBookBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnOrderBook.Create(_T("PK"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, orderBookBtnRect, this, IDC_ORDER_BOOK_BTN);

	CRect bollBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnBoll.Create(_T("BL"), WS_CHILD | BS_OWNERDRAW, bollBtnRect, this, IDC_BOLL_BTN);
	m_btnBoll.ShowWindow(SW_HIDE);

	CRect maBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnMA.Create(_T("MA"), WS_CHILD | BS_OWNERDRAW, maBtnRect, this, IDC_MA_BTN);
	m_btnMA.ShowWindow(SW_HIDE);

	// 缩放按钮（分时模式专用，初始隐藏，在量柱图标题栏右侧定位）
	const int zoomBtnWidth = g_data.RDPI(28);
	const int zoomBtnHeight = g_data.RDPI(16);
	CRect zoomOutRect(0, 0, zoomBtnWidth, zoomBtnHeight);
	CRect zoomInRect(0, 0, zoomBtnWidth, zoomBtnHeight);
	m_btnZoomOut.Create(_T("<"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, zoomOutRect, this, IDC_ZOOM_OUT_BTN);
	m_btnZoomIn.Create(_T(">"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, zoomInRect, this, IDC_ZOOM_IN_BTN);
	m_btnZoomOut.ShowWindow(SW_HIDE);
	m_btnZoomIn.ShowWindow(SW_HIDE);

	// MACD/KDJ指标切换按钮（初始隐藏，在OnPaint中定位）
	m_btnIndicatorMACD.Create(_T("MACD"), WS_CHILD | BS_OWNERDRAW, CRect(0, 0, 0, 0), this, IDC_INDICATOR_MACD_SIGNAL_BTN);
	m_btnIndicatorKDJ.Create(_T("KDJ"), WS_CHILD | BS_OWNERDRAW, CRect(0, 0, 0, 0), this, IDC_INDICATOR_KDJ_BTN);
	m_btnIndicatorWR.Create(_T("W&&R"), WS_CHILD | BS_OWNERDRAW, CRect(0, 0, 0, 0), this, IDC_INDICATOR_WR_BTN);
	m_btnIndicatorRSI.Create(_T("RSI"), WS_CHILD | BS_OWNERDRAW, CRect(0, 0, 0, 0), this, IDC_INDICATOR_RSI_BTN);
	m_btnIndicatorCJL.Create(_T("CJL"), WS_CHILD | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_INDICATOR_MACD_BTN);
	m_btnIndicatorCJL.ShowWindow(SW_HIDE);
	m_btnIndicatorMACD.ShowWindow(SW_HIDE);
	m_btnIndicatorKDJ.ShowWindow(SW_HIDE);
	m_btnIndicatorWR.ShowWindow(SW_HIDE);
	m_btnIndicatorRSI.ShowWindow(SW_HIDE);

	// 初始分时模式下，基金默认显示净值曲线
	m_showJZCurve = CCommon::IsFundCode(m_stock_id);

	UpdateModeButtons();
	UpdatePeriodComboVisibility();

	// 固定1秒定时检查：数据变化时才重绘，无变化则跳过
	SetTimer(IDC_REFRESH_TIMER, 1000, NULL);

	Invalidate();
	return 0;
}

// 处理消息
LRESULT CFloatingWnd::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
	// wParam=0: 图表数据更新，wParam=1: 盘口数据更新
	if (wParam == 1)
		m_orderBookDirty = true;
	else
		m_chartDirty = true;
	return 0;
}

CFloatingWnd::CFloatingWnd() : m_isDestroying(FALSE), m_viewMode(UI_VIEW_MIN5_KLINE)
{
}

CFloatingWnd::~CFloatingWnd()
{
	// 标记窗口正在销毁
	m_isDestroying = TRUE;
	if (m_CTransparentWnd.GetSafeHwnd())
		m_CTransparentWnd.DestroyWindow();
}

BOOL CFloatingWnd::Create(CFont* font, CPoint pt, std::wstring stock_id)
{
	m_stock_id = stock_id;
	// 注册窗口类
	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, L"CTransparentWnd", &wndcls)))
	{
		wndcls.style = CS_HREDRAW | CS_VREDRAW;  // 不使用CS_DBLCLKS，让双击也发送WM_LBUTTONDOWN
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
		// wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wndcls.hbrBackground = NULL; // 重要：设置为NULL
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = L"CTransparentWnd";
		if (!AfxRegisterClass(&wndcls))
			return FALSE;
	}

	// 设置父窗口指针
	m_CTransparentWnd.SetParent(this);

	m_pfont = font;

	// 获取包含鼠标点的显示器
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { sizeof(MONITORINFO) };
	GetMonitorInfo(hMonitor, &mi);
	CRect screenRect = mi.rcWork; // 工作区域

	// 创建透明全屏窗口
	if (!m_CTransparentWnd.CreateEx(WS_EX_TOOLWINDOW /* | WS_EX_LAYERED */ /* | WS_EX_TRANSPARENT */,
		L"CTransparentWnd", L"", WS_POPUP | WS_VISIBLE,
		screenRect, NULL, 0, NULL))
	{
		TRACE(L"Failed to create transparent window\n");
		return FALSE;
	}

	const int WIDTH = g_data.RDPI(g_data.m_setting_data.m_kline_width);
	const int HEIGHT = g_data.RDPI(g_data.m_setting_data.m_kline_height);

	// 固定在屏幕左下角（任务栏上方）
	int x = screenRect.left + 3;
	int y = screenRect.bottom - HEIGHT;
	y = max(screenRect.top, y);

	CRect rect(x, y, x + WIDTH, y + HEIGHT);

	// 创建实际的浮动窗口
	if (!CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW),
		L"", WS_POPUP | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN,
		rect, &m_CTransparentWnd, 0))
	{
		TRACE(L"Failed to create floating window\n");
		m_CTransparentWnd.DestroyWindow();
		return FALSE;
	}

	// 确保浮动窗口在最顶层
	BringWindowToTop();
	SetForegroundWindow();

	// 设置弹出窗口半透明（180兼顾可读性和隐蔽性）
	HWND hWnd = this->m_hWnd;
	::SetWindowLongPtr(hWnd, GWL_EXSTYLE, ::GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	::SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 220, LWA_ALPHA);

	// 设置父窗口完全透明
	m_CTransparentWnd.SetLayeredWindowAttributes(0, 0, LWA_ALPHA);
	m_CTransparentWnd.ShowWindow(SW_SHOW);

	TRACE(L"Windows created successfully\n");
	return TRUE;
}

// ========== OnPaint ==========
void CFloatingWnd::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);

	CDC memDC;
	CBitmap memBitmap;
	memDC.CreateCompatibleDC(&dc);
	if (m_pfont)
	{
		memDC.SelectObject(m_pfont);
	}
	memBitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
	CBitmap* pOldBitmap = memDC.SelectObject(&memBitmap);

	memDC.FillSolidRect(rect, COLOR_WHITE);

	memDC.SetBkMode(TRANSPARENT);

	int x = rect.left, y = rect.top, h = rect.Height(), w = rect.Width();

	bool isIndex = (GetStockPriority(m_stock_id) < 200);
	// 大盘在K线模式下不显示盘口（所有K线模式m_viewMode>=UI_VIEW_MIN5_KLINE，自动覆盖）
	bool isIndexKLine = isIndex && m_viewMode >= UI_VIEW_MIN5_KLINE;

	const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;  // 左侧股票列表面板宽度
	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = w - orderBookWidth;
	// 左侧Y轴坐标区域宽度（所有图表统一预留）
	const int yAxisWidth = g_data.RDPI(50);

	const int headerHeight = g_data.RDPI(26);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int singleBarHeight = g_data.RDPI(20);  // 单行状态栏高度
	const int relatedBarHeight = singleBarHeight;  // 关联股票栏高度（1行，位于标题栏下方）
	const int indexBarHeight = singleBarHeight;    // 底部系统状态栏高度（1行）

	// 统一布局：标题栏 + 关联股票栏 + 走势图(2/5) + 成交量(1/5) + MACD(1/5) + KDJ(1/5) + 时间标签 + 底部系统状态栏
	int chartArea = h - headerHeight - relatedBarHeight - xAxisLabelHeight - indexBarHeight;
	int priceChartHeight, macdChartHeight, kdjChartHeight, volumeChartHeight;
	if (m_expandedMode)
	{
		priceChartHeight = chartArea * 3 / 5;
		macdChartHeight = chartArea / 5;
		kdjChartHeight = chartArea / 5;
		volumeChartHeight = 0;  // 放大模式不显示独立成交量图
	}
	else
	{
		priceChartHeight = chartArea * 2 / 5;   // 2/5
		macdChartHeight = chartArea / 5;        // 1/5
		kdjChartHeight = chartArea / 5;         // 1/5
		volumeChartHeight = chartArea / 5;      // 1/5
	}

	const int priceChartTop = headerHeight + relatedBarHeight;

	STOCK::StockInfo realtimeData;
	STOCK::ChipDistribution chipData;
	STOCK::CallAuctionData callAuctionData;
	std::vector<STOCK::TimelinePoint> timelinePoint;
	std::vector<STOCK::KLinePoint> klineData;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			realtimeData = stockData->info;
			chipData = stockData->chipDistribution;
			// 数据与视图分离：行情推送时已在数据层更新模型，这里只同步到绘制的副本
			m_orderBook.UpdateFromStockInfo(realtimeData);
			m_orderBook.RefreshTransCache();
			if (m_viewMode == UI_VIEW_DAY_KLINE)
			{
				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					klineData = klineObj->data;
					// 将日K线数据转换为TimelinePoint格式，复用分时绘制流程
					for (const auto& kp : klineObj->data)
					{
						STOCK::TimelinePoint tp;
						// 日K线 day 格式为 "YYYY-MM-DD"，tp.time 取 "MM-DD" 用于常规标签，tp.fullTime 存完整日期用于悬停高亮
						if (kp.day.length() >= 10)
						{
							tp.time = kp.day.substr(5, 5);  // "MM-DD"
							tp.fullTime = kp.day;           // "YYYY-MM-DD"
						}
						else
						{
							tp.time = kp.day;
						}
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;  // 日K线无分时均价，暂用收盘价
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_viewMode == UI_VIEW_MIN5_KLINE)
			{
				// 5分钟K线模式：获取5分钟K线数据，转换为TimelinePoint格式
				auto min5KLineObj = stockData->getMin5KLineData();
				if (min5KLineObj)
				{
					klineData = min5KLineObj->data;
					// 将5分钟K线数据转换为TimelinePoint格式
					for (const auto& kp : min5KLineObj->data)
					{
						STOCK::TimelinePoint tp;
						// 从 "YYYY-MM-DD HH:MM" 格式中提取 "HH:MM"
						auto spacePos = kp.day.find(' ');
						if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
							tp.time = kp.day.substr(spacePos + 1, 5);
						else if (kp.day.length() >= 5 && kp.day[2] == ':')
							tp.time = kp.day.substr(0, 5);
						else
							tp.time = kp.day;
						tp.fullTime = kp.day;
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;  // 暂用收盘价
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_viewMode == UI_VIEW_MIN30_KLINE)
			{
				// 30分钟K线模式：获取30分钟K线数据，转换为TimelinePoint格式
				auto min30KLineObj = stockData->getMin30KLineData();
				if (min30KLineObj)
				{
					klineData = min30KLineObj->data;
					// 将30分钟K线数据转换为TimelinePoint格式
					for (const auto& kp : min30KLineObj->data)
					{
						STOCK::TimelinePoint tp;
						// 从 "YYYY-MM-DD HH:MM" 格式中提取 "HH:MM"
						auto spacePos = kp.day.find(' ');
						if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
							tp.time = kp.day.substr(spacePos + 1, 5);
						else if (kp.day.length() >= 5 && kp.day[2] == ':')
							tp.time = kp.day.substr(0, 5);
						else
							tp.time = kp.day;
						tp.fullTime = kp.day;
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;  // 暂用收盘价
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else
			{
				auto timelineData = stockData->getTimelineData();
				if (timelineData)
				{
					timelinePoint = timelineData->data;
				}

				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					klineData = klineObj->data;
				}
			}
			// 集合竞价数据（所有模式都需要加载，竞价模式下用于绘图，其他模式用于盘口展示）
			callAuctionData = stockData->callAuctionData;
		}
	}

	if (m_viewMode != UI_VIEW_OVERVIEW)
	{
		// 数据加载前也先把顶部按钮定位到目标标题栏，避免停留在初始坐标
		{
			const int titleH = g_data.RDPI(16);
			int origVolTop = priceChartTop + priceChartHeight;
			int origIndicatorTop = origVolTop + volumeChartHeight;
			int origKdjTop = origIndicatorTop + macdChartHeight;
			int btnW = yAxisWidth - g_data.RDPI(4);
			int btnX = stockListWidth + g_data.RDPI(2);
			int btnGap = g_data.RDPI(1);
			// JZ/MA/BL在MACD区域
			int macdAreaTop = origIndicatorTop + titleH;
			int macdAreaBottom = origKdjTop;
			int macdAreaH = max(1, macdAreaBottom - macdAreaTop);
			int macdBtnH = max(g_data.RDPI(16), (macdAreaH - btnGap * 2) / 3);
			int btn1Y = macdAreaTop;
			int btn2Y = btn1Y + macdBtnH + btnGap;
			int btn3Y = btn2Y + macdBtnH + btnGap;
			SafeSetWindowPos(m_btnMA, btnX, btn1Y, btnW, macdBtnH);
			SafeSetWindowPos(m_btnBoll, btnX, btn2Y, btnW, macdBtnH);
			SafeSetWindowPos(m_btnIndicatorMACD, btnX, btn3Y, btnW, macdBtnH);
			// KDJ/W&R/RSI在KDJ区域
			int kdjAreaTop = origKdjTop + titleH;
			int kdjAreaBottom = origKdjTop + kdjChartHeight;
			int kdjAreaH = max(1, kdjAreaBottom - kdjAreaTop);
			int kdjBtnH = max(g_data.RDPI(16), (kdjAreaH - btnGap * 2) / 3);
			int btn4Y = kdjAreaTop;
			int btn5Y = btn4Y + kdjBtnH + btnGap;
			int btn6Y = min(btn5Y + kdjBtnH + btnGap, kdjAreaBottom - kdjBtnH);
			SafeSetWindowPos(m_btnIndicatorKDJ, btnX, btn4Y, btnW, kdjBtnH);
			SafeSetWindowPos(m_btnIndicatorWR, btnX, btn5Y, btnW, kdjBtnH);
			SafeSetWindowPos(m_btnIndicatorRSI, btnX, btn6Y, btnW, kdjBtnH);
			SafeShowWindow(m_btnIndicatorCJL, false);
			// 强制重绘自绘按钮，避免初始零尺寸创建后不显示
			m_btnIndicatorMACD.Invalidate();

			int closeBtnW = g_data.RDPI(20);
			int closeBtnH = g_data.RDPI(18);
			int headerBtnTop = g_data.RDPI(2);
			// 顶部按钮统一靠右：分时/5分/30分/日K紧挨股票列表/放大/X按钮
			SafeSetWindowPos(m_btnClose, w - closeBtnW, headerBtnTop, closeBtnW, closeBtnH);
			SafeSetWindowPos(m_btnExpand, w - closeBtnW * 2, headerBtnTop, closeBtnW, closeBtnH);
			SafeSetWindowPos(m_btnToggleStockList, w - closeBtnW * 3, headerBtnTop, closeBtnW, closeBtnH);
			SafeSetWindowPos(m_btnKLine, w - closeBtnW * 3 - g_data.RDPI(40), headerBtnTop, g_data.RDPI(40), g_data.RDPI(22));
			SafeSetWindowPos(m_btnMin30KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 2, headerBtnTop, g_data.RDPI(40), g_data.RDPI(22));
			SafeSetWindowPos(m_btnMin5KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 3, headerBtnTop, g_data.RDPI(40), g_data.RDPI(22));
			SafeSetWindowPos(m_btnTimeLine, w - closeBtnW * 3 - g_data.RDPI(40) * 4, headerBtnTop, g_data.RDPI(40), g_data.RDPI(22));
			// 筹码峰/盘口按钮定位到盘口标题栏
			int obTitleH = g_data.RDPI(16);
			int obBtnW = g_data.RDPI(34);
			int obBtnH = min(obTitleH, g_data.RDPI(16));
			int obBtnTop = headerHeight + relatedBarHeight + (obTitleH - obBtnH) / 2;
			bool showObBtns = !isIndexKLine;
			SafeSetWindowPos(m_btnChipPeak, w - obBtnW, obBtnTop, obBtnW, obBtnH);
			SafeShowWindow(m_btnChipPeak, showObBtns);
			// 明细按钮(MX)在 PK 与 CM 中间
			SafeSetWindowPos(m_btnTickDetail, w - obBtnW * 2, obBtnTop, obBtnW, obBtnH);
			SafeShowWindow(m_btnTickDetail, showObBtns);
			SafeSetWindowPos(m_btnOrderBook, w - obBtnW * 3, obBtnTop, obBtnW, obBtnH);
			SafeShowWindow(m_btnOrderBook, showObBtns);
		}

		// 左侧股票列表面板（无论分时数据是否加载都绘制）
		if (m_showStockList)
			m_stockListPanel.Draw(memDC, 0, headerHeight + relatedBarHeight, stockListWidth, h - headerHeight - indexBarHeight - relatedBarHeight, m_stock_id);

		// 集合竞价模式绘制（主图+副图各占一半）
		if (m_viewMode == UI_VIEW_AUCTION)
		{
			// 竞价模式：主图价格和副图成交量各占一半
			int totalChartHeight = priceChartHeight + macdChartHeight + kdjChartHeight + volumeChartHeight;
			int halfChartHeight = totalChartHeight / 2;
			const int titleH = g_data.RDPI(16);
			int origPriceTop = priceChartTop;
			int origVolTop = priceChartTop + halfChartHeight;

			TimelineDrawContext ctx;
			ctx.chartLeft = stockListWidth + yAxisWidth;
			ctx.chartWidth = chartWidth - stockListWidth - yAxisWidth;
			ctx.windowWidth = w;
			ctx.chartHeight = h;
			ctx.priceChartTop = origPriceTop + titleH;
			ctx.priceChartHeight = halfChartHeight - titleH;
			ctx.volumeChartTop = origVolTop + titleH;
			ctx.volumeChartHeight = halfChartHeight - titleH;
			ctx.macdChartTop = origVolTop + titleH;
			ctx.macdChartHeight = halfChartHeight - titleH;
			ctx.positionY = origVolTop + halfChartHeight + g_data.RDPI(2);
			ctx.realtimeData = realtimeData;
			ctx.startIndex = 0;
			ctx.visibleCount = 0;
			ctx.klineData = &klineData;

			// Y轴范围基于集合竞价数据
			STOCK::Price visMax = callAuctionData.matchPrice;
			STOCK::Price visMin = callAuctionData.matchPrice;
			for (const auto& snap : callAuctionData.snapshots)
			{
				if (snap.matchPrice > 0)
				{
					visMax = (std::max)(visMax, snap.matchPrice);
					visMin = (std::min)(visMin, snap.matchPrice);
				}
			}
			STOCK::Price refPrice = callAuctionData.prevClosePrice > 0 ? callAuctionData.prevClosePrice : realtimeData.prevClosePrice;
			if (refPrice > 0)
			{
				if (visMax <= 0) visMax = refPrice;
				if (visMin <= 0 || visMin == visMax) visMin = refPrice;
			}
			if (visMax <= visMin)
			{
				if (refPrice > 0)
				{
					visMax = refPrice * 1.02;
					visMin = refPrice * 0.98;
				}
				else
				{
					visMax = 10.0;
					visMin = 9.8;
				}
			}

			const double DIV_COUNT = 6.0;
			const double MIN_STEP = 0.001;
			double axisMin, axisMax, niceStep;
			CStockIndicator::CalcNiceAxisRange(visMin, visMax, DIV_COUNT, MIN_STEP, axisMin, axisMax, niceStep);
			ctx.maxPrice = axisMax;
			ctx.minPrice = axisMin;
			ctx.niceStep = niceStep;
			ctx.unitY = ctx.priceChartHeight / (ctx.maxPrice - ctx.minPrice);

			std::vector<STOCK::TimelinePoint> emptyTimeline;
			ctx.timelinePoint = &emptyTimeline;
			ctx.fullTimeline = &emptyTimeline;

			memDC.SaveDC();
			memDC.OffsetViewportOrg(stockListWidth + yAxisWidth, 0);

			CTimelineChart::HoverState tlHover;
			tlHover.viewMode = m_viewMode;
			tlHover.isHoveringVolume = m_isHoveringVolume;
			tlHover.hoveredBarIndex = m_hoveredBarIndex;
			tlHover.hoveredData = m_hoveredData;
			tlHover.hoverMa1 = m_hoverMa1; tlHover.hoverMa5 = m_hoverMa5;
			tlHover.hoverMa10 = m_hoverMa10; tlHover.hoverMa20 = m_hoverMa20;
			tlHover.hoverPrevMa1 = m_hoverPrevMa1; tlHover.hoverPrevMa5 = m_hoverPrevMa5;
			tlHover.hoverPrevMa10 = m_hoverPrevMa10; tlHover.hoverPrevMa20 = m_hoverPrevMa20;
			tlHover.hoverTip = m_hoverTip;
			tlHover.timelinePriceTitleTip = m_timelinePriceTitleTip;
			tlHover.timelineVolumeTitleTip = m_timelineVolumeTitleTip;
			tlHover.timelineMacdTitleTip = m_timelineMacdTitleTip;
			tlHover.timelineKdjTitleTip = m_timelineKdjTitleTip;
			tlHover.timelineWrTitleTip = m_timelineWrTitleTip;
			tlHover.timelineRsiTitleTip = m_timelineRsiTitleTip;
			tlHover.showMA = m_showMA;
			tlHover.showBollBands = m_showBollBands;
			tlHover.showTrendView = m_showTrendView;
			tlHover.showChipPeak = m_showChipPeak;
			tlHover.expandedMode = m_expandedMode;
			tlHover.timelineScrollOffset = m_timelineScrollOffset;
			tlHover.timelineVisibleCount = m_timelineVisibleCount;
			tlHover.timelineLastTotalPoints = m_timelineLastTotalPoints;
			tlHover.stockId = m_stock_id;
			tlHover.mousePos = m_mousePos;

			m_timelineChart.DrawTimelineHeader(memDC, ctx, tlHover);
			m_callAuctionChart.Draw(memDC, ctx, callAuctionData);

			// 标题栏+图表内容（竞价模式只有价格图和成交量图）
			m_timelineChart.DrawPriceChartArea(memDC, ctx, origPriceTop, halfChartHeight, tlHover);
			{
				CIndicatorChart::HoverState volHover;
				volHover.isHoveringVolume = m_isHoveringVolume;
				volHover.hoveredBarIndex = m_hoveredBarIndex;
				volHover.viewMode = m_viewMode;
				volHover.timelineVolumeTitleTip = m_timelineVolumeTitleTip;
				m_indicatorChart.DrawVolumeChartArea(memDC, ctx, origVolTop, halfChartHeight, false, volHover);
			}

			memDC.RestoreDC(-1);

			// 主标题栏右侧按钮定位
			{
				int closeBtnW = g_data.RDPI(20);
				int closeBtnH = g_data.RDPI(18);
				int top = g_data.RDPI(2);
				// 顶部按钮统一靠右：分时/5分/30分/日K紧挨股票列表/放大/X按钮
				SafeSetWindowPos(m_btnClose, w - closeBtnW, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnExpand, w - closeBtnW * 2, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnToggleStockList, w - closeBtnW * 3, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnKLine, w - closeBtnW * 3 - g_data.RDPI(40), top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnMin30KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 2, top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnMin5KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 3, top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnTimeLine, w - closeBtnW * 3 - g_data.RDPI(40) * 4, top, g_data.RDPI(40), g_data.RDPI(22));
			}
			// 盘口按钮
			{
				int obTitleH = g_data.RDPI(16);
				int obBtnW = g_data.RDPI(34);
				int obBtnH = min(obTitleH, g_data.RDPI(16));
				int obBtnTop = headerHeight + relatedBarHeight + (obTitleH - obBtnH) / 2;
				bool showObBtns = !isIndexKLine;
				SafeSetWindowPos(m_btnChipPeak, w - obBtnW, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnChipPeak, showObBtns);
				// 明细按钮(MX)在 PK 与 CM 中间
				SafeSetWindowPos(m_btnTickDetail, w - obBtnW * 2, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnTickDetail, showObBtns);
				SafeSetWindowPos(m_btnOrderBook, w - obBtnW * 3, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnOrderBook, showObBtns);
			}
			// 竞价模式隐藏其他工具按钮
			SafeShowWindow(m_btnMA, false);
			SafeShowWindow(m_btnBoll, false);
			SafeShowWindow(m_btnIndicatorMACD, false);
			SafeShowWindow(m_btnZoomOut, false);
			SafeShowWindow(m_btnZoomIn, false);
			SafeShowWindow(m_btnIndicatorCJL, false);
			SafeShowWindow(m_btnIndicatorKDJ, false);
			SafeShowWindow(m_btnIndicatorWR, false);
			SafeShowWindow(m_btnIndicatorRSI, false);

			// 右侧盘口（竞价模式下始终显示盘口）
			if (!isIndexKLine)
			{
				m_orderBookPanel.Draw(memDC, chartWidth, w, h - headerHeight - indexBarHeight - relatedBarHeight, m_orderBook, klineData, m_viewMode);
			}
		}
		else if (!timelinePoint.empty())
		{
			// 先基于完整分时数据计算MA，避免缩放/拖动后只用可见区间导致均线与其他APP不一致
			CStockIndicator::CalcAllRollingAvgPrices(timelinePoint);

			// 计算可见范围：m_timelineVisibleCount控制缩放，m_timelineScrollOffset控制拖动
			int totalPoints = static_cast<int>(timelinePoint.size());
			int visibleCount = min(m_timelineVisibleCount, totalPoints);
			int maxOffset = max(0, totalPoints - visibleCount);
			int prevMaxOffset = max(0, m_timelineLastTotalPoints - visibleCount);
			bool wasAtLatest = (m_timelineScrollOffset < 0 || m_timelineScrollOffset >= prevMaxOffset);

			// 首次显示或更新前就在最新位置时，数据追加后继续自动跟随末尾
			if (wasAtLatest)
				m_timelineScrollOffset = maxOffset;
			m_timelineLastTotalPoints = totalPoints;

			int startIndex = max(0, min(m_timelineScrollOffset, maxOffset));
			// 创建可见范围的子向量
			std::vector<STOCK::TimelinePoint> subTimeline(
				timelinePoint.begin() + startIndex,
				timelinePoint.begin() + startIndex + visibleCount);

			TimelineDrawContext ctx;
			ctx.chartLeft = stockListWidth + yAxisWidth;         // 左侧股票列表+Y轴留白
			ctx.chartWidth = chartWidth - stockListWidth - yAxisWidth;  // 图表宽度（不含股票列表和Y轴区域）
			ctx.windowWidth = w;
			ctx.chartHeight = h;
			// 每个图表顶部预留16像素标题栏，绘图区域下移并减小高度
			const int titleH = g_data.RDPI(16);
			int origPriceTop = priceChartTop;
			int origVolTop = priceChartTop + priceChartHeight;        // 成交量区（紧贴走势图下方）
			int origIndicatorTop = origVolTop + volumeChartHeight;    // MACD指标区
			// KDJ/RSI/W&R图形区域隐藏时（m_showKdjChart=false），该区域高度归0，
			// 空出的位置由MACD图形向下扩展占用；左侧按钮布局仍用原KDJ区域坐标
			int kdjChartHeightEff = m_showKdjChart ? kdjChartHeight : 0;
			int macdChartHeightEff = m_showKdjChart ? macdChartHeight : (macdChartHeight + kdjChartHeight);
			int origKdjTop = origIndicatorTop + macdChartHeightEff;   // KDJ指标区（隐藏时与MACD区底对齐）
			ctx.priceChartTop = origPriceTop + titleH;
			ctx.priceChartHeight = priceChartHeight - titleH;
			ctx.volumeChartTop = origVolTop + titleH;
			ctx.volumeChartHeight = volumeChartHeight - titleH;
			ctx.macdChartTop = origIndicatorTop + titleH;
			ctx.macdChartHeight = macdChartHeightEff - titleH;
			// 时间标签位置：KDJ图下方（隐藏时贴MACD区底）
			ctx.positionY = origKdjTop + kdjChartHeightEff + g_data.RDPI(2);
			ctx.realtimeData = realtimeData;
			ctx.timelinePoint = &subTimeline;
			ctx.fullTimeline = &timelinePoint;  // 完整分时数据，供布林带等指标回溯
			ctx.startIndex = startIndex;
			ctx.visibleCount = visibleCount;
			ctx.xAxisPoints = (m_viewMode >= UI_VIEW_MIN5_KLINE) ? 0 : m_timelineVisibleCount;  // 仅分时模式固定X轴，K线模式动态
			ctx.klineData = &klineData;

			// 技术指标模型：只在序列长度/视图模式变化时重算全部指标并缓存，
			// 重绘/拖动/缩放时渲染层只读缓存，不做指标计算
			// 注意：所有视图模式（含5/30分钟/日K）的副图指标都基于转成分时点的序列计算，
			// 仅 W&R/RSI 在K线模式下使用K线版计算
			{
				bool sizeChanged = (totalPoints != m_lastIndicatorDataSize);
				bool modeChanged = (m_viewMode != m_lastIndicatorViewMode);
				if (!m_indicatorLoaded || sizeChanged || modeChanged || !m_indicatorModel.IsValid())
				{
					m_indicatorLoaded = true;
					m_lastIndicatorDataSize = totalPoints;
					m_lastIndicatorViewMode = m_viewMode;
					m_indicatorModel.UpdateAll(timelinePoint, klineData, m_viewMode);
				}
				ctx.indicatorModel = &m_indicatorModel;
				ctx.overlayKDJInMacd = m_overlayKDJInMacd;
			}

			// 使用完整数据中已计算好的MA值
			if (!subTimeline.empty())
			{
				const auto& lastPt = subTimeline.back();
				ctx.ma1 = lastPt.price;
				ctx.ma5 = lastPt.ma5;
				ctx.ma10 = lastPt.ma10;
				ctx.ma20 = lastPt.ma20;
				// 前一分钟数据用于箭头方向判断
				if (subTimeline.size() >= 2)
				{
					const auto& prevPt = subTimeline[subTimeline.size() - 2];
					ctx.prevMa1 = prevPt.price;
					ctx.prevMa5 = prevPt.ma5;
					ctx.prevMa10 = prevPt.ma10;
					ctx.prevMa20 = prevPt.ma20;
				}
			}

			// 计算整齐Y轴范围：先根据可见数据范围计算整齐步长，再扩展为整齐边界
			// 注意：缩放/拖动后Y轴范围仅基于可见数据，不强制包含昨收价，
			// 这样缩放到局部区域时Y轴步长能正确缩小，走势线始终居中
			{
				STOCK::Price visMax = 0;
				STOCK::Price visMin = (std::numeric_limits<STOCK::Price>::max)();
				for (const auto& tp : subTimeline)
				{
					if (tp.price > 0)
					{
						visMax = (std::max)(visMax, tp.price);
						visMin = (std::min)(visMin, tp.price);
					}
					if (m_viewMode < UI_VIEW_MIN5_KLINE && tp.averagePrice > 0)
					{
						visMax = (std::max)(visMax, tp.averagePrice);
						visMin = (std::min)(visMin, tp.averagePrice);
					}
				}
				// K线模式：Y轴范围需要包含K线柱的high/low
				// 分别纳入high和low，避免low=0时丢失有效的high值，也避免low=0/close=0时visMin被设为0
				if (m_viewMode >= UI_VIEW_MIN5_KLINE && ctx.klineData)
				{
					const auto& klineRef = *ctx.klineData;
					for (int i = 0; i < visibleCount && (startIndex + i) < static_cast<int>(klineRef.size()); i++)
					{
						const auto& kp = klineRef[startIndex + i];
						if (kp.high > 0)
							visMax = (std::max)(visMax, kp.high);
						if (kp.low > 0)
							visMin = (std::min)(visMin, kp.low);
					}
				}

				// 开启BOLL时，Y轴范围同时包含可见区间的布林上下轨，避免窄幅区间下BOLL被映射到价格图区外
				// 上下轨直接读模型缓存序列，不再逐点重算
				if (m_showBollBands && ctx.indicatorModel && !timelinePoint.empty())
				{
					const auto& upperSeq = ctx.indicatorModel->GetBollUpper();
					const auto& lowerSeq = ctx.indicatorModel->GetBollLower();
					const int totalCount = static_cast<int>(timelinePoint.size());
					for (int i = 0; i < visibleCount && (startIndex + i) < totalCount; i++)
					{
						int globalIdx = startIndex + i;
						if (globalIdx >= static_cast<int>(upperSeq.size()))
							continue;
						if (upperSeq[globalIdx] > 0)
							visMax = (std::max)(visMax, upperSeq[globalIdx]);
						if (lowerSeq[globalIdx] > 0)
							visMin = (std::min)(visMin, lowerSeq[globalIdx]);
					}
				}
				// 开启基金净值曲线时，Y轴范围同时包含可见区间的IOPV值，避免净值曲线绘制到图表区外
				if (m_showJZCurve && m_viewMode < UI_VIEW_MIN5_KLINE)
				{
					for (const auto& tp : subTimeline)
					{
						if (tp.iopv > 0)
						{
							visMax = (std::max)(visMax, tp.iopv);
							visMin = (std::min)(visMin, tp.iopv);
						}
					}
				}
				if (visMin == (std::numeric_limits<STOCK::Price>::max)() || visMax <= visMin)
				{
					// 数据无效，回退到涨跌停范围
					STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
					visMax = ctx.realtimeData.prevClosePrice + priceLimit;
					visMin = ctx.realtimeData.prevClosePrice - priceLimit;
				}

				// Y轴等分数：分时保持6等分（7根刻度线）；5/30分钟、日K走势图刻度线多时标签易拥挤，限制为4等分（最多5根刻度线），刻度仍随价格范围自动缩放
				// 先把轴边界对齐到实际显示的价格刻度，再让网格线、标签、曲线共用同一组刻度值，避免标签四舍五入后与曲线位置错位
				const double DIV_COUNT = (m_viewMode >= UI_VIEW_MIN5_KLINE) ? 4.0 : 6.0;
				const double MIN_STEP = 0.001;
				double axisMin, axisMax, niceStep;
				CStockIndicator::CalcNiceAxisRange(visMin, visMax, DIV_COUNT, MIN_STEP, axisMin, axisMax, niceStep);

				ctx.maxPrice = axisMax;
				ctx.minPrice = axisMin;
				ctx.niceStep = niceStep;
				ctx.unitY = ctx.priceChartHeight / (ctx.maxPrice - ctx.minPrice);
			}

			// 使用视口偏移让分时图所有绘制自动向右偏移 stockListWidth + yAxisWidth，实现左侧股票列表和Y轴留白
			memDC.SaveDC();
			memDC.OffsetViewportOrg(stockListWidth + yAxisWidth, 0);

			CTimelineChart::HoverState tlHover;
			tlHover.viewMode = m_viewMode;
			tlHover.isHoveringVolume = m_isHoveringVolume;
			tlHover.hoveredBarIndex = m_hoveredBarIndex;
			tlHover.hoveredData = m_hoveredData;
			tlHover.hoverMa1 = m_hoverMa1; tlHover.hoverMa5 = m_hoverMa5;
			tlHover.hoverMa10 = m_hoverMa10; tlHover.hoverMa20 = m_hoverMa20;
			tlHover.hoverPrevMa1 = m_hoverPrevMa1; tlHover.hoverPrevMa5 = m_hoverPrevMa5;
			tlHover.hoverPrevMa10 = m_hoverPrevMa10; tlHover.hoverPrevMa20 = m_hoverPrevMa20;
			tlHover.hoverTip = m_hoverTip;
			tlHover.timelinePriceTitleTip = m_timelinePriceTitleTip;
			tlHover.timelineVolumeTitleTip = m_timelineVolumeTitleTip;
			tlHover.timelineMacdTitleTip = m_timelineMacdTitleTip;
			tlHover.timelineKdjTitleTip = m_timelineKdjTitleTip;
			tlHover.timelineWrTitleTip = m_timelineWrTitleTip;
			tlHover.timelineRsiTitleTip = m_timelineRsiTitleTip;
			tlHover.showMA = m_showMA;
			tlHover.showBollBands = m_showBollBands;
			tlHover.showTrendView = m_showTrendView;
			tlHover.showChipPeak = m_showChipPeak;
			tlHover.expandedMode = m_expandedMode;
			tlHover.timelineScrollOffset = m_timelineScrollOffset;
			tlHover.timelineVisibleCount = m_timelineVisibleCount;
			tlHover.timelineLastTotalPoints = m_timelineLastTotalPoints;
			tlHover.stockId = m_stock_id;
			tlHover.mousePos = m_mousePos;

			m_timelineChart.DrawTimelineHeader(memDC, ctx, tlHover);
			// 交易日交替底色：5/30分钟K线主图，先于网格线绘制（网格压在底色上不被遮挡）
			m_timelineChart.DrawTradingDayBands(memDC, ctx, tlHover);
			m_timelineChart.DrawTimelineGridAndLines(memDC, ctx, tlHover);
			// 走势图区域（标题栏+图表内容）
			m_timelineChart.DrawPriceChartArea(memDC, ctx, origPriceTop, priceChartHeight, tlHover);
			// 成交量图区域（紧贴走势图下方，标题栏+图表内容+网格）
			if (volumeChartHeight > 0)
			{
				CIndicatorChart::HoverState volHover;
				volHover.isHoveringVolume = m_isHoveringVolume;
				volHover.hoveredBarIndex = m_hoveredBarIndex;
				volHover.viewMode = m_viewMode;
				volHover.timelineVolumeTitleTip = m_timelineVolumeTitleTip;
				m_indicatorChart.DrawVolumeChartArea(memDC, ctx, origVolTop, volumeChartHeight, false, volHover);
			}
			// MACD图区域（标题栏+图表内容+网格）；隐藏KDJ区时向下扩展占据其位置
			{
				CIndicatorChart::HoverState macdHover;
				macdHover.viewMode = m_viewMode;
				macdHover.timelineMacdTitleTip = m_timelineMacdTitleTip;
				macdHover.hoveredBarIndex = m_hoveredBarIndex;
				m_indicatorChart.DrawMacdChartArea(memDC, ctx, origIndicatorTop, macdChartHeightEff, m_timelineMacdTitleTip, macdHover);
			}
			// KDJ/指标切换区域（标题栏+图表内容+网格）；m_showKdjChart为false时区域高度已归0，
			// 空出位置由MACD向下扩展，此分支跳过绘制；左侧KDJ/W&R/RSI按钮始终显示
			if (m_showKdjChart)
			{
				CIndicatorChart::HoverState indicatorHover;
				indicatorHover.isHoveringVolume = m_isHoveringVolume;
				indicatorHover.hoveredBarIndex = m_hoveredBarIndex;
				indicatorHover.viewMode = m_viewMode;
				indicatorHover.timelineKdjTitleTip = m_timelineKdjTitleTip;
				indicatorHover.timelineWrTitleTip = m_timelineWrTitleTip;
				indicatorHover.timelineRsiTitleTip = m_timelineRsiTitleTip;
				indicatorHover.timelineVolumeTitleTip = m_timelineVolumeTitleTip;
				auto indicatorType = static_cast<CIndicatorChart::TimelineIndicator>(m_timelineIndicator);
				m_indicatorChart.DrawIndicatorChartArea(memDC, ctx, origKdjTop, kdjChartHeight, true, indicatorType, indicatorHover);
			}
			// X轴固定时间标签：独立于KDJ区域显示开关，始终绘制在图表底部（每根竖线下方）
			m_indicatorChart.DrawTimeLabels(memDC, ctx);
			m_timelineChart.DrawTimelineHoverOverlay(memDC, ctx, tlHover);

			memDC.RestoreDC(-1);

			// 应用信号颜色到按钮背景
			ApplySignalColors(tlHover.bollSignalColor, tlHover.macdSignalColor, tlHover.kdjSignalColor, tlHover.wrSignalColor, tlHover.rsiSignalColor, tlHover.maSignalColor);

			// 定位缩放按钮（MACD图标题栏最右侧，原始坐标系）
			{
				int zoomBtnW = g_data.RDPI(28);
				int zoomBtnH = g_data.RDPI(16);
				int zoomGap = g_data.RDPI(2);
				int rightEdge = chartWidth;
				int btnTop = origIndicatorTop + (titleH - zoomBtnH) / 2;
				SafeSetWindowPos(m_btnZoomIn, rightEdge - zoomBtnW - zoomGap, btnTop, zoomBtnW, zoomBtnH);
				SafeSetWindowPos(m_btnZoomOut, rightEdge - zoomBtnW * 2 - zoomGap * 2, btnTop, zoomBtnW, zoomBtnH);
			}

			// JZ/MA/BL/CJL/KDJ/W&R/RSI按钮统一布局（左侧Y轴预留区域）
			{
				int btnW = yAxisWidth - g_data.RDPI(4);
				int btnX = stockListWidth + g_data.RDPI(2);
				int btnGap = g_data.RDPI(1);
				// MA/BL/MACD按钮固定按原始MACD区域布局（图形隐藏/扩展不改变按钮位置和大小）
				int macdAreaTop = origIndicatorTop + titleH;
				int macdAreaBottom = origIndicatorTop + macdChartHeight;
				int macdAreaH = max(1, macdAreaBottom - macdAreaTop);
				int macdBtnH = max(g_data.RDPI(16), (macdAreaH - btnGap * 2) / 3);
				int btn1Y = macdAreaTop;
				int btn2Y = btn1Y + macdBtnH + btnGap;
				int btn3Y = btn2Y + macdBtnH + btnGap;
				SafeSetWindowPos(m_btnMA, btnX, btn1Y, btnW, macdBtnH);
				SafeSetWindowPos(m_btnBoll, btnX, btn2Y, btnW, macdBtnH);
				SafeSetWindowPos(m_btnIndicatorMACD, btnX, btn3Y, btnW, macdBtnH);
				// KDJ/W&R/RSI按钮固定在左侧原KDJ区域坐标（图形区域隐藏时按钮仍按原布局显示，
				// 不跟随MACD扩展后的origKdjTop）
				int kdjAreaTop = origIndicatorTop + macdChartHeight + titleH;
				int kdjAreaBottom = origIndicatorTop + macdChartHeight + kdjChartHeight;
				int kdjAreaH = max(1, kdjAreaBottom - kdjAreaTop);
				int kdjBtnH = max(g_data.RDPI(16), (kdjAreaH - btnGap * 2) / 3);
				int btn4Y = kdjAreaTop;
				int btn5Y = btn4Y + kdjBtnH + btnGap;
				int btn6Y = min(btn5Y + kdjBtnH + btnGap, kdjAreaBottom - kdjBtnH);
				SafeShowWindow(m_btnIndicatorCJL, false);
				if (m_expandedMode)
				{
					// 布局用KDJ区域坐标（放大模式下KDJ区域仍在，按钮保持显示）
				}
				SafeSetWindowPos(m_btnIndicatorKDJ, btnX, btn4Y, btnW, kdjBtnH);
				SafeShowWindow(m_btnIndicatorKDJ, true);
				SafeSetWindowPos(m_btnIndicatorWR, btnX, btn5Y, btnW, kdjBtnH);
				SafeShowWindow(m_btnIndicatorWR, true);
				SafeSetWindowPos(m_btnIndicatorRSI, btnX, btn6Y, btnW, kdjBtnH);
				SafeShowWindow(m_btnIndicatorRSI, true);

				// 首次绘制时强制所有按钮正确渲染
				if (!m_indicatorBtnsInitialized)
				{
					m_indicatorBtnsInitialized = true;
				}
				// 强制重绘指标按钮，避免位置变化后按钮不显示
				m_btnIndicatorMACD.Invalidate();
				m_btnIndicatorKDJ.Invalidate();
				m_btnIndicatorWR.Invalidate();
				m_btnIndicatorRSI.Invalidate();
			}

			// 主标题栏右侧按钮定位（关闭按钮、放大按钮、股票列表切换按钮）
			{
				int closeBtnW = g_data.RDPI(20);
				int closeBtnH = g_data.RDPI(18);
				int top = g_data.RDPI(2);
				// 顶部按钮统一靠右：分时/5分/30分/日K紧挨股票列表/放大/X按钮
				SafeSetWindowPos(m_btnClose, w - closeBtnW, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnExpand, w - closeBtnW * 2, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnToggleStockList, w - closeBtnW * 3, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnKLine, w - closeBtnW * 3 - g_data.RDPI(40), top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnMin30KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 2, top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnMin5KLine, w - closeBtnW * 3 - g_data.RDPI(40) * 3, top, g_data.RDPI(40), g_data.RDPI(22));
				SafeSetWindowPos(m_btnTimeLine, w - closeBtnW * 3 - g_data.RDPI(40) * 4, top, g_data.RDPI(40), g_data.RDPI(22));
			}

			// 盘口标题栏右侧按钮定位（筹码峰、盘口按钮）
			{
				int obTitleH = g_data.RDPI(16);
				int obBtnW = g_data.RDPI(34);
				int obBtnH = min(obTitleH, g_data.RDPI(16));
				int obBtnTop = headerHeight + relatedBarHeight + (obTitleH - obBtnH) / 2;
				bool showObBtns = !isIndexKLine;
				SafeSetWindowPos(m_btnChipPeak, w - obBtnW, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnChipPeak, showObBtns);
				// 明细按钮(MX)在 PK 与 CM 中间
				SafeSetWindowPos(m_btnTickDetail, w - obBtnW * 2, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnTickDetail, showObBtns);
				SafeSetWindowPos(m_btnOrderBook, w - obBtnW * 3, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnOrderBook, showObBtns);
			}

			// 右侧盘口高度：不减xAxisLabelHeight（那是左侧走势图的时间标签，右侧不需要）
			if (m_showTickDetail)
			{
				// 明细(MX)模式：在盘口区域绘制最近成交明细
				m_orderBookPanel.DrawTickDetail(memDC, chartWidth, w,
					h - headerHeight - indexBarHeight - relatedBarHeight, m_orderBook);
			}
			else if (m_showChipPeak)
				m_chipPeakPanel.Draw(memDC, chartWidth, w, h - headerHeight - indexBarHeight - relatedBarHeight, realtimeData, chipData, timelinePoint, m_viewMode);
			else
				m_orderBookPanel.Draw(memDC, chartWidth, w, h - headerHeight - indexBarHeight - relatedBarHeight, m_orderBook, klineData, m_viewMode);
		}
		else
		{
			CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&pMiddleLine);
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			memDC.TextOut((chartWidth - memDC.GetTextExtent(loading_state_txt).cx) / 2, headerHeight + g_data.RDPI(10), loading_state_txt);
		}

		// 绘制关联股票栏（标题栏下方）
		{
			int relatedBarY = headerHeight;
			memDC.FillSolidRect(0, relatedBarY, w, relatedBarHeight, RGB(240, 240, 240));
			memDC.SetBkMode(TRANSPARENT);
			m_statusBarPanel.DrawRelatedStatusBar(memDC, w, relatedBarY, singleBarHeight, m_stock_id, m_viewMode);

			// 关联模式时在右侧均幅区域中间绘制竖线分隔
			std::vector<std::wstring> relatedCodes = g_data.GetRelatedStockCodes(m_stock_id);
			bool isRelatedMode = !relatedCodes.empty();
			if (isRelatedMode)
			{
				auto avgData = g_data.GetAvgDiffData(m_stock_id);
				bool showAvgDiff = !(avgData.minVal == 0.0 && avgData.maxVal == 0.0 && avgData.currentVal == 0.0);
				if (showAvgDiff)
				{
					// 与 DrawRelatedStatusBar 内部一致：均幅区域宽 RDPI(128)，起点 w-128-2
					// 竖线画在三等分边界处（1/3、2/3）
					int avgAreaWidth = g_data.RDPI(128);
					int avgAreaX = w - avgAreaWidth - 2;
					const int lineOffsets[] = { avgAreaWidth / 4, avgAreaWidth / 2, avgAreaWidth * 3 / 4 };
					CPen pen(PS_SOLID, 1, RGB(180, 180, 180));
					CPen* pOldPen = memDC.SelectObject(&pen);
					for (int off : lineOffsets)
					{
						int midX = avgAreaX + off;
						memDC.MoveTo(midX, relatedBarY);
						memDC.LineTo(midX, relatedBarY + singleBarHeight);
					}
					memDC.SelectObject(pOldPen);
					pen.DeleteObject();
				}
			}
		}

		// 绘制底部系统状态栏
		{
			int bottomBarY = h - indexBarHeight;
			memDC.FillSolidRect(0, bottomBarY, w, indexBarHeight, RGB(240, 240, 240));
			memDC.SetBkMode(TRANSPARENT);
			m_statusBarPanel.DrawSystemStatusBar(memDC, w, bottomBarY, singleBarHeight);
		}
	} // end if (m_viewMode != UI_VIEW_OVERVIEW)

	if (m_viewMode == UI_VIEW_OVERVIEW)
	{
		const int headerHeight = g_data.RDPI(26);

		// 计算状态栏高度
		CSize textSize = memDC.GetTextExtent(_T("Ay"));
		const int statusBarHeight = textSize.cy + g_data.RDPI(6);

		// 计算大盘指数区域高度
		auto stockCodes = g_data.m_setting_data.m_stock_codes;
		int indexCount = 0;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			for (const auto& code : stockCodes)
			{
				auto stockData = g_data.GetStockData(code);
				if (stockData && stockData->info.is_ok && GetStockPriority(code) < 200)
					indexCount++;
			}
		}
		const int indexSectionHeight = indexCount > 0 ? g_data.RDPI(56) : 0;

		int totalRows = (int)stockCodes.size() - indexCount;
		int totalTableH = headerHeight + totalRows * headerHeight;

		// 可滚动区域 = 总高度 - 表头 - 状态栏 - 指数区域
		int availableHeight = h - headerHeight - statusBarHeight - indexSectionHeight;
		int maxScrollOffset = max(0, totalTableH - availableHeight);

		// 限制滚动偏移
		if (m_vScrollOffset < 0) m_vScrollOffset = 0;
		if (m_vScrollOffset > maxScrollOffset) m_vScrollOffset = maxScrollOffset;

		// 绘制大盘指数区域
		if (indexCount > 0)
		{
			std::vector<std::pair<std::wstring, STOCK::StockInfo>> indices;
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			for (const auto& code : stockCodes)
			{
				auto stockData = g_data.GetStockData(code);
				if (stockData && stockData->info.is_ok && GetStockPriority(code) < 200)
					indices.push_back({ code, stockData->info });
			}
			m_overviewPanel.DrawIndexSection(memDC, 0, headerHeight, w, indices);
		}

		// 绘制表格（从指数区域下方开始，间距3像素在表格外部）
		int tableTop = headerHeight + indexSectionHeight + 3;
		int tableHeight = h - tableTop - statusBarHeight;
		m_overviewPanel.DrawOverviewTable(memDC, 0, tableTop, w, tableHeight, m_vScrollOffset, h, m_overviewRows);
	}

	dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

	memDC.SelectObject(pOldBitmap);
}

BOOL CFloatingWnd::OnEraseBkgnd(CDC* pDC)
{
	return TRUE; // 不擦除背景
}

void CFloatingWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	// 检测双击
	DWORD currentTime = GetTickCount();
	int dx = point.x - m_lastClickPos.x;
	int dy = point.y - m_lastClickPos.y;
	bool isDoubleClick = (currentTime - m_lastClickTime < GetDoubleClickTime()) &&
		(abs(dx) < 4) && (abs(dy) < 4);
	m_lastClickTime = currentTime;
	m_lastClickPos = point;

	// 单击：点击在按钮区域不处理（让按钮自己处理）
	const int btnBarHeight = g_data.RDPI(2) + g_data.RDPI(22);  // 按钮y起始 + 按钮高度
	if (point.y < btnBarHeight)
	{
		// 在按钮区域，不处理，让子控件处理
		return;
	}

	// 左侧股票列表区域的单击切换
	if (m_viewMode != UI_VIEW_OVERVIEW && m_showStockList)
	{
		const int stockListWidth = g_data.RDPI(65);
		const int headerHeight = g_data.RDPI(26);
		const int relatedBarHeight = g_data.RDPI(20);  // 关联股票栏高度
		const int titleH = g_data.RDPI(16);
		const int rowHeight = g_data.RDPI(35);
		const int listTop = headerHeight + relatedBarHeight + titleH + g_data.RDPI(2);

		if (point.x >= 0 && point.x < stockListWidth && point.y >= listTop)
		{
			int rowIndex = (point.y - listTop) / rowHeight;
			std::vector<std::wstring> stockCodes;
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				for (const auto& code : g_data.m_setting_data.m_stock_codes)
				{
					if (GetStockPriority(code) >= 200 && code.find(kHK) != 0)  // 与绘制一致，过滤指数和港股
						stockCodes.push_back(code);
				}
			}
			if (rowIndex >= 0 && rowIndex < (int)stockCodes.size())
			{
				const std::wstring& clickedCode = stockCodes[rowIndex];
				if (clickedCode != m_stock_id)
				{
					SetStockId(clickedCode);
					UpdateModeButtons();
				}
				return;
			}
		}
	}

	// 总览模式下的鼠标点击处理
	if (m_viewMode == UI_VIEW_OVERVIEW)
	{
		// 双击表头：弹出增加股票对话框
		const int tableHeaderHeight = g_data.RDPI(26);
		if (isDoubleClick && point.y >= tableHeaderHeight && point.y < 2 * tableHeaderHeight)
		{
			PostMessage(FWND_MSG_SHOW_ADD_DLG);
			return;
		}

		if (m_overviewRows.empty())
			return;

		for (const auto& rowInfo : m_overviewRows)
		{
			if (rowInfo.code.empty())
				continue;

			if (point.y >= rowInfo.rowY && point.y < rowInfo.rowY + rowInfo.rowH)
			{
				if (point.x >= 0 && point.x < rowInfo.nameColWidth)
				{
					// 单击名称列：切换到走势图
					m_viewMode = UI_VIEW_MIN5_KLINE;
					g_data.SetCurrentViewMode(UI_VIEW_MIN5_KLINE);
					m_showChipPeak = false;
					SetStockId(rowInfo.code);
					UpdateModeButtons();
					UpdatePeriodComboVisibility();
					return;
				}
				else if (rowInfo.deleteBtnStartX > 0 && point.x >= rowInfo.deleteBtnStartX && point.x <= rowInfo.deleteBtnEndX)
				{
					// 点击删除按钮：删除该股票
					auto& stockCodes = g_data.m_setting_data.m_stock_codes;
					auto it = std::find(stockCodes.begin(), stockCodes.end(), rowInfo.code);
					if (it != stockCodes.end())
					{
						stockCodes.erase(it);
						g_data.SaveConfig();

						m_vScrollOffset = 0;
						Invalidate();
						return;
					}
				}
				else if (isDoubleClick)
				{
					// 双击非名称列、非删除按钮列：延迟弹出股票编辑对话框
					m_pendingEditStockCode = rowInfo.code;
					PostMessage(FWND_MSG_SHOW_EDIT_DLG);
					return;
				}
			}
		}
	}

	// 非总览模式下的分时图双击（所有模式都支持）
	if (m_viewMode != UI_VIEW_OVERVIEW && isDoubleClick)
	{
		// 双击MACD指标标题栏（MACD图区顶部标题条，Y轴区域右侧整行）：切换放大/普通模式
		{
			const int yAxisWidth = g_data.RDPI(50);
			const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
			const int headerHeight = g_data.RDPI(26);
			const int relatedBarHeight = g_data.RDPI(20);
			const int xAxisLabelHeight = g_data.RDPI(20);
			const int indexBarHeight = g_data.RDPI(20);
			const int titleH = g_data.RDPI(16);
			CRect clientRect;
			GetClientRect(&clientRect);
			const int chartArea = clientRect.Height() - headerHeight - relatedBarHeight - xAxisLabelHeight - indexBarHeight;
			const int priceChartHeight = m_expandedMode ? chartArea * 3 / 5 : chartArea * 2 / 5;
			const int volumeChartHeight = m_expandedMode ? 0 : chartArea / 5;
			const int macdTitleTop = headerHeight + relatedBarHeight + priceChartHeight + volumeChartHeight;

			if (point.x >= stockListWidth + yAxisWidth && point.x < clientRect.Width() &&
				point.y >= macdTitleTop && point.y < macdTitleTop + titleH)
			{
				OnBnClickedExpandBtn();
				return;
			}
		}

		// 双击Y轴价格区域（MA/BL/MACD等指标按钮上方的左侧Y轴预留区）：切换股票列表显示/隐藏
		{
			// 按钮_X和区域右边界都与OnPaint一致：股票列表显示时Y轴区域整体右移一个列表宽度
			const int yAxisWidth = g_data.RDPI(50);
			const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
			// 与OnPaint布局一致：标题栏+关联股票栏下方是价格图区起点，MA按钮顶 = 指标区起点+标题栏
			const int headerHeight = g_data.RDPI(26);
			const int relatedBarHeight = g_data.RDPI(20);
			const int xAxisLabelHeight = g_data.RDPI(20);
			const int indexBarHeight = g_data.RDPI(20);
			const int titleH = g_data.RDPI(16);
			CRect clientRect;
			GetClientRect(&clientRect);
			const int chartArea = clientRect.Height() - headerHeight - relatedBarHeight - xAxisLabelHeight - indexBarHeight;
			const int priceChartHeight = m_expandedMode ? chartArea * 3 / 5 : chartArea * 2 / 5;
			const int volumeChartHeight = m_expandedMode ? 0 : chartArea / 5;
			const int yAxisTop = headerHeight + relatedBarHeight;
			const int maBtnTop = yAxisTop + priceChartHeight + volumeChartHeight + titleH;

			if (point.x >= stockListWidth && point.x < stockListWidth + yAxisWidth &&
				point.y >= yAxisTop && point.y < maBtnTop)
			{
				OnBnClickedToggleStockListBtn();
				return;
			}
		}

		// 双击位置过滤：仅响应右侧盘口的首行（最高/最低行）和买卖十档区域
		// 行号: 0=最高/最低行, 1~5=卖五~卖一, 6~10=买一~买五
		// 盘口未绘制时（MX明细/筹码峰模式/大盘K线模式）不处理
		bool isIndex = (GetStockPriority(m_stock_id) < 200);
		bool isIndexKLine = isIndex && m_viewMode >= UI_VIEW_MIN5_KLINE;
		if (!isIndexKLine && !m_showTickDetail && !m_showChipPeak)
		{
			int orderBookRow = -1;
			if (m_orderBookPanel.HitTestOrderBook(point, orderBookRow))
			{
				bool isOrderRow = (orderBookRow >= 1 && orderBookRow <= 10);
				bool isAskRow = (orderBookRow >= 1 && orderBookRow <= 5);

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				CEstVolumeDialog dlg(this);
				dlg.SetStockInfo(m_stock_id, isAskRow);
				// 双击时刻 HH:MM（库中无记录时显示/保存该时间）
				{
					time_t nowTime;
					time(&nowTime);
					tm localTm = {};
					localtime_s(&localTm, &nowTime);
					CString nowText;
					nowText.Format(_T("%02d:%02d"), localTm.tm_hour, localTm.tm_min);
					dlg.SetNowTime(nowText);
				}
				if (isOrderRow)
				{
					// 十档买卖行：带入该档信息，口径与盘口 BuildAskRow/BuildBidRow 完全一致：
					// 挂单量 = level.volume/100；累计成交量 ask行取主动买(cumVol.first)、bid行取主动卖(cumVol.second)
					int levelIdx = isAskRow ? (5 - orderBookRow) : (orderBookRow - 6);
					const auto& lv = isAskRow ? m_orderBook.GetAsk(levelIdx) : m_orderBook.GetBid(levelIdx);
					double price = lv.price;
					auto cumVol = m_orderBook.GetCumVol(price);
					long long orderVolume = lv.volume / 100;
					long long cumVolume = isAskRow ? cumVol.first : cumVol.second;
					dlg.SetIsOrderRow(true);
					dlg.SetPrice(price);
					dlg.SetOrderVolume(orderVolume);
					dlg.SetCumVolume(cumVolume);
					dlg.SetEstVolume(orderVolume + cumVolume);
					// 实时列初始值与买入时列相同
					dlg.SetCurOrderVolume(orderVolume);
					dlg.SetCurCumVolume(cumVolume);
					dlg.SetCurEstVolume(orderVolume + cumVolume);
					// 重置按钮数据源：双击时的当前盘口值
					dlg.SetResetInfo(orderVolume, cumVolume);
				}
				else
				{
					// 首行：实时列显示全部十档对侧累计求和（与盘口同单位）
					long long cumBuy = 0, cumSell = 0;
					for (int i = 0; i < 5; i++)
					{
						auto askVol = m_orderBook.GetCumVol(m_orderBook.GetAsk(i).price);
						auto bidVol = m_orderBook.GetCumVol(m_orderBook.GetBid(i).price);
						cumBuy += askVol.first + bidVol.first;
						cumSell += askVol.second + bidVol.second;
					}
					long long curCum = isAskRow ? cumSell : cumBuy;
					dlg.SetCurCumVolume(curCum);
					dlg.SetCurEstVolume(curCum);
				}

				// 查询当天同价格+同方向的历史预估记录：有则回显（总数量/挂单量/累计成交量），无则上方带入的当前值
				{
					CStockDbManager::OrderBookEstimate savedEst;
					int tradeType = isAskRow ? 1 : 0;   // 双击卖盘=卖出, 买盘=买入
					double queryPrice = 0.0;
					if (isOrderRow)
					{
						int levelIdx = isAskRow ? (5 - orderBookRow) : (orderBookRow - 6);
						queryPrice = isAskRow ? m_orderBook.GetAsk(levelIdx).price
							: m_orderBook.GetBid(levelIdx).price;
					}
					else
					{
						queryPrice = m_orderBook.GetCurrentPrice();   // 首行以现价为查询键
					}
					if (queryPrice > 0 &&
						g_data.GetDbManager().LoadLatestOrderBookEstimate(m_stock_id,
							CCommon::get_stock_trade_date(), queryPrice, tradeType, savedEst))
					{
						dlg.SetSavedEstimate(savedEst);
					}
				}

				dlg.DoModal();
				return;
			}
		}
		// 盘口区域外的双击：按顺序切到左侧界面（对称右键；已在最左侧总览界面时不再切换）
		OnLButtonDblClkModeSwitch();
		return;
	}


	// 拖动启动：在图表区域内按下左键开始拖动滚动
	{
		CRect dragRect;
		GetClientRect(&dragRect);
		bool isIdx = (GetStockPriority(m_stock_id) < 200);
		bool isIdxKLine = isIdx && m_viewMode >= UI_VIEW_MIN5_KLINE;
		const int dragOrderBookWidth = isIdxKLine ? 0 : ORDER_BOOK_WIDTH;
		const int dragChartWidth = dragRect.Width() - dragOrderBookWidth;
		const int dragYAxisWidth = g_data.RDPI(50);
		const int dragStockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
		const int dragChartLeft = dragStockListWidth + dragYAxisWidth;
		const int dragHeaderHeight = g_data.RDPI(26) + g_data.RDPI(20);  // 标题栏+关联股票栏
		if (m_viewMode != UI_VIEW_OVERVIEW && point.x >= dragChartLeft && point.x < dragChartWidth && point.y >= dragHeaderHeight)
		{
			// 分时图拖动（5分钟K线模式和日K线模式也使用分时拖动逻辑）
			{
				m_isTimelineDragging = true;
				m_timelineDragStartPos = point;
				m_timelineDragStartOffset = m_timelineScrollOffset;
			}
			SetCapture();
			m_hPrevCursor = SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		}
	}

	// 点击在窗口外部则关闭
	CPoint ptScreen = point;
	ClientToScreen(&ptScreen);
	CRect rcWindow;
	GetWindowRect(rcWindow);
	if (!rcWindow.PtInRect(ptScreen))
	{
		DestroyWindow();
		Stock::Instance().DestroyFloatingWnd();
	}

	CWnd::OnLButtonDown(nFlags, point);
}

void CFloatingWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
	bool wasDragging = m_isTimelineDragging;
	m_isTimelineDragging = false;
	if (wasDragging)
	{
		ReleaseCapture();
		if (m_hPrevCursor)
		{
			SetCursor(m_hPrevCursor);
			m_hPrevCursor = NULL;
		}
		Invalidate();
	}
	CWnd::OnLButtonUp(nFlags, point);
}

// ========== KDJ 指标绘制 ==========
// 注：CalculateKDJ/CalculateTimelineKDJ 已移至 CStockIndicator 类。

// 已移至 CIndicatorChart

// 已移至 CTimelineChart

// 已移至 CIndicatorChart

void CFloatingWnd::SwitchViewMode(UIViewMode viewMode)
{
	// 按目标模式恢复对应默认参数
	switch (viewMode)
	{
	case UI_VIEW_OVERVIEW:
		g_data.SetCurrentViewMode(UI_VIEW_OVERVIEW);
		m_viewMode = UI_VIEW_OVERVIEW;
		m_showChipPeak = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
		break;
	case UI_VIEW_TIMELINE:
		g_data.SetCurrentViewMode(UI_VIEW_TIMELINE);
		SetTimelineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		break;
	case UI_VIEW_MIN5_KLINE:
		g_data.SetCurrentViewMode(UI_VIEW_MIN5_KLINE);
		SetMin5KLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		break;
	case UI_VIEW_MIN30_KLINE:
		g_data.SetCurrentViewMode(UI_VIEW_MIN30_KLINE);
		SetMin30KLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		break;
	default:
		g_data.SetCurrentViewMode(UI_VIEW_DAY_KLINE);
		SetDayKLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		break;
	}
	m_prevKLineViewMode = UI_VIEW_OVERVIEW;  // 顺序切换模式下来源记录不再使用
}

void CFloatingWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	// 右键按界定顺序切到右侧下一个界面：总览 -> 分时 -> 5分 -> 30分 -> 日K；
	// 已在最右侧（日K界面）时不再切换。
	// 注意：枚举中总览与分时之间夹着竞价界面(UI_VIEW_AUCTION)，用显式数组跳过
	static const UIViewMode kModeSeq[] = { UI_VIEW_OVERVIEW, UI_VIEW_TIMELINE,
		UI_VIEW_MIN5_KLINE, UI_VIEW_MIN30_KLINE, UI_VIEW_DAY_KLINE };
	for (size_t i = 0; i < _countof(kModeSeq) - 1; i++)
	{
		if (m_viewMode == kModeSeq[i])
		{
			SwitchViewMode(kModeSeq[i + 1]);
			return;
		}
	}
}

void CFloatingWnd::OnLButtonDblClkModeSwitch()
{
	// 左键双击按界定顺序切到左侧上一个界面，逻辑与右键对称；
	// 已在最左侧（总览界面）时不再切换
	static const UIViewMode kModeSeq[] = { UI_VIEW_OVERVIEW, UI_VIEW_TIMELINE,
		UI_VIEW_MIN5_KLINE, UI_VIEW_MIN30_KLINE, UI_VIEW_DAY_KLINE };
	for (size_t i = 1; i < _countof(kModeSeq); i++)
	{
		if (m_viewMode == kModeSeq[i])
		{
			SwitchViewMode(kModeSeq[i - 1]);
			return;
		}
	}
}

void CFloatingWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	m_mousePos = point;

	// 拖动滚动处理
	if (m_isTimelineDragging)
	{
		int dx = 0;
		if (m_isTimelineDragging)
		{
			dx = point.x - m_timelineDragStartPos.x;
			// 计算可见范围
			std::vector<STOCK::TimelinePoint> timelinePoint;
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = g_data.GetStockData(m_stock_id);
				if (stockData)
				{
					if (m_viewMode == UI_VIEW_DAY_KLINE)
					{
						// 日K线模式：使用日K线数据计算可见范围
						auto klineObj = stockData->getKLineData();
						if (klineObj)
						{
							for (const auto& kp : klineObj->data)
							{
								STOCK::TimelinePoint tp;
								if (kp.day.length() >= 10)
									tp.time = kp.day.substr(5, 5);  // "MM-DD"
								else
									tp.time = kp.day;
								tp.price = kp.close;
								tp.openPrice = kp.open;
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_viewMode == UI_VIEW_MIN5_KLINE)
					{
						// 5分钟K线模式：使用5分钟K线数据计算可见范围
						auto min5KLineObj = stockData->getMin5KLineData();
						if (min5KLineObj)
						{
							for (const auto& kp : min5KLineObj->data)
							{
								STOCK::TimelinePoint tp;
								auto spacePos = kp.day.find(' ');
								if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
									tp.time = kp.day.substr(spacePos + 1, 5);
								else if (kp.day.length() >= 5 && kp.day[2] == ':')
									tp.time = kp.day.substr(0, 5);
								else
									tp.time = kp.day;
								tp.fullTime = kp.day;
								tp.price = kp.close;
								tp.openPrice = kp.open;
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_viewMode == UI_VIEW_MIN30_KLINE)
					{
						// 30分钟K线模式：使用30分钟K线数据计算可见范围
						auto min30KLineObj = stockData->getMin30KLineData();
						if (min30KLineObj)
						{
							for (const auto& kp : min30KLineObj->data)
							{
								STOCK::TimelinePoint tp;
								auto spacePos = kp.day.find(' ');
								if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
									tp.time = kp.day.substr(spacePos + 1, 5);
								else if (kp.day.length() >= 5 && kp.day[2] == ':')
									tp.time = kp.day.substr(0, 5);
								else
									tp.time = kp.day;
								tp.fullTime = kp.day;
								tp.price = kp.close;
								tp.openPrice = kp.open;
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else
					{
						auto timelineData = stockData->getTimelineData();
						if (timelineData)
							timelinePoint = timelineData->data;
					}
				}
			}
			int totalPoints = static_cast<int>(timelinePoint.size());
			int visibleCount = min(m_timelineVisibleCount, totalPoints);
			int maxOffset = max(0, totalPoints - visibleCount);
			// 像素偏移转换为数据点偏移
			CRect clientRect;
			GetClientRect(&clientRect);
			int yAxisW = g_data.RDPI(50);
			int effectiveWidth = clientRect.Width() - yAxisW;
			int pointsDelta = static_cast<int>(dx * static_cast<float>(visibleCount) / effectiveWidth);
			int newOffset = m_timelineDragStartOffset - pointsDelta;
			newOffset = max(0, min(newOffset, maxOffset));
			if (newOffset != m_timelineScrollOffset)
			{
				m_timelineScrollOffset = newOffset;
				Invalidate();
			}
		}
		// 拖动期间不进行 hover 检测，直接返回
		CWnd::OnMouseMove(nFlags, point);
		return;
	}

	CRect rect;
	GetClientRect(&rect);
	bool isIndex = (GetStockPriority(m_stock_id) < 200);
	bool isIndexKLine = isIndex && m_viewMode >= UI_VIEW_MIN5_KLINE;
	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = rect.Width() - orderBookWidth;
	const int yAxisWidth = g_data.RDPI(50);
	const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
	const int chartLeft = stockListWidth + yAxisWidth;
	const int headerHeight = g_data.RDPI(26);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int singleBarHeight = g_data.RDPI(20);
	const int relatedBarHeight = singleBarHeight;  // 关联股票栏高度（1行，位于标题栏下方）
	const int indexBarHeight = singleBarHeight;    // 底部系统状态栏高度（1行）

	// 统一布局：标题栏 + 关联股票栏 + 走势图(2/5) + 成交量(1/5) + MACD(1/5) + KDJ(1/5) + 时间标签 + 底部系统状态栏
	int chartArea = rect.Height() - headerHeight - relatedBarHeight - xAxisLabelHeight - indexBarHeight;
	int priceChartHeight = chartArea * 2 / 5;
	int volumeChartHeight = chartArea / 5;
	int macdChartHeight = chartArea / 5;
	int kdjChartHeight = chartArea / 5;

	m_isHoveringVolume = false;
	int prevHoveredBarIndex = m_hoveredBarIndex;
	m_hoveredBarIndex = -1;

	std::vector<STOCK::TimelinePoint> timelinePoint;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			if (m_viewMode == UI_VIEW_DAY_KLINE)
			{
				// 日K线模式：使用日K线数据
				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					for (const auto& kp : klineObj->data)
					{
						STOCK::TimelinePoint tp;
						if (kp.day.length() >= 10)
							tp.time = kp.day.substr(5, 5);  // "MM-DD"
						else
							tp.time = kp.day;
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_viewMode == UI_VIEW_MIN5_KLINE)
			{
				// 5分钟K线模式：使用5分钟K线数据
				auto min5KLineObj = stockData->getMin5KLineData();
				if (min5KLineObj)
				{
					for (const auto& kp : min5KLineObj->data)
					{
						STOCK::TimelinePoint tp;
						auto spacePos = kp.day.find(' ');
						if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
							tp.time = kp.day.substr(spacePos + 1, 5);
						else if (kp.day.length() >= 5 && kp.day[2] == ':')
							tp.time = kp.day.substr(0, 5);
						else
							tp.time = kp.day;
						tp.fullTime = kp.day;
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_viewMode == UI_VIEW_MIN30_KLINE)
			{
				// 30分钟K线模式：使用30分钟K线数据
				auto min30KLineObj = stockData->getMin30KLineData();
				if (min30KLineObj)
				{
					for (const auto& kp : min30KLineObj->data)
					{
						STOCK::TimelinePoint tp;
						auto spacePos = kp.day.find(' ');
						if (spacePos != std::string::npos && kp.day.length() > spacePos + 5)
							tp.time = kp.day.substr(spacePos + 1, 5);
						else if (kp.day.length() >= 5 && kp.day[2] == ':')
							tp.time = kp.day.substr(0, 5);
						else
							tp.time = kp.day;
						tp.fullTime = kp.day;
						tp.price = kp.close;
						tp.openPrice = kp.open;
						tp.averagePrice = kp.close;
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else
			{
				auto timelineData = stockData->getTimelineData();
				if (timelineData)
				{
					timelinePoint = timelineData->data;
				}
			}
		}
	}

	if (!timelinePoint.empty() && point.x >= chartLeft && point.x < chartWidth)
	{
		// 计算可见范围（与OnPaint一致）
		int totalPoints = static_cast<int>(timelinePoint.size());
		int visibleCount = min(m_timelineVisibleCount, totalPoints);
		int maxOffset = max(0, totalPoints - visibleCount);
		// 自动跟随：如果当前在末尾或需要自动滚动
		if (m_timelineScrollOffset < 0 || m_timelineScrollOffset >= maxOffset)
			m_timelineScrollOffset = maxOffset;
		int startIndex = max(0, min(m_timelineScrollOffset, maxOffset));

		// 构建可见子向量并计算MA值（与OnPaint一致）
		CStockIndicator::CalcAllRollingAvgPrices(timelinePoint);
		auto subStart = timelinePoint.begin() + startIndex;
		auto subEnd = timelinePoint.begin() + startIndex + visibleCount;
		std::vector<STOCK::TimelinePoint> subTimeline(subStart, subEnd);

		// 鼠标坐标减去图表左边界，对应到分时图内部坐标
		int adjX = point.x - chartLeft;
		int effectiveWidth = chartWidth - chartLeft;
		// 按索引比例计算鼠标对应的可见数据索引
		// 分时模式X轴基于m_timelineVisibleCount固定格数，K线模式基于实际数据点数
		int xSlotCount = (m_viewMode >= UI_VIEW_MIN5_KLINE) ? visibleCount : m_timelineVisibleCount;
		int relIndex = static_cast<int>(adjX * static_cast<float>(xSlotCount) / effectiveWidth);
		relIndex = max(0, min(relIndex, visibleCount - 1));

		if (relIndex >= 0 && relIndex < static_cast<int>(subTimeline.size()))
		{
			m_isHoveringVolume = true;
			m_hoveredBarIndex = relIndex;
			m_hoveredData = subTimeline[relIndex];

			// 保存hover点的MA值
			m_hoverMa1 = m_hoveredData.price;
			m_hoverMa5 = m_hoveredData.ma5;
			m_hoverMa10 = m_hoveredData.ma10;
			m_hoverMa20 = m_hoveredData.ma20;
			// 保存前一点MA值（用于箭头方向）
			m_hoverPrevMa1 = 0; m_hoverPrevMa5 = 0; m_hoverPrevMa10 = 0; m_hoverPrevMa20 = 0;
			if (relIndex > 0)
			{
				m_hoverPrevMa1 = subTimeline[relIndex - 1].price;
				m_hoverPrevMa5 = subTimeline[relIndex - 1].ma5;
				m_hoverPrevMa10 = subTimeline[relIndex - 1].ma10;
				m_hoverPrevMa20 = subTimeline[relIndex - 1].ma20;
			}

			CString timeStr(m_hoveredData.time.c_str());
			STOCK::Volume volumeLots = m_hoveredData.volume / 100;
			CString volumeStr = CCommon::FormatVolumeInt(volumeLots);

			double amount = static_cast<double>(m_hoveredData.volume) * m_hoveredData.price;
			CString amountStr = CCommon::FormatAmount(amount);

			m_hoverTip.Format(_T("%s %s %s"), timeStr, volumeStr, amountStr);
			// 设置量柱图标题栏悬停提示：显示鼠标指向位置的分量和分额
			m_timelineVolumeTitleTip.Format(_T("分量:%s 分额:%s"), volumeStr, amountStr);

			// 设置MACD/KDJ/W&R/RSI标题栏悬停提示
			// MACD固定显示，始终计算悬停提示（用完整数据确保EMA收敛）
			{
				int shortP = 12, longP = 26, signalP = 9;
				if (m_viewMode == UI_VIEW_MIN5_KLINE)
				{
					shortP = 7; longP = 15; signalP = 5;
				}
				else if (m_viewMode == UI_VIEW_TIMELINE)
				{
					shortP = 6; longP = 12; signalP = 4;
				}
				auto macdData = CStockIndicator::CalculateTimelineMACD(timelinePoint, shortP, longP, signalP);
				int globalIdx = startIndex + relIndex;
				if (globalIdx < static_cast<int>(macdData.size()) && macdData[globalIdx].valid)
				{
					auto formatMACDValue = [](double val) -> CString {
						CString s;
						double absVal = std::abs(val);
						if (absVal < 0.001 && absVal > 0)
							s.Format(_T("%.5f"), val);
						else if (absVal < 0.01)
							s.Format(_T("%.4f"), val);
						else
							s.Format(_T("%.3f"), val);
						return s;
						};
					m_timelineMacdTitleTip.Format(_T("DIF:%s DEA:%s"), formatMACDValue(macdData[globalIdx].dif), formatMACDValue(macdData[globalIdx].dea));
				}
			}
			if (m_timelineIndicator == TimelineIndicator::KDJ)
			{
				// 5分钟K线用8,3,3参数，分时(1分钟)用7,3,3参数，30分钟和日K用默认9,3,3
				int kdjN = 9, kdjM1 = 3, kdjM2 = 3;
				if (m_viewMode == UI_VIEW_MIN5_KLINE)
				{
					kdjN = 8; kdjM1 = 3; kdjM2 = 3;
				}
				else if (m_viewMode == UI_VIEW_TIMELINE)
				{
					kdjN = 7; kdjM1 = 3; kdjM2 = 3;
				}
				auto kdjData = CStockIndicator::CalculateTimelineKDJ(subTimeline, kdjN, kdjM1, kdjM2);
				if (relIndex < static_cast<int>(kdjData.size()) && kdjData[relIndex].valid)
				{
					m_timelineKdjTitleTip.Format(_T("K:%.1f D:%.1f J:%.1f"), kdjData[relIndex].k, kdjData[relIndex].d, kdjData[relIndex].j);
				}
				m_timelineWrTitleTip.Empty();
				m_timelineRsiTitleTip.Empty();
			}
			else if (m_timelineIndicator == TimelineIndicator::WR)
			{
				// WR悬停提示
				auto wrData = CStockIndicator::CalculateTimelineWR(subTimeline);
				if (relIndex < static_cast<int>(wrData.size()) && wrData[relIndex].valid)
				{
					m_timelineWrTitleTip.Format(_T("WR6:%.1f WR14:%.1f"), wrData[relIndex].wr1, wrData[relIndex].wr2);
				}
				m_timelineKdjTitleTip.Empty();
				m_timelineRsiTitleTip.Empty();
			}
			else if (m_timelineIndicator == TimelineIndicator::RSI)
			{
				auto rsiData = CStockIndicator::CalculateTimelineRSI(subTimeline);
				if (relIndex < static_cast<int>(rsiData.size()) && rsiData[relIndex].valid)
				{
					m_timelineRsiTitleTip.Format(_T("RSI6:%.1f RSI14:%.1f"), rsiData[relIndex].rsi1, rsiData[relIndex].rsi2);
				}
				m_timelineKdjTitleTip.Empty();
				m_timelineWrTitleTip.Empty();
			}
			else
			{
				m_timelineKdjTitleTip.Empty();
				m_timelineWrTitleTip.Empty();
				m_timelineRsiTitleTip.Empty();
			}
		}
		else
		{
			m_isHoveringVolume = false;
			m_hoveredBarIndex = -1;
			m_hoverTip.Empty();
			m_timelineVolumeTitleTip.Empty();
			m_timelineMacdTitleTip.Empty();
			m_timelineKdjTitleTip.Empty();
			m_timelineWrTitleTip.Empty();
			m_timelineRsiTitleTip.Empty();
			m_hoverMa1 = 0; m_hoverMa5 = 0; m_hoverMa10 = 0; m_hoverMa20 = 0;
			m_hoverPrevMa1 = 0; m_hoverPrevMa5 = 0; m_hoverPrevMa10 = 0; m_hoverPrevMa20 = 0;
		}

		// 只在悬停状态变化时重绘图表区域，避免按钮闪烁
		bool hoverChanged = (m_isHoveringVolume != (prevHoveredBarIndex >= 0) ||
			m_hoveredBarIndex != prevHoveredBarIndex);
		if (hoverChanged)
		{
			InvalidateRect(CRect(0, headerHeight, chartWidth, rect.Height()));
		}
	}
}

void CFloatingWnd::SetStockId(const std::wstring& stockId)
{
	if (m_stock_id == stockId)
		return;
	m_stock_id = stockId;
	// 通知获取线程切换关注股票，线程自动重置计时器并立即获取新股数据
	CStockFetchThread::Instance().SetFocusStockId(m_stock_id);
	m_timelineScrollOffset = -1;
	// 切换股票后强制重算技术指标模型缓存
	m_indicatorLoaded = false;
	m_lastIndicatorDataSize = -1;
	// 切换股票时重置可见点数为当前模式的默认值，避免旧值导致新股票数据显示异常
	if (m_viewMode == UI_VIEW_DAY_KLINE)
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1DAY;
	else if (m_viewMode == UI_VIEW_MIN5_KLINE)
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_5MIN;
	else if (m_viewMode == UI_VIEW_MIN30_KLINE)
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_30MIN;
	else
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1MIN;
		// 分时模式下，根据新股票是否为基金自动切换净值曲线显示
		m_showJZCurve = CCommon::IsFundCode(m_stock_id);
	}
	Invalidate();
}

void CFloatingWnd::ToggleKLineMode()
{
	m_viewMode = (m_viewMode == UI_VIEW_DAY_KLINE) ? UI_VIEW_TIMELINE : UI_VIEW_DAY_KLINE;
	m_showBollBands = (m_viewMode != UI_VIEW_DAY_KLINE);
	m_btnBoll.SetWindowText(_T("BL"));
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	m_timelineVisibleCount = 30;  // 切回分时显示最新走势
	m_showTrendView = false;
	m_showChipPeak = (m_viewMode == UI_VIEW_DAY_KLINE);
	m_showMA = (m_viewMode == UI_VIEW_DAY_KLINE);
	m_showJZCurve = (m_viewMode == UI_VIEW_TIMELINE) && CCommon::IsFundCode(m_stock_id);  // 分时+基金默认显示净值
	ResetHoverState();
	UpdateModeButtons();
	UpdatePeriodComboVisibility();

	if (m_viewMode == UI_VIEW_DAY_KLINE)
	{
		EnsureChipPeakData();
	}
	Invalidate();
}

void CFloatingWnd::UpdateModeButtons()
{
	if (m_btnTimeLine.GetSafeHwnd() && m_btnKLine.GetSafeHwnd())
	{
		if (m_viewMode == UI_VIEW_DAY_KLINE)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
		}
		else if (m_viewMode == UI_VIEW_MIN5_KLINE)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
		}
		else if (m_viewMode == UI_VIEW_MIN30_KLINE)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
		}
		else
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
		}
		// 分时/5分/30分/日K为自绘按钮，当前界面背景由OnDrawItem用浅蓝色高亮，
		// 不再设置pushbutton样式（会覆盖BS_OWNERDRAW导致按钮不显示）
		m_btnTimeLine.Invalidate();
		m_btnMin5KLine.Invalidate();
		m_btnMin30KLine.Invalidate();
		m_btnKLine.Invalidate();

		if (m_btnMA.GetSafeHwnd()) m_btnMA.Invalidate();
		if (m_btnBoll.GetSafeHwnd()) m_btnBoll.Invalidate();
		SafeSetButtonStyle(m_btnIndicatorMACD, m_showKdjChart ? BS_DEFPUSHBUTTON : BS_FLAT);
		if (m_btnIndicatorMACD.GetSafeHwnd()) m_btnIndicatorMACD.Invalidate();
		SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnTickDetail, m_showTickDetail ? BS_DEFPUSHBUTTON : BS_FLAT);

		SafeSetButtonStyle(m_btnExpand, m_expandedMode ? BS_FLAT : BS_DEFPUSHBUTTON);
		m_btnExpand.SetWindowText(m_expandedMode ? _T("△") : _T("▽"));

		m_btnToggleStockList.SetWindowText(m_showStockList ? _T("|>") : _T("<|"));
		SafeSetButtonStyle(m_btnToggleStockList, m_showStockList ? BS_FLAT : BS_DEFPUSHBUTTON);
		SafeShowWindow(m_btnToggleStockList, m_viewMode != UI_VIEW_OVERVIEW);

		// 缩放按钮在所有模式下显示（除总览模式外）
		SafeShowWindow(m_btnZoomOut, m_viewMode != UI_VIEW_OVERVIEW);
		SafeShowWindow(m_btnZoomIn, m_viewMode != UI_VIEW_OVERVIEW);
		// KDJ/WR/RSI/MACD/MA/BL按钮在所有模式下显示（除总览模式外）；
		// 放大模式只是隐藏副图，左侧按钮区域布局正确，不再隐藏按钮，CJL按钮已移除
		bool showIndicatorBtns = m_viewMode != UI_VIEW_OVERVIEW;
		SafeShowWindow(m_btnIndicatorCJL, false);
		SafeShowWindow(m_btnIndicatorMACD, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorKDJ, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorWR, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorRSI, showIndicatorBtns);
		// MA/BL按钮与指标按钮同区域
		SafeShowWindow(m_btnMA, showIndicatorBtns);
		SafeShowWindow(m_btnBoll, showIndicatorBtns);
	}
}

void CFloatingWnd::UpdatePeriodComboVisibility()
{
	// MA/BL按钮与指标按钮同区域，放大模式下保持显示
	bool showIndicatorBtns = m_viewMode != UI_VIEW_OVERVIEW;
	SafeShowWindow(m_btnMA, showIndicatorBtns);

	if (m_btnMin5KLine.GetSafeHwnd())
	{
		// 5分按钮作为主模式按钮之一，始终显示
		m_btnMin5KLine.ShowWindow(SW_SHOW);
	}

	if (m_btnMin30KLine.GetSafeHwnd())
	{
		// 30分按钮作为主模式按钮之一，始终显示
		m_btnMin30KLine.ShowWindow(SW_SHOW);
	}

	SafeShowWindow(m_btnBoll, showIndicatorBtns);
	SafeShowWindow(m_btnChipPeak, m_viewMode != UI_VIEW_OVERVIEW);
	SafeShowWindow(m_btnTickDetail, m_viewMode != UI_VIEW_OVERVIEW);
	SafeShowWindow(m_btnOrderBook, m_viewMode != UI_VIEW_OVERVIEW);
}

BOOL CFloatingWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 分时图模式/5分钟K线模式/日K线模式：滚轮缩放可见数据点数
	if (m_viewMode != UI_VIEW_OVERVIEW)
	{
		int minVisible;              // 最大放大倍率：与"+"按钮一致
		int maxVisible;              // 最小缩放上限：根据模式不同
		if (m_viewMode == UI_VIEW_DAY_KLINE)
		{
			minVisible = TIME_LINE_VISIBLE_COUNT_1DAY;
			maxVisible = 750;        // 日K线：最多显示约3年
		}
		else if (m_viewMode == UI_VIEW_MIN5_KLINE)
		{
			minVisible = TIME_LINE_VISIBLE_COUNT_5MIN;
			maxVisible = 480;        // 5分K线
		}
		else if (m_viewMode == UI_VIEW_MIN30_KLINE)
		{
			minVisible = TIME_LINE_VISIBLE_COUNT_30MIN;
			maxVisible = 480;        // 30分K线
		}
		else
		{
			minVisible = TIME_LINE_VISIBLE_COUNT_1MIN;
			maxVisible = 240;        // 分时：1天240分钟
		}
		// 获取实际数据点数
		int totalPoints = 0;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = g_data.GetStockData(m_stock_id);
			if (stockData)
			{
				if (m_viewMode == UI_VIEW_DAY_KLINE)
				{
					auto klineObj = stockData->getKLineData();
					if (klineObj)
						totalPoints = static_cast<int>(klineObj->data.size());
				}
				else if (m_viewMode == UI_VIEW_MIN5_KLINE)
				{
					auto min5KLineObj = stockData->getMin5KLineData();
					if (min5KLineObj)
						totalPoints = static_cast<int>(min5KLineObj->data.size());
				}
				else if (m_viewMode == UI_VIEW_MIN30_KLINE)
				{
					auto min30KLineObj = stockData->getMin30KLineData();
					if (min30KLineObj)
						totalPoints = static_cast<int>(min30KLineObj->data.size());
				}
				else
				{
					auto timelineData = stockData->getTimelineData();
					if (timelineData)
						totalPoints = static_cast<int>(timelineData->data.size());
				}
			}
		}
		int effectiveMax = min(maxVisible, max(totalPoints, minVisible));
		int newCount = m_timelineVisibleCount;
		if (zDelta > 0)
		{
			// 向上滚：放大（减少可见点数，逐步缩小，最终倍率与"+"按钮一致）
			newCount = max(minVisible, m_timelineVisibleCount - TIME_LINE_VISIBLE_COUNT_STEP);
		}
		else
		{
			// 向下滚：缩小（增加可见点数）
			newCount = min(effectiveMax, m_timelineVisibleCount + TIME_LINE_VISIBLE_COUNT_STEP);
		}
		if (newCount != m_timelineVisibleCount)
		{
			// 计算鼠标位置对应的全局数据索引（以鼠标为中心缩放）
			CRect clientRect;
			GetClientRect(&clientRect);
			ScreenToClient(&pt);
			const int yAxisWidth = g_data.RDPI(50);
			const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
			const int chartLeft = stockListWidth + yAxisWidth;
			const int orderBookWidth = ORDER_BOOK_WIDTH;
			int chartWidth = clientRect.Width() - orderBookWidth;
			int adjX = pt.x - chartLeft;
			int effectiveWidth = chartWidth - chartLeft;

			// 鼠标在可见区域中的比例位置
			float ratio = 0.5f;
			if (effectiveWidth > 0 && adjX >= 0 && adjX < effectiveWidth)
			{
				ratio = static_cast<float>(adjX) / effectiveWidth;
			}
			else if (adjX >= effectiveWidth)
			{
				ratio = 1.0f;
			}

			// 鼠标对应的全局数据索引
			int mouseGlobalIndex = m_timelineScrollOffset + static_cast<int>(ratio * m_timelineVisibleCount);

			// 新的 scrollOffset 应使鼠标位置对应的数据索引在缩放后仍处于相同比例位置
			int newOffset = mouseGlobalIndex - static_cast<int>(ratio * newCount);
			int maxOffset = max(0, totalPoints - newCount);
			newOffset = max(0, min(newOffset, maxOffset));

			m_timelineVisibleCount = newCount;
			m_timelineScrollOffset = newOffset;
			Invalidate();
		}
		return TRUE;
	}

	if (m_viewMode != UI_VIEW_OVERVIEW)
	{
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	}

	const int headerHeight = g_data.RDPI(26);

	auto stockCodes = g_data.m_setting_data.m_stock_codes;
	int totalRows = (int)stockCodes.size();
	if (totalRows == 0)
	{
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	}

	int totalTableH = headerHeight + totalRows * headerHeight;

	// 计算状态栏高度
	CDC* pDC = GetDC();
	if (!pDC)
	{
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	}
	CSize textSize = pDC->GetTextExtent(_T("Ay"));
	ReleaseDC(pDC);

	const int statusBarHeight = textSize.cy + g_data.RDPI(6);

	// 可滚动区域 = 总高度 - 表头 - 状态栏
	CRect rect;
	GetClientRect(&rect);
	int availableHeight = rect.Height() - headerHeight - statusBarHeight;
	int maxScrollOffset = max(0, totalTableH - availableHeight);

	if (maxScrollOffset == 0)
	{
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	}

	int newPos = m_vScrollOffset;

	if (zDelta > 0)
	{
		newPos -= headerHeight;
	}
	else
	{
		newPos += headerHeight;
	}

	newPos = max(0, min(newPos, maxScrollOffset));

	if (newPos != m_vScrollOffset)
	{
		m_vScrollOffset = newPos;
		Invalidate();
		return TRUE;
	}

	return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

void CFloatingWnd::OnBnClickedTimeLineBtn()
{
	g_data.SetCurrentViewMode(UI_VIEW_TIMELINE);
	if (m_viewMode != UI_VIEW_TIMELINE)
	{
		SetTimelineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedKLineBtn()
{
	g_data.SetCurrentViewMode(UI_VIEW_DAY_KLINE);
	if (m_viewMode != UI_VIEW_DAY_KLINE)
	{
		SetDayKLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedIndicatorMACDSignalBtn()
{
	// 可见的MACD按钮（IDC_INDICATOR_MACD_SIGNAL_BTN）：点击切换下方KDJ/WR/RSI指标
	// 图形区域的显示/隐藏（默认隐藏，点击显示，再点隐藏；左侧KDJ/WR/RSI按钮始终显示，只隐藏图形区域）
	m_showKdjChart = !m_showKdjChart;
	SafeSetButtonStyle(m_btnIndicatorMACD, m_showKdjChart ? BS_DEFPUSHBUTTON : BS_FLAT);
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
	m_timelineWrTitleTip.Empty();
	m_timelineRsiTitleTip.Empty();
	if (!m_showKdjChart)
		ResetHoverState();
	Invalidate();
}

void CFloatingWnd::OnBnClickedChipPeakBtn()
{
	// 切到筹码峰模式：退出明细模式
	m_showTickDetail = false;
	m_showChipPeak = !m_showChipPeak;
	SafeSetButtonStyle(m_btnTickDetail, m_showTickDetail ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);

	EnsureChipPeakData();
	Invalidate();
}

void CFloatingWnd::OnBnClickedOrderBookBtn()
{
	// 切到盘口模式：退出明细模式
	m_showTickDetail = false;
	m_showChipPeak = !m_showChipPeak;
	SafeSetButtonStyle(m_btnTickDetail, m_showTickDetail ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);

	EnsureChipPeakData();
	Invalidate();
}

void CFloatingWnd::OnBnClickedTickDetailBtn()
{
	// 明细(MX)模式：切走时保留原PK/CM状态，进入时显示成交明细
	m_showTickDetail = !m_showTickDetail;
	SafeSetButtonStyle(m_btnTickDetail, m_showTickDetail ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnChipPeak, (!m_showTickDetail && m_showChipPeak) ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnOrderBook, (!m_showTickDetail && !m_showChipPeak) ? BS_DEFPUSHBUTTON : BS_FLAT);
	Invalidate();
}

void CFloatingWnd::OnBnClickedExpandBtn()
{
	m_expandedMode = !m_expandedMode;
	m_btnExpand.SetWindowText(m_expandedMode ? _T("△") : _T("▽"));
	SafeSetButtonStyle(m_btnExpand, m_expandedMode ? BS_FLAT : BS_DEFPUSHBUTTON);
	Invalidate();
}

void CFloatingWnd::OnBnClickedToggleStockListBtn()
{
	m_showStockList = !m_showStockList;
	m_btnToggleStockList.SetWindowText(m_showStockList ? _T("|>") : _T("<|"));
	SafeSetButtonStyle(m_btnToggleStockList, m_showStockList ? BS_FLAT : BS_DEFPUSHBUTTON);
	UpdateModeButtons();
	Invalidate();
	// 强制重绘指标按钮，避免位置变化后按钮不显示
	if (!m_expandedMode)
	{
		m_btnIndicatorKDJ.Invalidate();
		m_btnIndicatorWR.Invalidate();
		m_btnIndicatorRSI.Invalidate();
	}
}

void CFloatingWnd::SafeSetWindowPos(CWnd& wnd, int x, int y, int cx, int cy)
{
	if (!wnd.GetSafeHwnd()) return;
	CRect curRect;
	wnd.GetWindowRect(&curRect);
	wnd.ScreenToClient(&curRect);
	// GetWindowRect返回屏幕坐标，需要转换
	CRect parentRect;
	CWnd* parent = wnd.GetParent();
	if (parent)
	{
		parent->ScreenToClient(&curRect);
	}
	if (curRect.left != x || curRect.top != y || curRect.Width() != cx || curRect.Height() != cy)
	{
		// 不用SWP_NOREDRAW：按钮位置变化后需要立即重绘，
		// 否则旧位置残留、新位置空白（尤其按钮移出主标题栏区域后，
		// 局部重绘不会覆盖按钮区域，残影更明显）
		wnd.SetWindowPos(nullptr, x, y, cx, cy, SWP_NOZORDER);
		wnd.Invalidate();
	}
}

void CFloatingWnd::SafeShowWindow(CWnd& wnd, bool show)
{
	if (!wnd.GetSafeHwnd()) return;
	bool curVisible = wnd.IsWindowVisible() != FALSE;
	if (curVisible != show)
	{
		wnd.ShowWindow(show ? SW_SHOW : SW_HIDE);
	}
}

void CFloatingWnd::SafeSetButtonStyle(CButton& btn, UINT style)
{
	if (!btn.GetSafeHwnd()) return;
	// GetButtonStyle返回的低8位是按钮样式
	UINT curStyle = btn.GetButtonStyle() & 0xFF;
	if (curStyle != (style & 0xFF))
	{
		btn.SetButtonStyle(style, TRUE);
	}
}

HBRUSH CFloatingWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CFloatingWnd::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct->CtlType != ODT_BUTTON) return;
	UINT nID = lpDrawItemStruct->CtlID;

	// 获取信号颜色
	COLORREF signalColor = CLR_INVALID;
	bool isActive = false;  // 按钮是否为当前选中状态
	bool isModeBtn = false; // 主模式按钮（分时/5分/30分/日K），当前界面时用浅蓝高亮
	if (nID == IDC_BOLL_BTN) { signalColor = m_bollSignalColor; isActive = m_showBollBands; }
	else if (nID == IDC_MA_BTN) { signalColor = m_maSignalColor; isActive = m_showMA; }
	else if (nID == IDC_INDICATOR_MACD_SIGNAL_BTN) { signalColor = m_macdSignalColor; isActive = (m_timelineIndicator == TimelineIndicator::MACD); }
	else if (nID == IDC_INDICATOR_KDJ_BTN) { signalColor = m_kdjSignalColor; isActive = (m_timelineIndicator == TimelineIndicator::KDJ); }
	else if (nID == IDC_INDICATOR_WR_BTN) { signalColor = m_wrSignalColor; isActive = (m_timelineIndicator == TimelineIndicator::WR); }
	else if (nID == IDC_INDICATOR_RSI_BTN) { signalColor = m_rsiSignalColor; isActive = (m_timelineIndicator == TimelineIndicator::RSI); }
	// 主模式按钮：当前界面用浅蓝色背景高亮，帮助快速识别所处界面
	else if (nID == IDC_TIMELINE_BTN) { isModeBtn = true; isActive = (m_viewMode == UI_VIEW_TIMELINE || m_viewMode == UI_VIEW_AUCTION); }
	else if (nID == IDC_MIN5_KLINE_BTN) { isModeBtn = true; isActive = (m_viewMode == UI_VIEW_MIN5_KLINE); }
	else if (nID == IDC_MIN30_KLINE_BTN) { isModeBtn = true; isActive = (m_viewMode == UI_VIEW_MIN30_KLINE); }
	else if (nID == IDC_KLINE_BTN) { isModeBtn = true; isActive = (m_viewMode == UI_VIEW_DAY_KLINE); }

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	CRect rect = lpDrawItemStruct->rcItem;

	// 判断按钮状态
	bool isSelected = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;

	// 背景色：有信号时用信号颜色；主模式按钮当前界面用浅蓝色高亮；
	// 其他按钮选中用浅灰，未选中用更浅灰
	COLORREF bgColor;
	if (signalColor != CLR_INVALID)
		bgColor = signalColor;
	else if (isModeBtn && isActive)
		bgColor = RGB(200, 225, 255);   // 当前界面主模式按钮：浅蓝
	else if (isActive)
		bgColor = RGB(225, 225, 225);
	else
		bgColor = RGB(245, 245, 245);

	// 按下时稍微变暗
	if (isSelected)
	{
		int r = max(0, GetRValue(bgColor) - 30);
		int g = max(0, GetGValue(bgColor) - 30);
		int b = max(0, GetBValue(bgColor) - 30);
		bgColor = RGB(r, g, b);
	}

	// 填充背景
	dc.FillSolidRect(rect, bgColor);

	// 绘制边框：选中状态用深色边框
	COLORREF borderColor = isActive ? RGB(0, 0, 128) : RGB(180, 180, 180);
	CPen pen(PS_SOLID, 1, borderColor);
	CPen* pOldPen = dc.SelectObject(&pen);
	dc.MoveTo(rect.left, rect.bottom - 1);
	dc.LineTo(rect.left, rect.top);
	dc.LineTo(rect.right - 1, rect.top);
	dc.LineTo(rect.right - 1, rect.bottom - 1);
	dc.LineTo(rect.left, rect.bottom - 1);
	dc.SelectObject(pOldPen);

	// 绘制文字
	CString text;
	GetDlgItemText(nID, text);
	dc.SetBkMode(TRANSPARENT);
	// 有信号颜色时文字用白色，否则用黑色
	dc.SetTextColor(signalColor != CLR_INVALID ? RGB(255, 255, 255) : RGB(0, 0, 0));
	dc.DrawText(text, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.Detach();
}

void CFloatingWnd::ApplySignalColors(COLORREF bollColor, COLORREF macdColor, COLORREF kdjColor, COLORREF wrColor, COLORREF rsiColor, COLORREF maColor)
{
	auto updateColor = [](COLORREF& storedColor, COLORREF newColor, CButton& btn) {
		if (storedColor == newColor) return;
		storedColor = newColor;
		if (btn.GetSafeHwnd())
			btn.Invalidate();
		};
	updateColor(m_bollSignalColor, bollColor, m_btnBoll);
	updateColor(m_macdSignalColor, macdColor, m_btnIndicatorMACD);
	updateColor(m_kdjSignalColor, kdjColor, m_btnIndicatorKDJ);
	updateColor(m_wrSignalColor, wrColor, m_btnIndicatorWR);
	updateColor(m_rsiSignalColor, rsiColor, m_btnIndicatorRSI);
	updateColor(m_maSignalColor, maColor, m_btnMA);
}

void CFloatingWnd::EnsureChipPeakData()
{
	if (m_showChipPeak)
	{
		bool needRequest = true;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = g_data.GetStockData(m_stock_id);
			needRequest = !stockData || !stockData->chipDistribution.IsValid() || stockData->info.circulatingAShares <= 0;
		}

		if (needRequest)
		{
			// 通过 StockFetchThread 后台任务队列执行，避免创建临时线程
			std::wstring stockId = m_stock_id;
			CStockFetchThread::Instance().PostBackgroundTask([stockId]() {
				CStockFetchThread::Instance().FetchStockBasic(stockId);
				CStockFetchThread::Instance().FetchChipDistribution(stockId);
				// UI刷新由1秒定时器检查dirty标识驱动，此处仅更新数据
				});
		}
	}
}

void CFloatingWnd::ResetHoverState()
{
	m_isHoveringVolume = false;
	m_hoveredBarIndex = -1;
	m_hoverTip.Empty();
	m_timelineVolumeTitleTip.Empty();
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
}

void CFloatingWnd::SetTimelineModeDefaults()
{
	m_viewMode = UI_VIEW_TIMELINE;
	m_showBollBands = true;
	m_btnBoll.SetWindowText(_T("BL"));
	m_showMA = false;
	m_showTrendView = false;
	m_showChipPeak = false;
	m_showJZCurve = CCommon::IsFundCode(m_stock_id);  // 基金默认显示净值曲线
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1MIN;  // 显示最新走势
	ResetHoverState();
}

void CFloatingWnd::SetDayKLineModeDefaults()
{
	m_viewMode = UI_VIEW_DAY_KLINE;
	m_showBollBands = true;
	m_btnBoll.SetWindowText(_T("BL"));
	m_showTrendView = false;  // 日K默认显示K线图
	m_showChipPeak = false;
	m_showJZCurve = false;
	m_showMA = false;
	m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1DAY;  // 日K线初始缩放到最大，显示最新40根
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	ResetHoverState();
}

void CFloatingWnd::SetMin5KLineModeDefaults()
{
	m_viewMode = UI_VIEW_MIN5_KLINE;
	m_showBollBands = true;
	m_btnBoll.SetWindowText(_T("BL"));
	m_showMA = false;
	m_showJZCurve = false;
	m_showChipPeak = false;
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_5MIN;  // 初始化缩放，显示最新40个数据点
	ResetHoverState();
}

void CFloatingWnd::SetMin30KLineModeDefaults()
{
	m_viewMode = UI_VIEW_MIN30_KLINE;
	m_showBollBands = true;
	m_btnBoll.SetWindowText(_T("BL"));
	m_showMA = false;
	m_showJZCurve = false;
	m_showChipPeak = false;  // 默认展示盘口，与5分钟视图保持一致
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_30MIN;  // 初始化缩放，显示最新16个数据点
	ResetHoverState();
}

void CFloatingWnd::OnBnClickedMABtn()
{
	m_showMA = !m_showMA;
	if (m_showMA)
	{
		m_showBollBands = false;
		m_btnBoll.SetWindowText(_T("BL"));
	}
	if (m_btnMA.GetSafeHwnd()) m_btnMA.Invalidate();
	if (m_btnBoll.GetSafeHwnd()) m_btnBoll.Invalidate();
	Invalidate();
}

void CFloatingWnd::OnBnClickedMin5KLineBtn()
{
	g_data.SetCurrentViewMode(UI_VIEW_MIN5_KLINE);

	if (m_viewMode != UI_VIEW_MIN5_KLINE)
	{
		// 切换到5分钟K线模式
		SetMin5KLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedMin30KLineBtn()
{
	g_data.SetCurrentViewMode(UI_VIEW_MIN30_KLINE);
	if (m_viewMode != UI_VIEW_MIN30_KLINE)
	{
		// 切换到30分钟K线模式
		SetMin30KLineModeDefaults();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedBollBtn()
{
	// 布林带模式：点击切换显示/隐藏
	m_showBollBands = !m_showBollBands;
	if (m_showBollBands)
		m_showMA = false;
	m_btnBoll.SetWindowText(_T("BL"));
	if (m_btnBoll.GetSafeHwnd()) m_btnBoll.Invalidate();
	if (m_btnMA.GetSafeHwnd()) m_btnMA.Invalidate();
	Invalidate();
}

void CFloatingWnd::OnBnClickedZoomOutBtn()
{
	// 缩小：先放大到最大（与"+"按钮一致），然后移动到今天最开的位置（左边第一根线为9:30）
	if (m_viewMode == UI_VIEW_TIMELINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1MIN;
		m_timelineScrollOffset = 0;  // 分时模式从9:30开始
	}
	else if (m_viewMode == UI_VIEW_DAY_KLINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1DAY;
		m_timelineScrollOffset = 0;
	}
	else if (m_viewMode == UI_VIEW_MIN30_KLINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_30MIN;
		m_timelineScrollOffset = 0;
	}
	else
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_5MIN;
		// 5分钟K线模式：找到当天第一根K线的索引作为scrollOffset
		m_timelineScrollOffset = 0;
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			auto min5KLineObj = stockData->getMin5KLineData();
			if (min5KLineObj && !min5KLineObj->data.empty())
			{
				// 5分钟K线day格式为"YYYY-MM-DD HH:MM"，取最后一根的日期作为当天
				const auto& klineData = min5KLineObj->data;
				std::string todayDate = klineData.back().day.substr(0, 10);  // "YYYY-MM-DD"
				for (size_t i = 0; i < klineData.size(); i++)
				{
					if (klineData[i].day.compare(0, 10, todayDate) == 0)
					{
						m_timelineScrollOffset = static_cast<int>(i);
						break;
					}
				}
			}
		}
	}
	Invalidate();
}

void CFloatingWnd::OnBnClickedZoomInBtn()
{
	// 放大：显示最新40个数据点
	if (m_viewMode == UI_VIEW_TIMELINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1MIN;
	}
	else if (m_viewMode == UI_VIEW_DAY_KLINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_1DAY;
	}
	else if (m_viewMode == UI_VIEW_MIN30_KLINE)
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_30MIN;
	}
	else
	{
		m_timelineVisibleCount = TIME_LINE_VISIBLE_COUNT_5MIN;
	}

	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	Invalidate();
}

void CFloatingWnd::OnBnClickedIndicatorMACDBtn()
{
	// CJL隐藏按钮（IDC_INDICATOR_MACD_BTN），不再使用
}

void CFloatingWnd::OnBnClickedIndicatorKDJBtn()
{
	// KDJ按钮两态切换：
	// 1) 当前MACD图:   切换J线叠加开/关
	// 2) 当前KDJ图:    切回MACD图并直接带上J线叠加
	if (m_timelineIndicator == TimelineIndicator::KDJ)
	{
		m_timelineIndicator = TimelineIndicator::MACD;
		m_overlayKDJInMacd = true;   // 从KDJ图切回MACD时直接带上J线叠加
	}
	else
	{
		m_timelineIndicator = TimelineIndicator::MACD;
		m_overlayKDJInMacd = !m_overlayKDJInMacd;   // 开/关切换
	}
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
	m_timelineWrTitleTip.Empty();
	m_timelineRsiTitleTip.Empty();
	Invalidate();
}

void CFloatingWnd::OnBnClickedIndicatorWRBtn()
{
	m_timelineIndicator = TimelineIndicator::WR;
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
	m_timelineWrTitleTip.Empty();
	m_timelineRsiTitleTip.Empty();
	Invalidate();
}

void CFloatingWnd::OnBnClickedIndicatorRSIBtn()
{
	m_timelineIndicator = TimelineIndicator::RSI;
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
	m_timelineWrTitleTip.Empty();
	m_timelineRsiTitleTip.Empty();
	Invalidate();
}

void CFloatingWnd::OnBnClickedCloseBtn()
{
	ReleaseCapture();
	PostMessage(IDM_CLOSE_WINDOW, 0, 0);
}

LRESULT CFloatingWnd::OnCloseWindow(WPARAM wParam, LPARAM lParam)
{
	if (GetSafeHwnd())
	{
		SetForegroundWindow();
		DestroyWindow();
	}

	g_data.SetCurrentViewMode(UI_VIEW_MIN5_KLINE);
	return 0;
}

LRESULT CFloatingWnd::OnShowEditDialog(WPARAM wParam, LPARAM lParam)
{
	if (m_pendingEditStockCode.empty())
		return 0;

	std::wstring editCode = m_pendingEditStockCode;
	m_pendingEditStockCode.clear();

	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COptionsDlg dlg(editCode, AfxGetMainWnd());
	GetWindowRect(&dlg.m_refWndRect);
	if (dlg.DoModal() == IDOK && !dlg.m_stock_code.IsEmpty())
	{
		auto& codes = g_data.m_setting_data.m_stock_codes;
		for (auto& code : codes)
		{
			if (code == editCode)
			{
				code = dlg.m_stock_code.GetString();
				break;
			}
		}
		Invalidate();
	}
	return 0;
}

LRESULT CFloatingWnd::OnShowAddDialog(WPARAM wParam, LPARAM lParam)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COptionsDlg dlg(std::wstring(), AfxGetMainWnd());
	GetWindowRect(&dlg.m_refWndRect);
	if (dlg.DoModal() == IDOK && !dlg.m_stock_code.IsEmpty())
	{
		auto& codes = g_data.m_setting_data.m_stock_codes;
		codes.push_back(dlg.m_stock_code.GetString());
		g_data.SaveConfig();

		m_vScrollOffset = 0;
		Invalidate();
	}
	return 0;
}

LRESULT CFloatingWnd::OnShowTradeDialog(WPARAM wParam, LPARAM lParam)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	//测试买卖点检测时暂时屏蔽交易记录弹窗
	CTradeRecordDialog dlg(this);
	dlg.SetTradeInfo(m_pendingTradeTime, m_pendingTradePrice, CString(m_stock_id.c_str()));
	dlg.DoModal();

	return 0;
}

void CFloatingWnd::OnDestroy()
{
	KillTimer(IDC_REFRESH_TIMER);

	CWnd::OnDestroy();

	if (m_CTransparentWnd.GetSafeHwnd())
	{
		m_CTransparentWnd.DestroyWindow();
	}

	g_data.SetCurrentViewMode(UI_VIEW_MIN5_KLINE);
}

void CFloatingWnd::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDC_REFRESH_TIMER)
	{
		// 1秒定时检查：图表和盘口分别判断，任一有变化才重绘
		bool needRedraw = false;
		if (m_chartDirty)
		{
			m_chartDirty = false;
			needRedraw = true;
		}
		if (m_orderBookDirty)
		{
			m_orderBookDirty = false;
			needRedraw = true;
		}
		// 分时模式：未闭合分时点的价格实时变化不会改变序列长度，但最后一根
		// MACD/KDJ/BOLL 柱需要跟随tick刷新。按3秒节流（与行情推送周期一致），
		// 且仅在已经在重绘时顺带刷新，不额外触发重绘 —— CPU 负担可忽略
		if (m_viewMode == UI_VIEW_TIMELINE && m_indicatorLoaded && needRedraw
			&& GetTickCount() - m_lastIndicatorRefreshTick >= 3000)
		{
			m_lastIndicatorRefreshTick = GetTickCount();
			m_lastIndicatorDataSize = -1;   // 本次重绘顺带重算指标模型
		}
		if (needRedraw)
			Invalidate();
	}
	CWnd::OnTimer(nIDEvent);
}