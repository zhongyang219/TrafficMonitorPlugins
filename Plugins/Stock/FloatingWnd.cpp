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

// 大盘指数优先级列表（用于总览列表排序）
const std::vector<std::wstring> IndexPriority = {
	L"sh000001",  // 上证指数
	L"sh000300",  // 沪深300
	L"sz399006",  // 创业板指
	L"rt_hkHSI",  // 恒生指数
	L"rt_hkHSTECH", // 恒生科技
	L"sz399001",  // 深证成指
	L"sh000016",  // 上证50
};

int GetStockPriority(const std::wstring& code)
{
	// 检查是否是优先级指数
	for (size_t i = 0; i < IndexPriority.size(); i++)
	{
		if (code == IndexPriority[i])
		{
			return static_cast<int>(i);
		}
	}

	// 检查是否是其他指数
	// 判断逻辑：需要区分前缀
	// sh000xxx: 上海指数（如sh000001上证指数已在优先列表中，其他sh000xxx也可能是指数）
	// sz000xxx: 深圳个股（如sz000725京东方A），不是指数
	// sz399xxx: 深圳指数（如sz399006创业板指已在优先列表中，其他sz399xxx也可能是指数）
	// bj000xxx: 北京个股，不是指数
	if (code.size() >= 8)
	{
		std::wstring marketPrefix = code.substr(0, 2);   // sh/sz/bj/rt等
		std::wstring numPart = code.substr(2, 6);         // 6位数字部分

		// 上海：sh000xxx为指数
		if (marketPrefix == L"sh" && numPart.find(L"000") == 0)
		{
			return 100;  // 其他上海A股指数
		}
		// 深圳：sz399xxx为指数（sz000xxx是个股！）
		if (marketPrefix == L"sz" && numPart.find(L"399") == 0)
		{
			return 100;  // 其他深圳指数
		}
	}
	else if (code.size() >= 3)
	{
		// 无市场前缀的短代码，399开头视为指数
		std::wstring prefix = code.substr(0, 3);
		if (prefix == L"399")
		{
			return 100;
		}
	}

	// 个股
	return 200;
}

bool CompareStockPriority(const std::wstring& a, const std::wstring& b)
{
	int priA = GetStockPriority(a);
	int priB = GetStockPriority(b);
	if (priA != priB)
		return priA < priB;
	// 同为个股时，按总成本（成本×持股）从大到小排序
	if (priA >= 200)
	{
		double totalCostA = g_data.GetCostPrice(a) * g_data.GetHoldingCount(a);
		double totalCostB = g_data.GetCostPrice(b) * g_data.GetHoldingCount(b);
		if (totalCostA != totalCostB)
			return totalCostA > totalCostB;
	}
	return false;
}

CString TrendStateToText(STOCK::TrendState30m state)
{
	switch (state)
	{
	case STOCK::TrendState30m::STATE_WEAK:
		return _T("弱势");
	case STOCK::TrendState30m::STATE_STRONG:
		return _T("强势");
	case STOCK::TrendState30m::STATE_SHAKE:
	default:
		return _T("震荡");
	}
}

CString Signal5mToText(STOCK::Signal5m signal)
{
	switch (signal)
	{
	case STOCK::Signal5m::SIG_BUY:
		return _T("买入");
	case STOCK::Signal5m::SIG_SELL:
		return _T("卖出");
	case STOCK::Signal5m::SIG_NONE:
	default:
		return _T("无信号");
	}
}

CString BoolToText(bool value)
{
	return value ? _T("是") : _T("否");
}

// 颜色常量定义
#define COLOR_WHITE               RGB(255, 255, 255)
#define COLOR_BLACK               RGB(0, 0, 0)
#define COLOR_RED_UP              RGB(179, 64, 65)    // 红色-上涨/盈利
#define COLOR_GREEN_DOWN          RGB(44, 144, 51)    // 绿色-下跌/亏损
#define COLOR_GRAY_TEXT           RGB(102, 102, 102)  // 灰色文字
#define COLOR_GRAY_GRID           RGB(200, 200, 200)  // 浅灰网格线
#define COLOR_GRAY_MIDDLE         RGB(140, 140, 140)  // 中灰虚线
#define COLOR_GRAY_PURPLE         RGB(154, 151, 157)  // 灰紫色
#define COLOR_BLUE_COST           RGB(0, 43, 204)     // 深蓝色成本线
#define COLOR_DARK_GRAY_BORDER    RGB(60, 60, 60)     // 深灰边框
#define COLOR_BLUE_AVG1           RGB(0, 0, 230)    // 蓝色-1年均线
#define COLOR_GREEN_AVG2          RGB(0, 166, 235)     // 2年均线
#define COLOR_GREEN_AVG3          RGB(169, 102, 186)     // -3年均线
#define COLOR_LIGHT_BLUE          RGB(210, 235, 255)  // 淡蓝色背景
#define COLOR_LIGHT_GREEN         RGB(210, 245, 210)  // 淡绿色背景
#define COLOR_GOLDEN              RGB(200,150,0)  // 黄金色

// 涨幅/盈亏背景颜色
#define COLOR_BG_RED              RGB(220, 80, 80)    // >= 5% 红色背景
#define COLOR_BG_PURPLE           RGB(160, 50, 160)   // >= 10% 紫色背景
#define COLOR_BG_GREEN            RGB(60, 160, 70)    // <= -5% 绿色背景
#define COLOR_BG_DARK_GREEN       RGB(20, 100, 40)    // <= -10% 墨绿色背景

// 根据百分比值获取背景颜色（用于涨幅和盈亏列）
COLORREF GetCellBgColor(double percent)
{
	if (percent >= 10.0)
		return COLOR_BG_PURPLE;
	else if (percent >= 5.0)
		return COLOR_BG_RED;
	else if (percent <= -10.0)
		return COLOR_BG_DARK_GREEN;
	else if (percent <= -5.0)
		return COLOR_BG_GREEN;
	return COLOR_WHITE;  // 默认白色背景
}

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
	IDC_T0_BTN = 1017,
	IDC_CHIP_PEAK_BTN = 1018,
	IDC_ORDER_BOOK_BTN = 1019,
	IDC_EXPAND_BTN = 1020,
	IDC_TOGGLE_STOCK_LIST_BTN = 1021
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
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_MESSAGE((WM_USER + 100), OnUpdateStatus)
	ON_MESSAGE((WM_USER + 101), OnRequestData)
	ON_MESSAGE((WM_USER + 102), OnShowEditDialog)
	ON_MESSAGE((WM_USER + 103), OnShowAddDialog)
	ON_MESSAGE((WM_USER + 104), OnShowTradeDialog)
	ON_MESSAGE(IDM_CLOSE_WINDOW, OnCloseWindow)
	ON_BN_CLICKED(IDC_TIMELINE_BTN, &CFloatingWnd::OnBnClickedTimeLineBtn)
	ON_BN_CLICKED(IDC_KLINE_BTN, &CFloatingWnd::OnBnClickedKLineBtn)
	ON_BN_CLICKED(IDC_T0_BTN, &CFloatingWnd::OnBnClickedT0Btn)
	ON_BN_CLICKED(IDC_CLOSE_BTN, &CFloatingWnd::OnBnClickedCloseBtn)
	ON_BN_CLICKED(IDC_MA_BTN, &CFloatingWnd::OnBnClickedMABtn)
	ON_BN_CLICKED(IDC_MIN5_KLINE_BTN, &CFloatingWnd::OnBnClickedMin5KLineBtn)
	ON_BN_CLICKED(IDC_MIN30_KLINE_BTN, &CFloatingWnd::OnBnClickedMin30KLineBtn)
	ON_BN_CLICKED(IDC_BOLL_BTN, &CFloatingWnd::OnBnClickedBollBtn)
	ON_BN_CLICKED(IDC_ZOOM_OUT_BTN, &CFloatingWnd::OnBnClickedZoomOutBtn)
	ON_BN_CLICKED(IDC_ZOOM_IN_BTN, &CFloatingWnd::OnBnClickedZoomInBtn)
	ON_BN_CLICKED(IDC_INDICATOR_MACD_BTN, &CFloatingWnd::OnBnClickedIndicatorMACDBtn)
	ON_BN_CLICKED(IDC_INDICATOR_KDJ_BTN, &CFloatingWnd::OnBnClickedIndicatorKDJBtn)
	ON_BN_CLICKED(IDC_INDICATOR_WR_BTN, &CFloatingWnd::OnBnClickedIndicatorWRBtn)
	ON_BN_CLICKED(IDC_INDICATOR_RSI_BTN, &CFloatingWnd::OnBnClickedIndicatorRSIBtn)
	ON_BN_CLICKED(IDC_CHIP_PEAK_BTN, &CFloatingWnd::OnBnClickedChipPeakBtn)
	ON_BN_CLICKED(IDC_ORDER_BOOK_BTN, &CFloatingWnd::OnBnClickedOrderBookBtn)
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

	// 左侧按钮：分时、5分、30分、日K（贴边排列，无间隙）
	CRect timelineRect(0, g_data.RDPI(2), btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnTimeLine.Create(_T("分时"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, timelineRect, this, IDC_TIMELINE_BTN);

	CRect min5KLineRect(timelineRect.right + btnGap, g_data.RDPI(2), timelineRect.right + btnGap + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnMin5KLine.Create(_T("5分"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, min5KLineRect, this, IDC_MIN5_KLINE_BTN);

	CRect min30KLineRect(min5KLineRect.right + btnGap, g_data.RDPI(2), min5KLineRect.right + btnGap + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnMin30KLine.Create(_T("30分"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, min30KLineRect, this, IDC_MIN30_KLINE_BTN);

	CRect klineRect(min30KLineRect.right + btnGap, g_data.RDPI(2), min30KLineRect.right + btnGap + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnKLine.Create(_T("日K"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, klineRect, this, IDC_KLINE_BTN);

	// 右侧按钮：关闭、筹码峰（T0/MA/BOLL 在走势图标题栏右侧定位）
	const int closeBtnWidth = g_data.RDPI(20);
	const int closeBtnHeight = g_data.RDPI(18);
	const int cx = lpCreateStruct->cx;
	CRect closeBtnRect(cx - closeBtnWidth, g_data.RDPI(2), cx, g_data.RDPI(2) + closeBtnHeight);
	m_btnClose.Create(_T("X"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, closeBtnRect, this, IDC_CLOSE_BTN);

	// 放大按钮在X按钮左边
	const int expandBtnWidth = closeBtnWidth;
	const int expandBtnHeight = closeBtnHeight;
	CRect expandBtnRect(closeBtnRect.left - expandBtnWidth, g_data.RDPI(2), closeBtnRect.left, g_data.RDPI(2) + expandBtnHeight);
	m_btnExpand.Create(_T("△"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, expandBtnRect, this, IDC_EXPAND_BTN);

	// 股票列表显示/隐藏按钮在放大按钮左边
	const int toggleStockListBtnWidth = closeBtnWidth;
	const int toggleStockListBtnHeight = closeBtnHeight;
	CRect toggleStockListBtnRect(expandBtnRect.left - toggleStockListBtnWidth, g_data.RDPI(2), expandBtnRect.left, g_data.RDPI(2) + toggleStockListBtnHeight);
	m_btnToggleStockList.Create(_T("<|"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, toggleStockListBtnRect, this, IDC_TOGGLE_STOCK_LIST_BTN);

	const int rightBtnWidth = g_data.RDPI(40);

	CRect chipPeakBtnRect(closeBtnRect.left - rightBtnWidth, g_data.RDPI(2), closeBtnRect.left, g_data.RDPI(2) + btnHeight);
	m_btnChipPeak.Create(_T("CM"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, chipPeakBtnRect, this, IDC_CHIP_PEAK_BTN);

	CRect orderBookBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnOrderBook.Create(_T("PK"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, orderBookBtnRect, this, IDC_ORDER_BOOK_BTN);

	CRect bollBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnBoll.Create(_T("ZF"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, bollBtnRect, this, IDC_BOLL_BTN);
	m_btnBoll.ShowWindow(SW_HIDE);

	CRect maBtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnMA.Create(_T("MA"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, maBtnRect, this, IDC_MA_BTN);
	m_btnMA.ShowWindow(SW_HIDE);

	CRect t0BtnRect(0, 0, rightBtnWidth, btnHeight);
	m_btnT0.Create(_T("T0"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, t0BtnRect, this, IDC_T0_BTN);
	m_btnT0.ShowWindow(SW_HIDE);

	// 缩放按钮（分时模式专用，初始隐藏，在量柱图标题栏右侧定位）
	const int zoomBtnWidth = g_data.RDPI(28);
	const int zoomBtnHeight = g_data.RDPI(16);
	CRect zoomOutRect(0, 0, zoomBtnWidth, zoomBtnHeight);
	CRect zoomInRect(0, 0, zoomBtnWidth, zoomBtnHeight);
	m_btnZoomOut.Create(_T("—"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, zoomOutRect, this, IDC_ZOOM_OUT_BTN);
	m_btnZoomIn.Create(_T("＋"), WS_CHILD | BS_PUSHBUTTON | BS_FLAT, zoomInRect, this, IDC_ZOOM_IN_BTN);
	m_btnZoomOut.ShowWindow(SW_HIDE);
	m_btnZoomIn.ShowWindow(SW_HIDE);

	// MACD/KDJ指标切换按钮（初始隐藏，在OnPaint中定位）
	m_btnIndicatorMACD.Create(_T("MACD"), WS_CHILD | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_INDICATOR_MACD_BTN);
	m_btnIndicatorKDJ.Create(_T("KDJ"), WS_CHILD | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_INDICATOR_KDJ_BTN);
	m_btnIndicatorWR.Create(_T("W&&R"), WS_CHILD | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_INDICATOR_WR_BTN);
	m_btnIndicatorRSI.Create(_T("RSI"), WS_CHILD | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_INDICATOR_RSI_BTN);
	m_btnIndicatorMACD.ShowWindow(SW_HIDE);
	m_btnIndicatorKDJ.ShowWindow(SW_HIDE);
	m_btnIndicatorWR.ShowWindow(SW_HIDE);
	m_btnIndicatorRSI.ShowWindow(SW_HIDE);

	m_hScrollBar.Create(WS_CHILD | SBS_HORZ, CRect(0, 0, 0, 0), this, 0);
	m_hScrollBar.ShowWindow(SW_HIDE);

	UpdateModeButtons();
	UpdatePeriodComboVisibility();

	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	return 0;
}

// 处理消息
LRESULT CFloatingWnd::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
	Invalidate();
	return 0;
}

LRESULT CFloatingWnd::OnRequestData(WPARAM wParam, LPARAM lParam)
{
	time_t req_time = (time_t)wParam;
	if (req_time)
	{
		time_t now = req_time;
		bool need = false;

		// 视图相关数据（分时/日K线）：仅在非5min/30min视图下按10秒间隔获取
		if (!m_isMin5KLineMode && !m_isMin30KLineMode)
		{
			if (now - (time_t)m_last_request_time > 10)
				need = true;
		}
		// 5分钟K线：固定60秒间隔获取，与当前视图无关
		if (now - (time_t)m_last_min5_fetch_time > 60)
			need = true;
		// 30分钟K线：固定600秒间隔获取，与当前视图无关
		if (now - (time_t)m_last_min30_fetch_time > 600)
			need = true;

		if (need)
			RequestData();

		loading_state_txt += L".";
		Invalidate();
	}
	return 0;
}

CFloatingWnd::CFloatingWnd() : m_isDestroying(FALSE), m_klineDataLoaded(false), m_isOverviewMode(false)
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

	// 设置完全透明
	m_CTransparentWnd.SetLayeredWindowAttributes(0, 0, LWA_ALPHA);
	m_CTransparentWnd.ShowWindow(SW_SHOW);

	TRACE(L"Windows created successfully\n");
	return TRUE;
}

CPoint CFloatingWnd::Stock2Point(int x, int y, int w, int h, double unitY, const STOCK::TimelinePoint& item, const STOCK::Price prevClosePrice)
{
	CPoint p = CPoint();
	std::vector<std::string> time_arr = CCommon::split(item.time, ":");
	if (time_arr.size() == 3)
	{
		// 9:30 10:00 11:30 13:00 14:00 15:00
		static int before12ClockOffset = 570; // 9.5 * 60;
		static int after12ClockOffset = 660;  // 9.5 * 60 + 1.5 * 60;
		static float totalMinutes = 240.0;    // 4 * 60;

		int hour = _ttoi(CString(time_arr[0].c_str()));
		int minute = _ttoi(CString(time_arr[1].c_str()));

		int countX = hour * 60 + minute;

		if (hour < 12)
		{
			countX -= before12ClockOffset;
		}
		else if (hour >= 13)
		{
			countX -= after12ClockOffset;
		}
		p.x = w / totalMinutes * countX;
	}
	p.y = (item.price - prevClosePrice) * unitY * 100;
	return p;
}

// ========== 分时图绘制函数 ==========

// 时间标记结构体（供分时图绘制函数共用）
struct TimeMarker {
	const TCHAR* label;
	int minutesFromStart;
};

void CFloatingWnd::DrawHeader(CDC& memDC, const STOCK::StockInfo& realtimeData, int windowWidth, int headerHeight)
{
	double diff = realtimeData.GetChangeAmount();
	double diffPercent = realtimeData.GetChangePercent();
	COLORREF diffColor = diff >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;

	// 标题格式：(股票代码)股票名称：
	CString prefixTxt;
	prefixTxt.Format(_T("%s:"), realtimeData.displayName.c_str());

	CString currentTxt = realtimeData.IsETF() ? CCommon::FormatETFPrice(realtimeData.currentPrice) : CCommon::FormatFloat(realtimeData.currentPrice);
	CString diffTxt;
	if (diff >= 0)
		diffTxt.Format(_T(" +%.2f%%"), diffPercent);
	else
		diffTxt.Format(_T(" %.2f%%"), diffPercent);

	// 计算总宽度，在整个标题栏水平居中
	CSize prefixSize = memDC.GetTextExtent(prefixTxt);
	CSize currentSize = memDC.GetTextExtent(currentTxt);
	CSize diffSize = memDC.GetTextExtent(diffTxt);
	int totalWidth = prefixSize.cx + currentSize.cx + diffSize.cx;

	int startX = (windowWidth - totalWidth) / 2;
	int centerY = headerHeight / 2;

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(startX, centerY - prefixSize.cy / 2, prefixTxt);

	int curX = startX + prefixSize.cx;
	memDC.SetTextColor(diffColor);
	memDC.TextOut(curX, centerY - currentSize.cy / 2, currentTxt);

	curX += currentSize.cx;
	memDC.TextOut(curX, centerY - diffSize.cy / 2, diffTxt);
}

void CFloatingWnd::DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx)
{
	// 标题栏需要在原始坐标系绘制（当前视口已被OffsetViewportOrg偏移），恢复后再绘制
	CPoint origOrg = memDC.GetViewportOrg();
	memDC.SetViewportOrg(0, 0);
	DrawHeader(memDC, ctx.realtimeData, ctx.windowWidth, g_data.RDPI(26));
	memDC.SetViewportOrg(origOrg);
}

// 绘制最高价、最低价行（原MA均线位置）
void CFloatingWnd::DrawTimelineMAIndicators(CDC& memDC, const TimelineDrawContext& ctx)
{
	int maY = g_data.RDPI(26) + g_data.RDPI(2);
	if (ctx.realtimeData.currentPrice <= 0)
		return;

	// 最高价
	CString highTxt;
	COLORREF highColor = (ctx.realtimeData.highPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	highTxt.Format(_T("最高:%s "), CCommon::FormatFloat(ctx.realtimeData.highPrice));

	// 最低价
	CString lowTxt;
	COLORREF lowColor = (ctx.realtimeData.lowPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	lowTxt.Format(_T("最低:%s"), CCommon::FormatFloat(ctx.realtimeData.lowPrice));

	// 居中排列
	int totalWidth = memDC.GetTextExtent(highTxt).cx + memDC.GetTextExtent(lowTxt).cx;
	int startX = (ctx.chartWidth - totalWidth) / 2;
	startX = max(g_data.RDPI(5), startX);

	memDC.SetTextColor(highColor);
	memDC.TextOut(startX, maY, highTxt);
	startX += memDC.GetTextExtent(highTxt).cx;

	memDC.SetTextColor(lowColor);
	memDC.TextOut(startX, maY, lowTxt);
}

// 绘制开盘/尾盘时间区间高亮背景
void CFloatingWnd::DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx)
{
	// 仅在显示全天数据时绘制背景高亮
	if (!ctx.timelinePoint || ctx.timelinePoint->size() < 240)
		return;
	CBrush blueBrush(COLOR_LIGHT_BLUE);
	memDC.FillRect(CRect(ctx.chartWidth * 0 / 240, ctx.priceChartTop, ctx.chartWidth * 30 / 240, ctx.priceChartTop + ctx.priceChartHeight), &blueBrush);
	memDC.FillRect(CRect(ctx.chartWidth * 140 / 240, ctx.priceChartTop, ctx.chartWidth * 150 / 240, ctx.priceChartTop + ctx.priceChartHeight), &blueBrush);

	CBrush greenBrush(COLOR_LIGHT_GREEN);
	memDC.FillRect(CRect(ctx.chartWidth * 210 / 240, ctx.priceChartTop, ctx.chartWidth * 225 / 240, ctx.priceChartTop + ctx.priceChartHeight), &greenBrush);
}

// 绘制网格线（时间竖线、横线、中间虚线）
void CFloatingWnd::DrawTimelineGridLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	// X轴时间竖线：4等分可见数据范围，共5条竖线（含首尾）
	if (ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		const int totalPts = static_cast<int>(ctx.timelinePoint->size());
		const int numVLines = 4;
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, ctx.priceChartTop);
			memDC.LineTo(xPos, ctx.priceChartTop + ctx.priceChartHeight);
		}
	}

	// X轴横线
	memDC.MoveTo(0, ctx.priceChartTop + ctx.priceChartHeight);
	memDC.LineTo(ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight);

	// 走势图横线（使用OnPaint预计算的niceStep）
	if (ctx.maxPrice > 0 && ctx.minPrice > 0 && ctx.maxPrice > ctx.minPrice && ctx.niceStep > 0)
	{
		CPen pGridLine(PS_DOT, 1, COLOR_GRAY_GRID);
		memDC.SelectObject(&pGridLine);
		double priceRange = ctx.maxPrice - ctx.minPrice;
		double unitY = ctx.unitY;
		int labelCount = static_cast<int>(round(priceRange / ctx.niceStep));
		for (int i = 0; i <= labelCount; i++)
		{
			double labelPrice = round((ctx.minPrice + i * ctx.niceStep) * 1000.0) / 1000.0;
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((labelPrice - ctx.minPrice) * unitY));
			memDC.MoveTo(0, y);
			memDC.LineTo(ctx.chartWidth, y);
		}
	}

	// 昨收价虚线（仅当昨收价在可见Y轴范围内时绘制）
	if (ctx.maxPrice > 0 && ctx.minPrice > 0 && ctx.maxPrice > ctx.minPrice && ctx.realtimeData.prevClosePrice > 0
		&& ctx.realtimeData.prevClosePrice >= ctx.minPrice && ctx.realtimeData.prevClosePrice <= ctx.maxPrice)
	{
		CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
		memDC.SelectObject(&pMiddleLine);
		int prevCloseY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((ctx.realtimeData.prevClosePrice - ctx.minPrice) * ctx.unitY));
		prevCloseY = max(ctx.priceChartTop, min(prevCloseY, ctx.priceChartTop + ctx.priceChartHeight));
		memDC.MoveTo(0, prevCloseY);
		memDC.LineTo(ctx.chartWidth, prevCloseY);
	}

	memDC.SelectObject(pOldPen);
}

// 绘制Y轴价格标签（最高价/最低价/昨收价）
void CFloatingWnd::DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx)
{
	// Y轴价格标签（使用OnPaint预计算的niceStep）
	if (ctx.maxPrice > 0 && ctx.minPrice > 0 && ctx.maxPrice > ctx.minPrice && ctx.niceStep > 0)
	{
		int oldBkMode = memDC.SetBkMode(TRANSPARENT);
		double priceRange = ctx.maxPrice - ctx.minPrice;
		// 直接使用ctx.unitY，确保与网格线/悬停提示完全一致
		double unitY = ctx.unitY;

		// 用整数循环替代浮点累加，避免精度误差导致标签重复或丢失
		int labelCount = static_cast<int>(round(priceRange / ctx.niceStep));
		for (int i = 0; i <= labelCount; i++)
		{
			double labelPrice = round((ctx.minPrice + i * ctx.niceStep) * 1000.0) / 1000.0;
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((labelPrice - ctx.minPrice) * unitY));
			CString priceTxt = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(labelPrice) : CCommon::FormatFloat(labelPrice);
			CSize sz = memDC.GetTextExtent(priceTxt);
			int labelX = -sz.cx - g_data.RDPI(4);
			int labelY = y - sz.cy / 2;
			// 标签颜色：高于昨收红色，低于昨收绿色，等于昨收黑色
			if (ctx.realtimeData.prevClosePrice > 0 && labelPrice > ctx.realtimeData.prevClosePrice + ctx.niceStep * 0.01)
				memDC.SetTextColor(COLOR_RED_UP);
			else if (ctx.realtimeData.prevClosePrice > 0 && labelPrice < ctx.realtimeData.prevClosePrice - ctx.niceStep * 0.01)
				memDC.SetTextColor(COLOR_GREEN_DOWN);
			else
				memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(labelX, labelY, priceTxt);
		}

		// 昨收价标签（仅当昨收价在可见Y轴范围内时绘制）
		if (ctx.realtimeData.prevClosePrice > 0
			&& ctx.realtimeData.prevClosePrice >= ctx.minPrice && ctx.realtimeData.prevClosePrice <= ctx.maxPrice)
		{
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			CString prevTxt = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(ctx.realtimeData.prevClosePrice) : CCommon::FormatFloat(ctx.realtimeData.prevClosePrice);
			CSize prevSize = memDC.GetTextExtent(prevTxt);
			int prevY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((ctx.realtimeData.prevClosePrice - ctx.minPrice) * unitY));
			int labelY = prevY - prevSize.cy / 2;
			memDC.TextOut(-prevSize.cx - g_data.RDPI(4), labelY, prevTxt);
		}
		memDC.SetBkMode(oldBkMode);
		return;
	}

	// 回退：涨跌停价标签（居中对齐）
	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	memDC.SetTextColor(COLOR_RED_UP);
	CString upperLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice + priceLimit);
	CSize upperSize = memDC.GetTextExtent(upperLimitTxt);
	int upperY = ctx.priceChartTop - upperSize.cy / 2;
	upperY = max(ctx.priceChartTop, min(upperY, ctx.priceChartTop + ctx.priceChartHeight - upperSize.cy));
	memDC.TextOut(-upperSize.cx - g_data.RDPI(4), upperY, upperLimitTxt);

	memDC.SetTextColor(COLOR_GREEN_DOWN);
	CString lowerLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice - priceLimit);
	CSize lowerSize = memDC.GetTextExtent(lowerLimitTxt);
	int lowerY = ctx.priceChartTop + ctx.priceChartHeight - lowerSize.cy / 2;
	lowerY = max(ctx.priceChartTop, min(lowerY, ctx.priceChartTop + ctx.priceChartHeight - lowerSize.cy));
	memDC.TextOut(-lowerSize.cx - g_data.RDPI(4), lowerY, lowerLimitTxt);

	memDC.SetTextColor(COLOR_GRAY_PURPLE);
	CString middleTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice);
	CSize midSize = memDC.GetTextExtent(middleTxt);
	int midY = ctx.priceChartTop + ctx.priceChartHeight / 2 - midSize.cy / 2;
	midY = max(ctx.priceChartTop, min(midY, ctx.priceChartTop + ctx.priceChartHeight - midSize.cy));
	memDC.TextOut(-midSize.cx - g_data.RDPI(4), midY, middleTxt);

	memDC.SetBkMode(oldBkMode);
}

// 绘制成本线和盈亏线（已禁用：用户要求去掉成本曲线和振幅最高最低曲线）
void CFloatingWnd::DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	// 已禁用：不再绘制成本线和振幅最高最低曲线
	(void)memDC;
	(void)ctx;
}

// 保持向后兼容：DrawTimelineGridAndLines 调用新的拆分函数
void CFloatingWnd::DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawTimelineBackgroundHighlights(memDC, ctx);
	DrawTimelineGridLines(memDC, ctx);
	DrawTimelinePriceLabels(memDC, ctx);
	DrawTimelineCostAndProfitLines(memDC, ctx);
}

void CFloatingWnd::DrawTimelinePriceCurve(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());

	// Y轴范围：直接使用ctx中预计算的参数，确保与标签/网格线/悬停提示完全一致
	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice <= 0 || maxPrice <= minPrice || unitY <= 0)
	{
		// 回退：基于涨跌停限制
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	CPen pKLine(PS_SOLID, 2, COLOR_DARK_GRAY_BORDER);
	memDC.SelectObject(&pKLine);

	// 价格曲线
	std::vector<CPoint> dataPoints;
	dataPoints.reserve(timelinePoint.size());
	for (int i = 0; i < totalPoints; i++)
	{
		const auto& item = timelinePoint[i];
		int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
		double yVal = (item.price - minPrice) * unitY;
		dataPoints.push_back(CPoint(pointX, static_cast<int>(round(yVal))));
	}

	if (!dataPoints.empty())
	{
		// 走势线：从第一个可见数据点开始绘制（缩放/拖动时openPrice可能不在可见范围内）
		memDC.MoveTo(dataPoints[0].x, ctx.priceChartTop + ctx.priceChartHeight - dataPoints[0].y);
		for (int i = 1; i < static_cast<int>(dataPoints.size()); i++)
			memDC.LineTo(dataPoints[i].x, ctx.priceChartTop + ctx.priceChartHeight - dataPoints[i].y);

		// 最高/最低价标签
		{
			STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
			int hiIdx = -1, loIdx = -1;
			for (int i = 0; i < totalPoints; i++)
			{
				STOCK::Price p = timelinePoint[i].price;
				if (p > 0)
				{
					if (p > hiPrice) { hiPrice = p; hiIdx = i; }
					if (p >= hiPrice) { hiPrice = p; hiIdx = i; }  // 相同价格取最后一个
					if (p < loPrice) { loPrice = p; loIdx = i; }
					if (p <= loPrice) { loPrice = p; loIdx = i; }  // 相同价格取最后一个
				}
			}

			STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

			// 最高价标签：默认显示在最高点上方，左右边缘时改为侧向显示
			if (hiIdx >= 0 && hiPrice > 0)
			{
				int hiX = dataPoints[hiIdx].x;
				int hiY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[hiIdx].y;
				DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
					hiPrice, true, COLOR_RED_UP);
			}

			// 最低价标签：默认显示在最低点下方，左右边缘时改为侧向显示
			if (loIdx >= 0 && loPrice > 0 && loIdx != hiIdx)
			{
				int loX = dataPoints[loIdx].x;
				int loY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[loIdx].y;
				DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
					loPrice, false, COLOR_GREEN_DOWN);
			}
		}

		// 均价线
		CPen avgLinePen(PS_SOLID, 1, COLOR_GOLDEN);
		CPen* pOldAvgPen = memDC.SelectObject(&avgLinePen);
		bool firstAvgPoint = true;
		for (int i = 0; i < totalPoints; i++)
		{
			const auto& item = timelinePoint[i];
			if (item.averagePrice <= 0)
			{
				firstAvgPoint = true;
				continue;
			}
			int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((item.averagePrice - minPrice) * unitY));
			py = max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			if (firstAvgPoint)
			{
				memDC.MoveTo(pointX, py);
				firstAvgPoint = false;
			}
			else
			{
				memDC.LineTo(pointX, py);
			}
		}
		memDC.SelectObject(pOldAvgPen);
	}

	// MA5/MA10/MA20均线（仅当m_showMA开启时绘制）
	if (m_showMA)
	{
		const COLORREF ma5Color = RGB(0, 0, 230);       // 蓝色
		const COLORREF ma10Color = RGB(0, 166, 235);     // 青色
		const COLORREF ma20Color = RGB(169, 102, 186);   // 紫色

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round(yVal));
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带（仅当m_showBollBands开启时绘制）
	if (m_showBollBands)
	{
		const int N = 20;       // 布林带周期
		const int K = 2;        // 标准差倍数

		// 使用完整分时数据计算布林带，避免缩放/拖动时subTimeline前N-1个点被跳过
		const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;

		std::vector<double> upperBand(totalPoints, 0);
		std::vector<double> middleBand(totalPoints, 0);
		std::vector<double> lowerBand(totalPoints, 0);

		for (int i = 0; i < totalPoints; i++)
		{
			// 将subTimeline的局部索引映射到完整数据的全局索引
			int globalIdx = ctx.startIndex + i;
			if (globalIdx < N - 1)
			{
				upperBand[i] = middleBand[i] = lowerBand[i] = 0;
				continue;
			}
			double sum = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				sum += fullData[j].price;
			}
			double ma = sum / N;
			double variance = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				double diff = fullData[j].price - ma;
				variance += diff * diff;
			}
			double stddev = std::sqrt(variance / N);
			middleBand[i] = ma;
			upperBand[i] = ma + K * stddev;
			lowerBand[i] = ma - K * stddev;
		}

		auto priceToY = [&](double price) -> int {
			return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			};

		// 绘制上轨（金色）
		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_SOLID, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				if (band[i] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				int py = priceToY(band[i]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(0, 0, 230));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 振幅上下线（仅当m_showAmplitudeBands开启时绘制）
	// 以近5日平均振幅为基准，每个分时点的均价*(1+振幅/2)作为上轨，均价*(1-振幅/2)作为下轨
	if (m_showAmplitudeBands)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		auto* dayKLineObj = stockData ? stockData->getKLineData() : nullptr;
		double avgAmplitude = dayKLineObj ? dayKLineObj->CalculateAverageAmplitude(5) : 0;  // 百分比，如3.5表示3.5%
		if (avgAmplitude > 0)
		{
			double ampRatio = avgAmplitude / 100.0 / 2.0;  // 转换为小数再除2

			auto priceToY = [&](double price) -> int {
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
				return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
				};

			// 绘制振幅上轨曲线（红色）
			{
				CPen upperPen(PS_SOLID, 1, COLOR_RED_UP);
				memDC.SelectObject(&upperPen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					double avgP = timelinePoint[i].averagePrice;
					if (avgP <= 0) { first = true; continue; }
					double upperPrice = avgP * (1 + ampRatio);
					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int py = priceToY(upperPrice);
					if (first) { memDC.MoveTo(pointX, py); first = false; }
					else { memDC.LineTo(pointX, py); }
				}
			}
			// 绘制振幅下轨曲线（绿色）
			{
				CPen lowerPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
				memDC.SelectObject(&lowerPen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					double avgP = timelinePoint[i].averagePrice;
					if (avgP <= 0) { first = true; continue; }
					double lowerPrice = avgP * (1 - ampRatio);
					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int py = priceToY(lowerPrice);
					if (first) { memDC.MoveTo(pointX, py); first = false; }
					else { memDC.LineTo(pointX, py); }
				}
			}

			// 在右端标注价格
			double lastAvgP = timelinePoint.back().averagePrice;
			if (lastAvgP > 0)
			{
				CString upperLabel, lowerLabel;
				upperLabel.Format(_T("%.2f"), lastAvgP * (1 + ampRatio));
				lowerLabel.Format(_T("%.2f"), lastAvgP * (1 - ampRatio));
				int labelX = ctx.chartLeft + ctx.chartWidth + 2;
				int upperY = priceToY(lastAvgP * (1 + ampRatio));
				int lowerY = priceToY(lastAvgP * (1 - ampRatio));
				memDC.SetTextColor(COLOR_RED_UP);
				memDC.TextOut(labelX, upperY - memDC.GetTextExtent(upperLabel).cy / 2, upperLabel);
				memDC.SetTextColor(COLOR_GREEN_DOWN);
				memDC.TextOut(labelX, lowerY - memDC.GetTextExtent(lowerLabel).cy / 2, lowerLabel);
			}
		}
	}

	// 绘制智能分析买卖点标记（基于5min/30min K线共振判定）
	if (m_showT0Markers)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			auto min5KLineObj = stockData->getMin5KLineData();
			auto min30KLineObj = stockData->getMin30KLineData();

			if (min5KLineObj && min5KLineObj->data.size() >= 120 &&
				min30KLineObj && min30KLineObj->data.size() >= 22)
			{
				// 将KLinePoint转换为Bar
				std::vector<STOCK::Bar> bars5, bars30;
				bars5.reserve(min5KLineObj->data.size());
				for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));
				bars30.reserve(min30KLineObj->data.size());
				for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

				// 批量检测信号（一次性计算全部指标序列，高效）
				auto signals = CSignalAnalyzer::BatchDetectSignals(bars5, bars30);

				// 构建分时时间→索引查找表（支持HH:MM和HH:MM:SS格式）
				std::map<std::string, int> timeIndexMap;
				for (int k = 0; k < totalPoints; k++)
				{
					const auto& t = timelinePoint[k].time;
					// 同时存原始格式和截断到HH:MM的格式
					timeIndexMap[t] = k;
					if (t.length() > 5 && t[5] == ':')
						timeIndexMap[t.substr(0, 5)] = k;
				}

				// 将5分钟K线信号映射到分时数据点
				auto buySignals = std::vector<bool>(totalPoints, false);
				auto sellSignals = std::vector<bool>(totalPoints, false);
				auto forbidSignals = std::vector<bool>(totalPoints, false);
				auto buyReasons = std::vector<CString>(totalPoints);
				auto sellReasons = std::vector<CString>(totalPoints);

				for (const auto& sig : signals)
				{
					const auto& bar5Time = min5KLineObj->data[sig.barIndex].day;
					// 提取 "HH:MM" 部分
					std::string timeStr;
					auto spacePos = bar5Time.find(' ');
					if (spacePos != std::string::npos && bar5Time.length() > spacePos + 5)
						timeStr = bar5Time.substr(spacePos + 1, 5);
					else if (bar5Time.length() >= 5 && bar5Time[2] == ':')
						timeStr = bar5Time.substr(0, 5);
					else
						timeStr = bar5Time;

					// 在查找表中匹配
					auto it = timeIndexMap.find(timeStr);
					if (it == timeIndexMap.end() && timeStr.length() >= 8)
					{
						// 尝试截断 "HH:MM:SS" → "HH:MM"
						it = timeIndexMap.find(timeStr.substr(0, 5));
					}
					if (it != timeIndexMap.end())
					{
						int k = it->second;
						if (sig.isForbid)
						{
							// 禁止信号优先级最高，覆盖已有的买卖信号
							forbidSignals[k] = true;
							buySignals[k] = false;
							sellSignals[k] = false;
						}
						else if (!forbidSignals[k])
						{
							// 非禁止信号且当前点不是禁止，才设置买卖信号
							if (sig.isBuy)
							{
								buySignals[k] = true;
								buyReasons[k] = sig.reason;
							}
							else
							{
								sellSignals[k] = true;
								sellReasons[k] = sig.reason;
							}
						}
					}
				}

				// 绘制买卖点标记
				const int dotR = g_data.RDPI(3);
				const int labelOff = g_data.RDPI(8);
				int oldBkMode = memDC.SetBkMode(TRANSPARENT);
				auto drawSignalArrow = [&](int x, int fromY, int toY, COLORREF color) {
					CPen pen(PS_SOLID, 1, color);
					CPen* pOldP = memDC.SelectObject(&pen);
					memDC.MoveTo(x, fromY);
					memDC.LineTo(x, toY);

					int dir = (toY >= fromY) ? 1 : -1;
					int arrowLen = g_data.RDPI(4);
					int arrowHalf = g_data.RDPI(3);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x - arrowHalf, toY - dir * arrowLen);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x + arrowHalf, toY - dir * arrowLen);
					memDC.SelectObject(pOldP);
					};

				for (int i = 0; i < totalPoints; i++)
				{
					if (!buySignals[i] && !sellSignals[i] && !forbidSignals[i])
						continue;
					if (i >= static_cast<int>(dataPoints.size()))
						continue;

					int ptX = dataPoints[i].x;
					int ptY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[i].y;

					if (forbidSignals[i])
					{
						CPen pen(PS_SOLID, 2, COLOR_GRAY_TEXT);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(4);
						memDC.MoveTo(ptX - r, ptY - r);
						memDC.LineTo(ptX + r, ptY + r);
						memDC.MoveTo(ptX + r, ptY - r);
						memDC.LineTo(ptX - r, ptY + r);
						memDC.SelectObject(pOldP);
					}
					else if (buySignals[i])
					{
						CBrush brush(COLOR_GREEN_DOWN);
						CPen pen(PS_SOLID, 1, COLOR_GREEN_DOWN);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						memDC.Ellipse(ptX - dotR, ptY - dotR, ptX + dotR, ptY + dotR);
						memDC.SetTextColor(COLOR_GREEN_DOWN);
						CString label = buyReasons[i].IsEmpty() ? _T("B") : buyReasons[i];
						CSize sz = memDC.GetTextExtent(label);
						int labelY = ptY + dotR + labelOff;
						memDC.TextOut(ptX - sz.cx / 2, labelY, label);
						drawSignalArrow(ptX, labelY, ptY + dotR, COLOR_GREEN_DOWN);
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
					else if (sellSignals[i])
					{
						CBrush brush(COLOR_RED_UP);
						CPen pen(PS_SOLID, 1, COLOR_RED_UP);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						memDC.Ellipse(ptX - dotR, ptY - dotR, ptX + dotR, ptY + dotR);
						memDC.SetTextColor(COLOR_RED_UP);
						CString label = sellReasons[i].IsEmpty() ? _T("S") : sellReasons[i];
						CSize sz = memDC.GetTextExtent(label);
						int labelY = ptY - dotR - labelOff - sz.cy;
						memDC.TextOut(ptX - sz.cx / 2, labelY, label);
						drawSignalArrow(ptX, labelY + sz.cy, ptY - dotR, COLOR_RED_UP);
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
				}
				memDC.SetBkMode(oldBkMode);
			}
		}
	}
}

void CFloatingWnd::DrawTimelineHoverOverlay(CDC& memDC, const TimelineDrawContext& ctx)
{
	if (!m_isHoveringVolume || m_hoveredBarIndex < 0)
		return;

	const auto& timelinePoint = *ctx.timelinePoint;
	if (m_hoveredBarIndex >= static_cast<int>(timelinePoint.size()))
		return;

	// 直接使用ctx中预计算的Y轴参数，确保与标签/网格线完全一致
	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice <= 0 || maxPrice <= minPrice || unitY <= 0)
	{
		// 回退：基于涨跌停限制
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	// 重新计算悬停点位置（按索引比例分配X坐标）
	const auto& item = timelinePoint[m_hoveredBarIndex];
	int hoverX = static_cast<int>(ctx.chartWidth / static_cast<float>(timelinePoint.size()) * m_hoveredBarIndex + ctx.chartWidth / static_cast<float>(timelinePoint.size()) / 2);

	int dotY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((item.price - minPrice) * unitY));

	COLORREF dotColor = (item.price >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;

	CPen dotPen(PS_SOLID, 4, dotColor);
	CPen* pOldPen = memDC.SelectObject(&dotPen);
	memDC.Ellipse(hoverX - 3, dotY - 3, hoverX + 3, dotY + 3);

	// K线模式不绘制均价圆点（K线柱本身已显示OHLC）
	if (!m_isKLineMode)
	{
		// 均价圆点
		double avgPrice = item.averagePrice;
		int avgDotY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((avgPrice - minPrice) * unitY));
		COLORREF avgDotColor = (avgPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CPen avgDotPen(PS_SOLID, 4, avgDotColor);
		memDC.SelectObject(&avgDotPen);
		memDC.Ellipse(hoverX - 3, avgDotY - 3, hoverX + 3, avgDotY + 3);
	}

	// 十字竖线（延伸到成交量图或MACD图底部）
	CPen crossPen(PS_DOT, 1, RGB(70, 130, 210));
	memDC.SelectObject(&crossPen);
	memDC.MoveTo(hoverX, ctx.priceChartTop);
	memDC.LineTo(hoverX, ctx.positionY);

	// 十字横虚线：跟随真实价格位置
	memDC.MoveTo(0, dotY);
	memDC.LineTo(ctx.chartWidth, dotY);

	// Y轴左侧预留区域显示悬停点价格，浅蓝色背景高亮
	int yAxisW = g_data.RDPI(50);
	CPoint origOrg = memDC.GetViewportOrg();
	memDC.OffsetViewportOrg(-yAxisW, 0);  // 恢复原始坐标系

	CString hoverPriceStr = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(item.price) : CCommon::FormatFloat(item.price);
	CSize hoverPriceSize = memDC.GetTextExtent(hoverPriceStr);
	int priceLabelX = yAxisW - hoverPriceSize.cx - g_data.RDPI(3);
	int priceLabelY = dotY - hoverPriceSize.cy / 2;
	// 限制在走势图Y轴区域内
	priceLabelY = max(ctx.priceChartTop, min(priceLabelY, ctx.priceChartTop + ctx.priceChartHeight - hoverPriceSize.cy));
	CRect priceBgRect(priceLabelX - g_data.RDPI(2), priceLabelY, priceLabelX + hoverPriceSize.cx + g_data.RDPI(2), priceLabelY + hoverPriceSize.cy);
	memDC.FillSolidRect(priceBgRect, RGB(200, 220, 255));
	memDC.SetTextColor(dotColor);
	memDC.TextOut(priceLabelX, priceLabelY, hoverPriceStr);

	// 恢复视口偏移
	memDC.SetViewportOrg(origOrg);

	// 量柱图横虚线：在悬停量柱顶端画横线，左侧显示成交量
	{
		// 计算可见范围内的最大成交量
		STOCK::Volume maxVol = 0;
		for (const auto& tp : timelinePoint)
		{
			if (tp.volume > maxVol)
				maxVol = tp.volume;
		}
		if (maxVol > 0 && item.volume > 0)
		{
			float volRatio = static_cast<float>(item.volume) / static_cast<float>(maxVol);
			int volBarY = ctx.volumeChartTop + ctx.volumeChartHeight - static_cast<int>(volRatio * ctx.volumeChartHeight);
			// 横虚线
			CPen volCrossPen(PS_DOT, 1, RGB(70, 130, 210));
			memDC.SelectObject(&volCrossPen);
			memDC.MoveTo(0, volBarY);
			memDC.LineTo(ctx.chartWidth, volBarY);

			// 左侧Y轴预留区域显示成交量，浅蓝色背景高亮
			memDC.OffsetViewportOrg(-yAxisW, 0);
			STOCK::Volume volInLots = item.volume / 100;
			CString volLabel = CCommon::FormatVolumeInt(volInLots);
			CSize volLabelSize = memDC.GetTextExtent(volLabel);
			int volLabelX = yAxisW - volLabelSize.cx - g_data.RDPI(3);
			int volLabelY = volBarY - volLabelSize.cy / 2;
			volLabelY = max(ctx.volumeChartTop, min(volLabelY, ctx.volumeChartTop + ctx.volumeChartHeight - volLabelSize.cy));
			CRect volBgRect(volLabelX - g_data.RDPI(2), volLabelY, volLabelX + volLabelSize.cx + g_data.RDPI(2), volLabelY + volLabelSize.cy);
			memDC.FillSolidRect(volBgRect, RGB(200, 220, 255));
			memDC.SetTextColor(COLOR_GRAY_TEXT);
			memDC.TextOut(volLabelX, volLabelY, volLabel);
			memDC.SetViewportOrg(origOrg);
		}
	}

	// 时间标签显示在竖线对应的X轴下方，浅蓝色背景高亮
	CString timeStr;
	if (!item.fullTime.empty() && (m_isMin5KLineMode || m_isMin30KLineMode))
	{
		// 5分/30分K线模式：显示完整日期时间 yyyy-mm-dd hh:mm
		timeStr = CString(item.fullTime.c_str());
		if (timeStr.GetLength() >= 16)
			timeStr = timeStr.Left(16);
	}
	else
	{
		timeStr = CString(item.time.c_str());
		if (timeStr.GetLength() >= 5)
			timeStr = timeStr.Left(5);
	}
	CSize timeSize = memDC.GetTextExtent(timeStr);
	int timeLabelX = hoverX - timeSize.cx / 2;
	timeLabelX = max(g_data.RDPI(2), min(timeLabelX, ctx.chartWidth - timeSize.cx - g_data.RDPI(2)));
	int timeLabelY = ctx.positionY;
	// 浅蓝色背景
	CRect timeBgRect(timeLabelX - g_data.RDPI(3), timeLabelY, timeLabelX + timeSize.cx + g_data.RDPI(3), timeLabelY + timeSize.cy);
	memDC.FillSolidRect(timeBgRect, RGB(220, 235, 250));
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	memDC.TextOut(timeLabelX, timeLabelY, timeStr);

	memDC.SelectObject(pOldPen);
}

// 5分钟K线模式：在走势图区域绘制K线柱（替代分时走势线）
void CFloatingWnd::DrawMin5KLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());

	// Y轴范围：直接使用ctx中预计算的参数，确保与标签/网格线/悬停提示完全一致
	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice <= 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	// 需要从5分钟K线原始数据获取OHLC
	// 5分钟K线数据存储在ctx.klineData中
	const auto& klineData = *ctx.klineData;
	// 需要将可见范围的timelinePoint索引映射到klineData索引
	// 5分钟K线模式下，timelinePoint是从klineData转换的，索引一一对应
	// ctx.startIndex是timelinePoint的起始偏移，对应klineData的ctx.startIndex
	int klineStartIdx = ctx.startIndex;
	int klineEndIdx = klineStartIdx + totalPoints;
	if (klineEndIdx > static_cast<int>(klineData.size()))
		klineEndIdx = static_cast<int>(klineData.size());

	if (klineStartIdx >= static_cast<int>(klineData.size()))
		return;

	// 计算K线柱的宽度和间距
	float barTotalWidth = static_cast<float>(ctx.chartWidth) / totalPoints;
	int barWidth = max(1, static_cast<int>(barTotalWidth * 0.7));
	int gap = static_cast<int>(barTotalWidth) - barWidth;
	if (gap < 1) gap = 1;

	auto priceToY = [&](STOCK::Price price) -> int {
		return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((price - minPrice) * unitY);
		};

	// 最新一天K线背景高亮（5分钟/30分钟K线模式）
	{
		// 从klineData末尾获取最新一天的日期（格式 "YYYY-MM-DD HH:MM"）
		if (!klineData.empty() && (m_isMin5KLineMode || m_isMin30KLineMode))
		{
			const auto& lastKp = klineData.back();
			std::string lastDate;
			auto spacePos = lastKp.day.find(' ');
			if (spacePos != std::string::npos)
				lastDate = lastKp.day.substr(0, spacePos);  // "YYYY-MM-DD"
			else
				lastDate = lastKp.day;

			if (!lastDate.empty())
			{
				// 找到可见范围内属于最新一天的K线索引
				int firstIdx = -1, lastIdx = -1;
				for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
				{
					const auto& kp = klineData[klineStartIdx + i];
					std::string kpDate;
					auto sp = kp.day.find(' ');
					if (sp != std::string::npos)
						kpDate = kp.day.substr(0, sp);
					else
						kpDate = kp.day;
					if (kpDate == lastDate)
					{
						if (firstIdx < 0) firstIdx = i;
						lastIdx = i;
					}
				}

				if (firstIdx >= 0 && lastIdx >= 0)
				{
					// 计算最新一天区间对应的像素范围
					int xLeft = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * firstIdx);
					int xRight = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * (lastIdx + 1));

					// 价格图区域高亮
					CBrush highlightBrush(COLOR_LIGHT_BLUE);
					memDC.FillRect(CRect(xLeft, ctx.priceChartTop, xRight, ctx.priceChartTop + ctx.priceChartHeight), &highlightBrush);

					// 成交量图区域高亮
					memDC.FillRect(CRect(xLeft, ctx.volumeChartTop, xRight, ctx.volumeChartTop + ctx.volumeChartHeight), &highlightBrush);

					// MACD图区域高亮
					memDC.FillRect(CRect(xLeft, ctx.macdChartTop, xRight, ctx.macdChartTop + ctx.macdChartHeight), &highlightBrush);

					// 价格图网格线在调用本函数前已绘制，今日背景会覆盖它；背景绘制后补画一次价格图区网格
					DrawTimelineGridLines(memDC, ctx);
				}
			}
		}
	}

	// 绘制K线柱
	STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

	for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	{
		const auto& kp = klineData[klineStartIdx + i];
		if (kp.close <= 0) continue;

		int centerX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(barTotalWidth / 2);
		int leftX = centerX - barWidth / 2;

		bool isUp = (kp.close >= kp.open);
		COLORREF barColor = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		int openY = priceToY(kp.open);
		int closeY = priceToY(kp.close);
		int highY = priceToY(kp.high);
		int lowY = priceToY(kp.low);

		// 确保坐标在绘图区域内
		openY = max(ctx.priceChartTop, min(openY, ctx.priceChartTop + ctx.priceChartHeight));
		closeY = max(ctx.priceChartTop, min(closeY, ctx.priceChartTop + ctx.priceChartHeight));
		highY = max(ctx.priceChartTop, min(highY, ctx.priceChartTop + ctx.priceChartHeight));
		lowY = max(ctx.priceChartTop, min(lowY, ctx.priceChartTop + ctx.priceChartHeight));

		// 绘制上下影线
		CPen barPen(PS_SOLID, 1, barColor);
		memDC.SelectObject(&barPen);
		memDC.MoveTo(centerX, highY);
		memDC.LineTo(centerX, lowY);

		// 绘制实体
		int bodyTop = min(openY, closeY);
		int bodyBottom = max(openY, closeY);
		int bodyHeight = bodyBottom - bodyTop;
		if (bodyHeight < 1) bodyHeight = 1;

		if (isUp)
		{
			// 阳线：空心或实心红色
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
		else
		{
			// 阴线：实心绿色
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
	}

	// 绘制MA5/MA10/MA20均线（与分时模式一致）
	if (m_showMA)
	{
		const COLORREF ma5Color = RGB(0, 0, 230);
		const COLORREF ma10Color = RGB(0, 166, 235);
		const COLORREF ma20Color = RGB(169, 102, 186);

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(yVal);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带（仅当m_showBollBands开启时绘制）
	if (m_showBollBands)
	{
		const int N = 20;
		const int K = 2;

		const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;

		std::vector<double> upperBand(totalPoints, 0);
		std::vector<double> middleBand(totalPoints, 0);
		std::vector<double> lowerBand(totalPoints, 0);

		for (int i = 0; i < totalPoints; i++)
		{
			int globalIdx = ctx.startIndex + i;
			if (globalIdx < N - 1)
			{
				upperBand[i] = middleBand[i] = lowerBand[i] = 0;
				continue;
			}
			double sum = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				sum += fullData[j].price;
			}
			double ma = sum / N;
			double variance = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				double diff = fullData[j].price - ma;
				variance += diff * diff;
			}
			double stddev = std::sqrt(variance / N);
			middleBand[i] = ma;
			upperBand[i] = ma + K * stddev;
			lowerBand[i] = ma - K * stddev;
		}

		auto bandPriceToY = [&](double price) -> int {
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			};

		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_SOLID, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				if (band[i] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				int py = bandPriceToY(band[i]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(0, 0, 230));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 振幅上下线（仅当m_showAmplitudeBands开启时绘制）
	if (m_showAmplitudeBands)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		auto* dayKLineObj = stockData ? stockData->getKLineData() : nullptr;
		double avgAmplitude = dayKLineObj ? dayKLineObj->CalculateAverageAmplitude(5) : 0;
		if (avgAmplitude > 0)
		{
			double ampRatio = avgAmplitude / 100.0 / 2.0;

			auto priceToY = [&](double price) -> int {
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
				return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
				};

			// 绘制振幅上轨曲线（红色）
			{
				CPen upperPen(PS_SOLID, 1, COLOR_RED_UP);
				memDC.SelectObject(&upperPen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					double avgP = timelinePoint[i].averagePrice;
					if (avgP <= 0) { first = true; continue; }
					double upperPrice = avgP * (1 + ampRatio);
					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int py = priceToY(upperPrice);
					if (first) { memDC.MoveTo(pointX, py); first = false; }
					else { memDC.LineTo(pointX, py); }
				}
			}
			// 绘制振幅下轨曲线（绿色）
			{
				CPen lowerPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
				memDC.SelectObject(&lowerPen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					double avgP = timelinePoint[i].averagePrice;
					if (avgP <= 0) { first = true; continue; }
					double lowerPrice = avgP * (1 - ampRatio);
					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int py = priceToY(lowerPrice);
					if (first) { memDC.MoveTo(pointX, py); first = false; }
					else { memDC.LineTo(pointX, py); }
				}
			}

			double lastAvgP = timelinePoint.back().averagePrice;
			if (lastAvgP > 0)
			{
				CString upperLabel, lowerLabel;
				upperLabel.Format(_T("%.2f"), lastAvgP * (1 + ampRatio));
				lowerLabel.Format(_T("%.2f"), lastAvgP * (1 - ampRatio));
				int labelX = ctx.chartLeft + ctx.chartWidth + 2;
				int upperY = priceToY(lastAvgP * (1 + ampRatio));
				int lowerY = priceToY(lastAvgP * (1 - ampRatio));
				memDC.SetTextColor(COLOR_RED_UP);
				memDC.TextOut(labelX, upperY - memDC.GetTextExtent(upperLabel).cy / 2, upperLabel);
				memDC.SetTextColor(COLOR_GREEN_DOWN);
				memDC.TextOut(labelX, lowerY - memDC.GetTextExtent(lowerLabel).cy / 2, lowerLabel);
			}
		}
	}

	// 最高/最低价标签
	{
		STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
		int hiIdx = -1, loIdx = -1;
		for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
		{
			const auto& kp = klineData[klineStartIdx + i];
			if (kp.high > 0)
			{
				if (kp.high > hiPrice) { hiPrice = kp.high; hiIdx = i; }
				if (kp.high >= hiPrice) { hiPrice = kp.high; hiIdx = i; }
			}
			if (kp.low > 0)
			{
				if (kp.low < loPrice) { loPrice = kp.low; loIdx = i; }
				if (kp.low <= loPrice) { loPrice = kp.low; loIdx = i; }
			}
		}

		// 最高价标签
		if (hiIdx >= 0 && hiPrice > 0)
		{
			int hiX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * hiIdx) + static_cast<int>(barTotalWidth / 2);
			int hiY = priceToY(hiPrice);
			DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				hiPrice, true, COLOR_RED_UP);
		}

		// 最低价标签
		if (loIdx >= 0 && loPrice > 0 && loIdx != hiIdx)
		{
			int loX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * loIdx) + static_cast<int>(barTotalWidth / 2);
			int loY = priceToY(loPrice);
			DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				loPrice, false, COLOR_GREEN_DOWN);
		}
	}

	// 智能分析买卖点标记（仅5分钟K线模式，30分钟K线模式不绘制）
	if (m_showT0Markers && m_isMin5KLineMode && !m_isMin30KLineMode)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			auto min5KLineObj = stockData->getMin5KLineData();
			auto min30KLineObj = stockData->getMin30KLineData();
			if (min5KLineObj && min5KLineObj->data.size() >= 120 &&
				min30KLineObj && min30KLineObj->data.size() >= 22)
			{
				std::vector<STOCK::Bar> bars5, bars30;
				bars5.reserve(min5KLineObj->data.size());
				for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));
				bars30.reserve(min30KLineObj->data.size());
				for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

				auto signals = CSignalAnalyzer::BatchDetectSignals(bars5, bars30);
				int oldBkMode = memDC.SetBkMode(TRANSPARENT);
				auto drawSignalArrow = [&](int x, int fromY, int toY, COLORREF color) {
					CPen pen(PS_SOLID, 1, color);
					CPen* pOldP = memDC.SelectObject(&pen);
					memDC.MoveTo(x, fromY);
					memDC.LineTo(x, toY);

					int dir = (toY >= fromY) ? 1 : -1;
					int arrowLen = g_data.RDPI(4);
					int arrowHalf = g_data.RDPI(3);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x - arrowHalf, toY - dir * arrowLen);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x + arrowHalf, toY - dir * arrowLen);
					memDC.SelectObject(pOldP);
					};

				for (const auto& sig : signals)
				{
					int klineIdx = sig.barIndex;
					// 只绘制可见范围内的信号
					if (klineIdx < klineStartIdx || klineIdx >= klineEndIdx)
						continue;

					int visibleIdx = klineIdx - klineStartIdx;
					int barX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * visibleIdx) + static_cast<int>(barTotalWidth / 2);

					if (sig.isForbid)
					{
						int barY = priceToY(klineData[klineIdx].close);
						CPen pen(PS_SOLID, 2, COLOR_GRAY_TEXT);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						memDC.MoveTo(barX - r, barY - r); memDC.LineTo(barX + r, barY + r);
						memDC.MoveTo(barX + r, barY - r); memDC.LineTo(barX - r, barY + r);
						memDC.SelectObject(pOldP);
						continue;
					}

					if (sig.isBuy)
					{
						int barY = priceToY(klineData[klineIdx].low) + g_data.RDPI(2);
						CBrush brush(COLOR_GREEN_DOWN);
						CPen pen(PS_SOLID, 1, COLOR_GREEN_DOWN);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						int labelOff = g_data.RDPI(8);
						memDC.Ellipse(barX - r, barY, barX + r, barY + 2 * r);
						memDC.SetTextColor(COLOR_GREEN_DOWN);
						CSize sz = memDC.GetTextExtent(sig.reason);
						int labelY = barY + 2 * r + labelOff;
						memDC.TextOut(barX - sz.cx / 2, labelY, sig.reason);
						drawSignalArrow(barX, labelY, barY + 2 * r, COLOR_GREEN_DOWN);
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
					else
					{
						int barY = priceToY(klineData[klineIdx].high) - g_data.RDPI(2);
						CBrush brush(COLOR_RED_UP);
						CPen pen(PS_SOLID, 1, COLOR_RED_UP);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						int labelOff = g_data.RDPI(8);
						memDC.Ellipse(barX - r, barY - 2 * r, barX + r, barY);
						memDC.SetTextColor(COLOR_RED_UP);
						CSize sz = memDC.GetTextExtent(sig.reason);
						int labelY = barY - 2 * r - sz.cy - labelOff;
						memDC.TextOut(barX - sz.cx / 2, labelY, sig.reason);
						drawSignalArrow(barX, labelY + sz.cy, barY - 2 * r, COLOR_RED_UP);
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
				}
				memDC.SetBkMode(oldBkMode);
			}
		}
	}
}

// 日K线模式：在走势图区域绘制K线柱（替代分时走势线）
void CFloatingWnd::DrawDayKLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());

	// Y轴范围：直接使用ctx中预计算的参数，确保与标签/网格线/悬停提示完全一致
	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice <= 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	// 从日K线原始数据获取OHLC
	const auto& klineData = *ctx.klineData;
	int klineStartIdx = ctx.startIndex;
	int klineEndIdx = klineStartIdx + totalPoints;
	if (klineEndIdx > static_cast<int>(klineData.size()))
		klineEndIdx = static_cast<int>(klineData.size());

	if (klineStartIdx >= static_cast<int>(klineData.size()))
		return;

	// 计算K线柱的宽度和间距
	float barTotalWidth = static_cast<float>(ctx.chartWidth) / totalPoints;
	int barWidth = max(1, static_cast<int>(barTotalWidth * 0.7));
	int gap = static_cast<int>(barTotalWidth) - barWidth;
	if (gap < 1) gap = 1;

	auto priceToY = [&](STOCK::Price price) -> int {
		return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((price - minPrice) * unitY);
		};

	// 绘制K线柱
	STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

	for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	{
		const auto& kp = klineData[klineStartIdx + i];
		if (kp.close <= 0) continue;

		int centerX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(barTotalWidth / 2);
		int leftX = centerX - barWidth / 2;

		bool isUp = (kp.close >= kp.open);
		COLORREF barColor = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		int openY = priceToY(kp.open);
		int closeY = priceToY(kp.close);
		int highY = priceToY(kp.high);
		int lowY = priceToY(kp.low);

		// 确保坐标在绘图区域内
		openY = max(ctx.priceChartTop, min(openY, ctx.priceChartTop + ctx.priceChartHeight));
		closeY = max(ctx.priceChartTop, min(closeY, ctx.priceChartTop + ctx.priceChartHeight));
		highY = max(ctx.priceChartTop, min(highY, ctx.priceChartTop + ctx.priceChartHeight));
		lowY = max(ctx.priceChartTop, min(lowY, ctx.priceChartTop + ctx.priceChartHeight));

		// 绘制上下影线
		CPen barPen(PS_SOLID, 1, barColor);
		memDC.SelectObject(&barPen);
		memDC.MoveTo(centerX, highY);
		memDC.LineTo(centerX, lowY);

		// 绘制实体
		int bodyTop = min(openY, closeY);
		int bodyBottom = max(openY, closeY);
		int bodyHeight = bodyBottom - bodyTop;
		if (bodyHeight < 1) bodyHeight = 1;

		if (isUp)
		{
			// 阳线：空心红色（只画边框）
			CBrush* pOldBrush = memDC.SelectObject((CBrush*)CBrush::FromHandle((HBRUSH)GetStockObject(NULL_BRUSH)));
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
		else
		{
			// 阴线：实心绿色
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
	}

	// 绘制MA5/MA10/MA20均线
	if (m_showMA)
	{
		const COLORREF ma5Color = RGB(0, 0, 230);
		const COLORREF ma10Color = RGB(0, 166, 235);
		const COLORREF ma20Color = RGB(169, 102, 186);

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(yVal);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带
	if (m_showBollBands)
	{
		const int N = 20;
		const int K = 2;

		const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;

		std::vector<double> upperBand(totalPoints, 0);
		std::vector<double> middleBand(totalPoints, 0);
		std::vector<double> lowerBand(totalPoints, 0);

		for (int i = 0; i < totalPoints; i++)
		{
			int globalIdx = ctx.startIndex + i;
			if (globalIdx < N - 1)
			{
				upperBand[i] = middleBand[i] = lowerBand[i] = 0;
				continue;
			}
			double sum = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				sum += fullData[j].price;
			}
			double ma = sum / N;
			double variance = 0;
			for (int j = globalIdx - N + 1; j <= globalIdx; j++)
			{
				double diff = fullData[j].price - ma;
				variance += diff * diff;
			}
			double stddev = std::sqrt(variance / N);
			middleBand[i] = ma;
			upperBand[i] = ma + K * stddev;
			lowerBand[i] = ma - K * stddev;
		}

		auto bandPriceToY = [&](double price) -> int {
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			};

		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_SOLID, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				if (band[i] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				int py = bandPriceToY(band[i]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(0, 0, 230));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 最高/最低价标签
	{
		STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
		int hiIdx = -1, loIdx = -1;
		for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
		{
			const auto& kp = klineData[klineStartIdx + i];
			if (kp.high > 0)
			{
				if (kp.high > hiPrice) { hiPrice = kp.high; hiIdx = i; }
			}
			if (kp.low > 0)
			{
				if (kp.low < loPrice) { loPrice = kp.low; loIdx = i; }
			}
		}

		// 最高价标签
		if (hiIdx >= 0 && hiPrice > 0)
		{
			int hiX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * hiIdx) + static_cast<int>(barTotalWidth / 2);
			int hiY = priceToY(hiPrice);
			DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				hiPrice, true, COLOR_RED_UP);
		}

		// 最低价标签
		if (loIdx >= 0 && loPrice > 0 && loIdx != hiIdx)
		{
			int loX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * loIdx) + static_cast<int>(barTotalWidth / 2);
			int loY = priceToY(loPrice);
			DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				loPrice, false, COLOR_GREEN_DOWN);
		}
	}
}

void CFloatingWnd::DrawTimelineVolumeSection(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawVolumeChart(memDC, 0, ctx.volumeChartTop, ctx.chartWidth, ctx.volumeChartHeight, *ctx.timelinePoint, &ctx.realtimeData);

	// 5分钟K线量柱图：绘制成交量MA5/MA10（最近5/10根5分钟K成交量算术平均）
	if (m_isMin5KLineMode && ctx.fullTimeline && !ctx.fullTimeline->empty() && ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		const auto& fullData = *ctx.fullTimeline;
		const auto& visibleData = *ctx.timelinePoint;
		const int totalPoints = static_cast<int>(visibleData.size());
		const int fullCount = static_cast<int>(fullData.size());
		const int startIndex = ctx.startIndex;
		int endIndex = min(fullCount, startIndex + totalPoints);

		STOCK::Volume maxVolume = 0;
		for (int i = startIndex; i < endIndex; i++)
		{
			maxVolume = max(maxVolume, fullData[i].volume);
		}

		auto calcVolumeMA = [&](int globalIndex, int period) -> double {
			if (globalIndex < period - 1)
				return 0.0;

			double sum = 0.0;
			for (int i = globalIndex - period + 1; i <= globalIndex; i++)
				sum += static_cast<double>(fullData[i].volume);
			return sum / period;
			};

		std::vector<double> ma5(totalPoints, 0.0);
		std::vector<double> ma10(totalPoints, 0.0);
		for (int i = 0; i < totalPoints; i++)
		{
			int globalIndex = startIndex + i;
			if (globalIndex >= 0 && globalIndex < fullCount)
			{
				ma5[i] = calcVolumeMA(globalIndex, 5);
				ma10[i] = calcVolumeMA(globalIndex, 10);
			}
		}

		if (maxVolume > 0)
		{
			auto volumeToY = [&](double volume) -> int {
				int py = ctx.volumeChartTop + ctx.volumeChartHeight - static_cast<int>(volume / static_cast<double>(maxVolume) * ctx.volumeChartHeight);
				return max(ctx.volumeChartTop, min(py, ctx.volumeChartTop + ctx.volumeChartHeight));
				};

			auto drawVolumeMALine = [&](const std::vector<double>& values, COLORREF color) {
				CPen pen(PS_SOLID, 1, color);
				CPen* pOldPen = memDC.SelectObject(&pen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					if (values[i] <= 0)
					{
						first = true;
						continue;
					}

					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int pointY = volumeToY(values[i]);
					if (first)
					{
						memDC.MoveTo(pointX, pointY);
						first = false;
					}
					else
					{
						memDC.LineTo(pointX, pointY);
					}
				}
				memDC.SelectObject(pOldPen);
				};

			drawVolumeMALine(ma5, RGB(0, 0, 230));
			drawVolumeMALine(ma10, RGB(0, 166, 235));
		}
	}

	// 时间竖线和平均值参考线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldVolPen = memDC.SelectObject(&pGrid);

	int volumeY = ctx.volumeChartTop;
	memDC.MoveTo(0, volumeY);
	memDC.LineTo(ctx.chartWidth, volumeY);

	const auto& timelinePoint = *ctx.timelinePoint;
	if (!timelinePoint.empty())
	{
		// 计算可见范围内的最大成交量
		STOCK::Volume maxVolume = 0;
		for (const auto& item : timelinePoint)
		{
			if (item.volume > maxVolume)
				maxVolume = item.volume;
		}
		if (maxVolume > 0)
		{
			// 均分3等分，画2根点线
			CPen dotPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&dotPen);
			memDC.SetTextColor(COLOR_GRAY_TEXT);
			int volumeY = ctx.volumeChartTop;
			int yAxisWidth = g_data.RDPI(50);
			for (int i = 1; i <= 2; i++)
			{
				int yPos = volumeY + ctx.volumeChartHeight * i / 3;
				memDC.MoveTo(0, yPos);
				memDC.LineTo(ctx.chartWidth, yPos);

				// 左侧Y轴预留区域显示成交量数字（手），视口已偏移，用负坐标回到预留区
				STOCK::Volume volAtLine = maxVolume * (3 - i) / 3;
				STOCK::Volume volInLots = volAtLine / 100;
				CString volLabel = CCommon::FormatVolumeInt(volInLots);
				CSize labelSize = memDC.GetTextExtent(volLabel);
				memDC.TextOut(-labelSize.cx - g_data.RDPI(3), yPos - labelSize.cy / 2, volLabel);
			}
			memDC.SelectObject(&pGrid);
		}
	}

	// X轴时间竖线：4等分可见数据范围
	if (ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		const int totalPts = static_cast<int>(ctx.timelinePoint->size());
		const int numVLines = 4;
		for (int i = 0; i <= numVLines; i++)
		{
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, volumeY);
			memDC.LineTo(xPos, volumeY + ctx.volumeChartHeight);
		}
	}
	memDC.SelectObject(pOldVolPen);
}

void CFloatingWnd::DrawTimelinePositionInfo(CDC& memDC, const TimelineDrawContext& ctx)
{
	double costPrice = g_data.GetCostPrice(m_stock_id);
	double holdingCount = g_data.GetHoldingCount(m_stock_id);

	CString countValue;
	if (holdingCount > 0)
		countValue = CCommon::FormatVolumeInt(holdingCount);
	else
		countValue = _T("--");

	STOCK::StockData::PositionInfo posInfo = {};
	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	if (stockDataPtr)
		posInfo = stockDataPtr->CalculatePositionInfo(costPrice, holdingCount);

	CString totalCostValue;
	if (posInfo.totalCost > 0)
		totalCostValue = CCommon::FormatAmount(posInfo.totalCost);
	else
		totalCostValue = _T("--");

	CString marketValueValue;
	if (posInfo.marketValue > 0)
		marketValueValue = CCommon::FormatAmount(posInfo.marketValue);
	else
		marketValueValue = _T("--");

	CString profitLossValue;
	COLORREF profitLossColor = COLOR_BLACK;
	if (posInfo.totalCost > 0)
	{
		profitLossColor = posInfo.profitLoss >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		profitLossValue = CCommon::FormatProfitLoss(posInfo.profitLossPercent, posInfo.profitLoss, true);
	}
	else
		profitLossValue = _T("--");

	CString todayProfitLossValue;
	COLORREF todayProfitLossColor = COLOR_BLACK;
	if (holdingCount > 0 && ctx.realtimeData.prevClosePrice != 0)
	{
		double todayProfitLoss = (ctx.realtimeData.currentPrice - ctx.realtimeData.prevClosePrice) * holdingCount;
		double todayProfitLossPercent = (ctx.realtimeData.currentPrice - ctx.realtimeData.prevClosePrice) / ctx.realtimeData.prevClosePrice * 100;
		todayProfitLossColor = todayProfitLoss >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		todayProfitLossValue = CCommon::FormatProfitLoss(todayProfitLossPercent, todayProfitLoss, true);
	}
	else
		todayProfitLossValue = _T("--");

	struct InfoItem {
		CString label;
		CString value;
		COLORREF labelColor;
		COLORREF valueColor;
	};

	InfoItem items[] = {
		{ _T("持仓:"), countValue, COLOR_BLACK, COLOR_BLACK },
		{ _T(" 成本:"), totalCostValue, COLOR_BLACK, COLOR_BLACK },
		{ _T(" 市值:"), marketValueValue, COLOR_BLACK, COLOR_BLACK },
		{ _T(" 盈亏:"), profitLossValue, COLOR_BLACK, profitLossColor },
		{ _T(" 当日盈亏:"), todayProfitLossValue, COLOR_BLACK, todayProfitLossColor }
	};

	int totalWidth = 0;
	for (const auto& item : items)
		totalWidth += memDC.GetTextExtent(item.label).cx + memDC.GetTextExtent(item.value).cx;

	int startX = max(g_data.RDPI(5), (ctx.chartWidth - totalWidth) / 2);
	int currentX = startX;

	for (const auto& item : items)
	{
		memDC.SetTextColor(item.labelColor);
		memDC.TextOut(currentX, ctx.positionY, item.label);
		currentX += memDC.GetTextExtent(item.label).cx;

		memDC.SetTextColor(item.valueColor);
		memDC.TextOut(currentX, ctx.positionY, item.value);
		currentX += memDC.GetTextExtent(item.value).cx;
	}
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
	// 大盘在K线模式下不显示盘口（5分钟K线模式也设置m_isKLineMode=true，所以这里自动覆盖）
	bool isIndexKLine = isIndex && m_isKLineMode;

	const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;  // 左侧股票列表面板宽度
	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = w - orderBookWidth;
	// 左侧Y轴坐标区域宽度（所有图表统一预留）
	const int yAxisWidth = g_data.RDPI(50);

	const int headerHeight = g_data.RDPI(26);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int indexBarHeight = g_data.RDPI(20);  // 底部大盘指数状态栏高度

	// 统一布局：标题栏 + 走势图(1/2) + 量柱图(1/4) + 副图MACD/KDJ(1/4) + 时间标签 + 底部指数状态栏
	int chartArea = h - headerHeight - xAxisLabelHeight - indexBarHeight;
	int priceChartHeight, volumeChartHeight;
	if (m_expandedMode)
	{
		priceChartHeight = chartArea * 3 / 4;
		volumeChartHeight = chartArea / 4;
	}
	else
	{
		priceChartHeight = chartArea / 2;
		volumeChartHeight = chartArea / 4;
	}

	const int priceChartTop = headerHeight;

	STOCK::StockInfo realtimeData;
	STOCK::ChipDistribution chipData;
	std::vector<STOCK::TimelinePoint> timelinePoint;
	std::vector<STOCK::KLinePoint> klineData;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			realtimeData = stockData->info;
			chipData = stockData->chipDistribution;
			if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
			{
				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					klineData = klineObj->data;
					// 将日K线数据转换为TimelinePoint格式，复用分时绘制流程
					for (const auto& kp : klineObj->data)
					{
						STOCK::TimelinePoint tp;
						// 日K线 day 格式为 "YYYY-MM-DD"，取后5位 "MM-DD" 作为X轴标签
						if (kp.day.length() >= 10)
							tp.time = kp.day.substr(5, 5);  // "MM-DD"
						else
							tp.time = kp.day;
						tp.price = kp.close;
						tp.averagePrice = kp.close;  // 日K线无分时均价，暂用收盘价
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_isMin5KLineMode)
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
						tp.averagePrice = kp.close;  // 暂用收盘价
						tp.volume = kp.volume;
						tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
						timelinePoint.push_back(tp);
					}
				}
			}
			else if (m_isMin30KLineMode)
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
		}
	}

	if (!m_isOverviewMode)
	{
		// 始终隐藏滚动条（统一布局不再需要）
		if (m_hScrollBar.GetSafeHwnd())
			m_hScrollBar.ShowWindow(SW_HIDE);

		// 数据加载前也先把顶部按钮定位到目标标题栏，避免停留在初始坐标
		{
			const int titleH = g_data.RDPI(16);
			int toolBtnW = g_data.RDPI(34);
			int toolBtnH = min(titleH, g_data.RDPI(16));
			int toolTop = priceChartTop + (titleH - toolBtnH) / 2;
			int obBtnW = g_data.RDPI(34);
			int t0Start = w - obBtnW * 2 - toolBtnW * 3;
			SafeSetWindowPos(m_btnBoll, t0Start + toolBtnW * 2, toolTop, toolBtnW, toolBtnH);
			SafeSetWindowPos(m_btnMA, t0Start + toolBtnW, toolTop, toolBtnW, toolBtnH);
			SafeSetWindowPos(m_btnT0, t0Start, toolTop, toolBtnW, toolBtnH);

			int closeBtnW = g_data.RDPI(20);
			int closeBtnH = g_data.RDPI(18);
			int headerBtnTop = g_data.RDPI(2);
			SafeSetWindowPos(m_btnClose, w - closeBtnW, headerBtnTop, closeBtnW, closeBtnH);
			SafeSetWindowPos(m_btnExpand, w - closeBtnW * 2, headerBtnTop, closeBtnW, closeBtnH);
			SafeSetWindowPos(m_btnToggleStockList, w - closeBtnW * 3, headerBtnTop, closeBtnW, closeBtnH);
			// 筹码峰/盘口按钮定位到盘口标题栏，高度与BOLL按钮一致
			int obTitleH = g_data.RDPI(16);
			int obBtnH = min(obTitleH, g_data.RDPI(16));
			int obBtnTop = headerHeight + (obTitleH - obBtnH) / 2;
			bool showObBtns = !isIndexKLine;
			SafeSetWindowPos(m_btnChipPeak, w - obBtnW, obBtnTop, obBtnW, obBtnH);
			SafeShowWindow(m_btnChipPeak, showObBtns);
			SafeSetWindowPos(m_btnOrderBook, w - obBtnW * 2, obBtnTop, obBtnW, obBtnH);
			SafeShowWindow(m_btnOrderBook, showObBtns);
		}

		// 左侧股票列表面板（无论分时数据是否加载都绘制）
		if (m_showStockList)
			DrawStockListPanel(memDC, 0, headerHeight, stockListWidth, h - headerHeight - indexBarHeight, m_stock_id);

		if (!timelinePoint.empty())
		{
			// 先基于完整分时数据计算MA，避免缩放/拖动后只用可见区间导致均线与其他APP不一致
			CalcAllRollingAvgPrices(timelinePoint);

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
			int origVolTop = priceChartTop + priceChartHeight;
			int origMacdTop = origVolTop + volumeChartHeight;
			ctx.priceChartTop = origPriceTop + titleH;
			ctx.priceChartHeight = priceChartHeight - titleH;
			ctx.volumeChartTop = origVolTop + titleH;
			ctx.volumeChartHeight = volumeChartHeight - titleH;
			ctx.macdChartTop = origMacdTop + titleH;
			ctx.macdChartHeight = volumeChartHeight - titleH;
			// 时间标签位置：放大模式下在成交量图下方，否则在副图下方
			ctx.positionY = (m_expandedMode ? origVolTop : origMacdTop) + volumeChartHeight + g_data.RDPI(2);
			ctx.realtimeData = realtimeData;
			ctx.timelinePoint = &subTimeline;
			ctx.fullTimeline = &timelinePoint;  // 完整分时数据，供布林带等指标回溯
			ctx.startIndex = startIndex;
			ctx.visibleCount = visibleCount;
			ctx.klineData = &klineData;

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
					if (!m_isKLineMode && tp.averagePrice > 0)
					{
						visMax = (std::max)(visMax, tp.averagePrice);
						visMin = (std::min)(visMin, tp.averagePrice);
					}
				}
				// K线模式：Y轴范围需要包含K线柱的high/low
				if (m_isKLineMode && ctx.klineData)
				{
					const auto& klineRef = *ctx.klineData;
					for (int i = 0; i < visibleCount && (startIndex + i) < static_cast<int>(klineRef.size()); i++)
					{
						const auto& kp = klineRef[startIndex + i];
						if (kp.high > 0)
						{
							visMax = (std::max)(visMax, kp.high);
							visMin = (std::min)(visMin, kp.low > 0 ? kp.low : kp.close);
						}
					}
				}

				// K线模式开启BOLL时，Y轴范围同时包含可见区间的布林上下轨，避免涨停等窄幅区间下BOLL被映射到价格图区外
				if (m_isKLineMode && m_showBollBands && !timelinePoint.empty())
				{
					const int N = 20;
					const int K = 2;
					const int totalCount = static_cast<int>(timelinePoint.size());
					for (int i = 0; i < visibleCount && (startIndex + i) < totalCount; i++)
					{
						int globalIdx = startIndex + i;
						if (globalIdx < N - 1)
							continue;

						double sum = 0;
						for (int j = globalIdx - N + 1; j <= globalIdx; j++)
							sum += timelinePoint[j].price;
						double ma = sum / N;

						double variance = 0;
						for (int j = globalIdx - N + 1; j <= globalIdx; j++)
						{
							double diff = timelinePoint[j].price - ma;
							variance += diff * diff;
						}
						double stddev = std::sqrt(variance / N);
						double upperBand = ma + K * stddev;
						double lowerBand = ma - K * stddev;
						if (upperBand > 0)
							visMax = (std::max)(visMax, upperBand);
						if (lowerBand > 0)
							visMin = (std::min)(visMin, lowerBand);
					}
				}
				if (visMin == (std::numeric_limits<STOCK::Price>::max)() || visMax <= visMin)
				{
					// 数据无效，回退到涨跌停范围
					STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
					visMax = ctx.realtimeData.prevClosePrice + priceLimit;
					visMin = ctx.realtimeData.prevClosePrice - priceLimit;
				}

				// Y轴固定6等分7根横线：至少保留上下各1格边距，最小刻度0.001
				// 先把轴边界对齐到实际显示的价格刻度，再让网格线、标签、曲线共用同一组刻度值，避免标签四舍五入后与曲线位置错位
				const double DIV_COUNT = 6.0;
				const double MIN_STEP = 0.001;
				double range = visMax - visMin;
				if (range <= 0) range = MIN_STEP;

				double rawStep = range / (DIV_COUNT - 2.0);
				if (rawStep <= 0) rawStep = MIN_STEP;
				double mag = pow(10.0, floor(log10(rawStep)));
				double norm = rawStep / mag;
				double niceStep;
				if (norm <= 1.0) niceStep = 1.0 * mag;
				else if (norm <= 2.0) niceStep = 2.0 * mag;
				else if (norm <= 5.0) niceStep = 5.0 * mag;
				else niceStep = 10.0 * mag;
				niceStep = (std::max)(niceStep, MIN_STEP);

				double centerPrice = (visMin + visMax) / 2.0;
				double axisRange = DIV_COUNT * niceStep;
				double centeredMin = centerPrice - axisRange / 2.0;
				double lowerBound = visMax - (DIV_COUNT - 1.0) * niceStep;
				double upperBound = visMin - niceStep;
				double axisMin = round(centeredMin / niceStep) * niceStep;
				if (axisMin < lowerBound)
					axisMin = ceil((lowerBound - niceStep * 1e-9) / niceStep) * niceStep;
				if (axisMin > upperBound)
					axisMin = floor((upperBound + niceStep * 1e-9) / niceStep) * niceStep;

				// 与FormatFloat的三位小数显示精度保持一致，确保标签值就是网格线和曲线映射使用的实际值
				axisMin = round(axisMin * 1000.0) / 1000.0;
				double axisMax = round((axisMin + axisRange) * 1000.0) / 1000.0;

				ctx.maxPrice = axisMax;
				ctx.minPrice = axisMin;
				ctx.niceStep = niceStep;
				ctx.unitY = ctx.priceChartHeight / (ctx.maxPrice - ctx.minPrice);
			}

			// 使用视口偏移让分时图所有绘制自动向右偏移 stockListWidth + yAxisWidth，实现左侧股票列表和Y轴留白
			memDC.SaveDC();
			memDC.OffsetViewportOrg(stockListWidth + yAxisWidth, 0);

			DrawTimelineHeader(memDC, ctx);
			DrawTimelineGridAndLines(memDC, ctx);
			if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
			{
				// 日K线模式：根据m_showTrendView决定绘制K线柱还是走势线
				if (m_showTrendView)
					DrawTimelinePriceCurve(memDC, ctx);
				else
					DrawDayKLinePriceChart(memDC, ctx);
			}
			else if (m_isMin5KLineMode)
				DrawMin5KLinePriceChart(memDC, ctx);
			else if (m_isMin30KLineMode)
				DrawMin5KLinePriceChart(memDC, ctx);  // 30分钟K线复用5分钟K线绘制函数
			else
				DrawTimelinePriceCurve(memDC, ctx);
			DrawTimelineVolumeSection(memDC, ctx);
			if (!m_expandedMode)
			{
				if (m_timelineIndicator == TimelineIndicator::KDJ)
					DrawTimelineKDJSection(memDC, ctx);
				else if (m_timelineIndicator == TimelineIndicator::WR)
					DrawTimelineWRSection(memDC, ctx);
				else if (m_timelineIndicator == TimelineIndicator::RSI)
					DrawTimelineRSISection(memDC, ctx);
				else
					DrawTimelineMACDSection(memDC, ctx);
			}
			else
			{
				// 放大模式下：在成交量图下方绘制X轴时间标签
				const auto& timelinePoint = *ctx.timelinePoint;
				const int totalPts = static_cast<int>(timelinePoint.size());
				const int numVLines = 4;
				int timeLabelY = origVolTop + volumeChartHeight + g_data.RDPI(2);
				memDC.SetTextColor(COLOR_GRAY_TEXT);
				if (totalPts > 0)
				{
					for (int i = 0; i <= numVLines; i++)
					{
						int idx = totalPts * i / numVLines;
						if (idx >= totalPts) idx = totalPts - 1;
						int xPos = ctx.chartWidth * i / numVLines;
						CString timeLabel(timelinePoint[idx].time.c_str());
						if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
						CSize labelSize = memDC.GetTextExtent(timeLabel);
						int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
						memDC.TextOut(labelX, timeLabelY, timeLabel);
					}
				}
			}
			// 标题栏用原始 top 绘制（在图表绘图区上方）
			DrawTimelineTitleBars(memDC, ctx, origPriceTop, origVolTop, m_expandedMode ? -1 : origMacdTop, titleH);
			DrawTimelineHoverOverlay(memDC, ctx);

			memDC.RestoreDC(-1);

			// 定位缩放按钮（量柱图标题栏最右侧，原始坐标系）
			{
				int zoomBtnW = g_data.RDPI(28);
				int zoomBtnH = g_data.RDPI(16);
				int zoomGap = g_data.RDPI(2);
				int rightEdge = chartWidth;
				int btnTop = origVolTop + (titleH - zoomBtnH) / 2;
				SafeSetWindowPos(m_btnZoomIn, rightEdge - zoomBtnW - zoomGap, btnTop, zoomBtnW, zoomBtnH);
				SafeSetWindowPos(m_btnZoomOut, rightEdge - zoomBtnW * 2 - zoomGap * 2, btnTop, zoomBtnW, zoomBtnH);
			}

			// T0/MA/BOLL按钮放到走势图标题栏右侧，与筹码/盘口按钮挨在一起
			{
				int obBtnW = g_data.RDPI(34);
				int toolBtnW = g_data.RDPI(34);
				int toolBtnH = min(titleH, g_data.RDPI(16));
				int btnTop = origPriceTop + (titleH - toolBtnH) / 2;
				// 筹码/盘口占2个obBtnW，T0/MA/BOLL紧接其左
				int t0Start = w - obBtnW * 2 - toolBtnW * 3;
				SafeSetWindowPos(m_btnBoll, t0Start + toolBtnW * 2, btnTop, toolBtnW, toolBtnH);
				SafeSetWindowPos(m_btnMA, t0Start + toolBtnW, btnTop, toolBtnW, toolBtnH);
				SafeSetWindowPos(m_btnT0, t0Start, btnTop, toolBtnW, toolBtnH);
			}

			// 主标题栏右侧按钮定位（关闭按钮、放大按钮、股票列表切换按钮）
			{
				int closeBtnW = g_data.RDPI(20);
				int closeBtnH = g_data.RDPI(18);
				int top = g_data.RDPI(2);
				SafeSetWindowPos(m_btnClose, w - closeBtnW, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnExpand, w - closeBtnW * 2, top, closeBtnW, closeBtnH);
				SafeSetWindowPos(m_btnToggleStockList, w - closeBtnW * 3, top, closeBtnW, closeBtnH);
			}

			// 盘口标题栏右侧按钮定位（筹码峰、盘口按钮），高度与BOLL按钮一致
			{
				int obTitleH = g_data.RDPI(16);
				int obBtnW = g_data.RDPI(34);
				int obBtnH = min(obTitleH, g_data.RDPI(16));
				int obBtnTop = headerHeight + (obTitleH - obBtnH) / 2;
				bool showObBtns = !isIndexKLine;
				SafeSetWindowPos(m_btnChipPeak, w - obBtnW, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnChipPeak, showObBtns);
				SafeSetWindowPos(m_btnOrderBook, w - obBtnW * 2, obBtnTop, obBtnW, obBtnH);
				SafeShowWindow(m_btnOrderBook, showObBtns);
			}

			// MACD/KDJ/W&R/RSI按钮（左侧Y轴预留区域，占用副图到X轴时间标签区域，不超过底部状态栏）
			{
				if (m_expandedMode)
				{
					SafeShowWindow(m_btnIndicatorMACD, false);
					SafeShowWindow(m_btnIndicatorKDJ, false);
					SafeShowWindow(m_btnIndicatorWR, false);
					SafeShowWindow(m_btnIndicatorRSI, false);
				}
				else
				{
					int btnW = yAxisWidth - g_data.RDPI(4);
					int btnX = stockListWidth + g_data.RDPI(2);
					int btnAreaTop = origMacdTop + titleH;
					int btnAreaBottom = h - indexBarHeight - g_data.RDPI(2);
					int btnAreaH = max(1, btnAreaBottom - btnAreaTop);
					int btnGap = g_data.RDPI(1);
					int btnH = max(g_data.RDPI(16), (btnAreaH - btnGap * 3) / 4);
					int btn1Y = btnAreaTop;
					int btn2Y = btn1Y + btnH + btnGap;
					int btn3Y = btn2Y + btnH + btnGap;
					int btn4Y = min(btn3Y + btnH + btnGap, btnAreaBottom - btnH);
					SafeSetWindowPos(m_btnIndicatorMACD, btnX, btn1Y, btnW, btnH);
					SafeShowWindow(m_btnIndicatorMACD, true);
					SafeSetButtonStyle(m_btnIndicatorMACD, m_timelineIndicator == TimelineIndicator::MACD ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
					SafeSetWindowPos(m_btnIndicatorKDJ, btnX, btn2Y, btnW, btnH);
					SafeShowWindow(m_btnIndicatorKDJ, true);
					SafeSetButtonStyle(m_btnIndicatorKDJ, m_timelineIndicator == TimelineIndicator::KDJ ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
					SafeSetWindowPos(m_btnIndicatorWR, btnX, btn3Y, btnW, btnH);
					SafeShowWindow(m_btnIndicatorWR, true);
					SafeSetButtonStyle(m_btnIndicatorWR, m_timelineIndicator == TimelineIndicator::WR ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
					SafeSetWindowPos(m_btnIndicatorRSI, btnX, btn4Y, btnW, btnH);
					SafeShowWindow(m_btnIndicatorRSI, true);
					SafeSetButtonStyle(m_btnIndicatorRSI, m_timelineIndicator == TimelineIndicator::RSI ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
				}

				// 首次绘制时依次切换指标再切回KDJ，强制所有按钮正确渲染
				if (!m_indicatorBtnsInitialized)
				{
					m_indicatorBtnsInitialized = true;
					m_timelineIndicator = TimelineIndicator::MACD;
					m_btnIndicatorMACD.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
					m_btnIndicatorKDJ.SetButtonStyle(BS_PUSHBUTTON, TRUE);
					m_btnIndicatorMACD.Invalidate();
					m_btnIndicatorKDJ.Invalidate();
					m_btnIndicatorWR.Invalidate();
					m_btnIndicatorRSI.Invalidate();
					m_timelineIndicator = TimelineIndicator::KDJ;
					m_btnIndicatorMACD.SetButtonStyle(BS_PUSHBUTTON, TRUE);
					m_btnIndicatorKDJ.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
					m_btnIndicatorMACD.Invalidate();
					m_btnIndicatorKDJ.Invalidate();
					m_btnIndicatorWR.Invalidate();
					m_btnIndicatorRSI.Invalidate();
				}
				// 强制重绘指标按钮，避免位置变化后按钮不显示
				m_btnIndicatorMACD.Invalidate();
				m_btnIndicatorKDJ.Invalidate();
				m_btnIndicatorWR.Invalidate();
				m_btnIndicatorRSI.Invalidate();
			}

			// 右侧盘口高度：不减xAxisLabelHeight（那是左侧走势图的时间标签，右侧不需要）
			if (m_showChipPeak)
				DrawChipPeakPanel(memDC, chartWidth, w, h - headerHeight - indexBarHeight, realtimeData, chipData, timelinePoint);
			else
				DrawOrderBook(memDC, chartWidth, w, h - headerHeight - indexBarHeight, realtimeData, klineData);
		}
		else
		{
			CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&pMiddleLine);
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			memDC.TextOut((chartWidth - memDC.GetTextExtent(loading_state_txt).cx) / 2, headerHeight + g_data.RDPI(10), loading_state_txt);
		}

		// 绘制底部大盘指数状态栏
		{
			const int barY = h - indexBarHeight;
			memDC.FillSolidRect(0, barY, w, indexBarHeight, RGB(240, 240, 240));
			memDC.SetBkMode(TRANSPARENT);

			// 从配置中获取勾选了状态栏展示的股票代码
			std::vector<std::wstring> statusBarCodes = g_data.GetStatusBarStockCodes();
			if (statusBarCodes.empty())
			{
				// 没有配置时使用默认指数：上证指数、中证银行、恒生科技
				statusBarCodes = { L"sh000001", L"sz399986", L"rt_hkHSTECH" };
			}
			const int indexCount = static_cast<int>(statusBarCodes.size());
			const int colWidth = w / max(indexCount, 1);

			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			for (int i = 0; i < indexCount; i++)
			{
				auto stockData = g_data.GetStockData(statusBarCodes[i]);
				int colX = i * colWidth;
				int textX = colX + g_data.RDPI(4);

				if (stockData && stockData->info.is_ok)
				{
					const auto& info = stockData->info;
					double displayPrice = info.currentPrice > 0 ? info.currentPrice : info.prevClosePrice;
					double diff = displayPrice - info.prevClosePrice;
					double diffPercent = info.prevClosePrice != 0 ? (diff / info.prevClosePrice) * 100 : 0;
					COLORREF priceColor = diff > 0 ? COLOR_RED_UP : (diff < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);

					CString nameStr = info.GetStockListName();
					CString priceStr;
					priceStr.Format(_T("%.2f"), displayPrice);
					CString changeStr;
					if (diff >= 0)
						changeStr.Format(_T("+%.2f%%"), diffPercent);
					else
						changeStr.Format(_T("%.2f%%"), diffPercent);

					memDC.SetTextColor(COLOR_BLACK);
					memDC.TextOut(textX, barY + g_data.RDPI(2), nameStr);
					textX += memDC.GetTextExtent(nameStr).cx + g_data.RDPI(4);

					memDC.SetTextColor(priceColor);
					memDC.TextOut(textX, barY + g_data.RDPI(2), priceStr);
					textX += memDC.GetTextExtent(priceStr).cx + g_data.RDPI(4);

					memDC.TextOut(textX, barY + g_data.RDPI(2), changeStr);
				}
				else
				{
					CString nameStr = CString(statusBarCodes[i].c_str());
					memDC.SetTextColor(COLOR_GRAY_PURPLE);
					memDC.TextOut(textX, barY + g_data.RDPI(2), nameStr + _T(" --"));
				}
			}
		}
	} // end if (!m_isOverviewMode)

	if (m_isOverviewMode)
	{
		m_hScrollBar.ShowWindow(SW_HIDE);

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
			DrawIndexSection(memDC, 0, headerHeight, w, indices);
		}

		// 绘制表格（从指数区域下方开始，间距3像素在表格外部）
		int tableTop = headerHeight + indexSectionHeight + 3;
		int tableHeight = h - tableTop - statusBarHeight;
		DrawOverviewTable(memDC, 0, tableTop, w, tableHeight, m_vScrollOffset, h);
	}

	dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

	memDC.SelectObject(pOldBitmap);
}

// ========== MACD指标计算与绘制 ==========

// 为每个分时数据点计算MA5/MA10/MA20滚动均价（滑动窗口）
void CFloatingWnd::CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return;

	// 计算每个窗口的滚动均价，使用滑动窗口避免重复求和
	auto calcWindow = [&](int windowSize, int fieldOffset) {
		STOCK::Amount sumAmount = 0;
		STOCK::Volume sumVolume = 0;
		for (int i = 0; i < n; i++)
		{
			sumAmount += timelinePoint[i].amount;
			sumVolume += timelinePoint[i].volume;
			if (i >= windowSize)
			{
				sumAmount -= timelinePoint[i - windowSize].amount;
				sumVolume -= timelinePoint[i - windowSize].volume;
			}
			if (sumVolume > 0)
			{
				STOCK::Price maVal = sumAmount / sumVolume;
				switch (fieldOffset)
				{
				case 5: timelinePoint[i].ma5 = maVal; break;
				case 10: timelinePoint[i].ma10 = maVal; break;
				case 20: timelinePoint[i].ma20 = maVal; break;
				}
			}
		}
		};

	calcWindow(5, 5);
	calcWindow(10, 10);
	calcWindow(20, 20);
}

// 计算滚动均价：最近nMinutes分钟的成交额/成交量（单次查询）
STOCK::Price CFloatingWnd::CalcRollingAvgPrice(const std::vector<STOCK::TimelinePoint>& timelinePoint, int nMinutes)
{
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0 || nMinutes <= 0)
		return 0;

	int startIdx = max(0, n - nMinutes);
	STOCK::Amount sumAmount = 0;
	STOCK::Volume sumVolume = 0;
	for (int i = startIdx; i < n; i++)
	{
		sumAmount += timelinePoint[i].amount;
		sumVolume += timelinePoint[i].volume;
	}
	if (sumVolume == 0)
		return 0;
	return sumAmount / sumVolume;
}

std::vector<CFloatingWnd::MACDData> CFloatingWnd::CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	std::vector<MACDData> result;
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return result;

	result.resize(n);

	// 提取收盘价（分时价格）
	std::vector<double> closes(n);
	for (int i = 0; i < n; i++)
		closes[i] = timelinePoint[i].price;

	// 通达信初始化方式：第一根收盘价作为EMA初始值，提前生成指标
	// 随着数据量增加，数值逐步收敛到标准值

	// 计算EMA12：第一根用收盘价初始化，后续迭代
	std::vector<double> ema12(n);
	ema12[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema12[i] = closes[i] * 2.0 / 13.0 + ema12[i - 1] * 11.0 / 13.0;

	// 计算EMA26：第一根用收盘价初始化，后续迭代
	std::vector<double> ema26(n);
	ema26[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema26[i] = closes[i] * 2.0 / 27.0 + ema26[i - 1] * 25.0 / 27.0;

	// 计算DIF = EMA12 - EMA26
	std::vector<double> dif(n);
	for (int i = 0; i < n; i++)
		dif[i] = ema12[i] - ema26[i];

	// 计算DEA（DIF的9周期EMA）：第一根DIF作为初始值，后续迭代
	std::vector<double> dea(n);
	dea[0] = dif[0];
	for (int i = 1; i < n; i++)
		dea[i] = dif[i] * 2.0 / 11.0 + dea[i - 1] * 9.0 / 11.0;

	// 计算MACD柱 = (DIF - DEA) * 2，所有数据点均有效
	for (int i = 0; i < n; i++)
	{
		result[i].dif = dif[i];
		result[i].dea = dea[i];
		result[i].bar = (dif[i] - dea[i]) * 2.0;
		result[i].valid = true;
	}

	return result;
}

std::vector<CFloatingWnd::MACDData> CFloatingWnd::CalculateKLineMACD(const std::vector<STOCK::KLinePoint>& klineData)
{
	std::vector<MACDData> result;
	int n = static_cast<int>(klineData.size());
	if (n == 0)
		return result;

	result.resize(n);

	// 提取收盘价
	std::vector<double> closes(n);
	for (int i = 0; i < n; i++)
		closes[i] = klineData[i].close;

	// 计算EMA12
	std::vector<double> ema12(n);
	ema12[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema12[i] = closes[i] * 2.0 / 13.0 + ema12[i - 1] * 11.0 / 13.0;

	// 计算EMA26
	std::vector<double> ema26(n);
	ema26[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema26[i] = closes[i] * 2.0 / 27.0 + ema26[i - 1] * 25.0 / 27.0;

	// 计算DIF = EMA12 - EMA26
	std::vector<double> dif(n);
	for (int i = 0; i < n; i++)
		dif[i] = ema12[i] - ema26[i];

	// 计算DEA（DIF的9周期EMA）
	std::vector<double> dea(n);
	dea[0] = dif[0];
	for (int i = 1; i < n; i++)
		dea[i] = dif[i] * 2.0 / 11.0 + dea[i - 1] * 9.0 / 11.0;

	// 计算MACD柱 = (DIF - DEA) * 2
	for (int i = 0; i < n; i++)
	{
		result[i].dif = dif[i];
		result[i].dea = dea[i];
		result[i].bar = (dif[i] - dea[i]) * 2.0;
		result[i].valid = true;
	}

	return result;
}

std::vector<CFloatingWnd::MACDCrossSignal> CFloatingWnd::DetectMACDCross(const std::vector<MACDData>& macdData)
{
	std::vector<MACDCrossSignal> signals(macdData.size(), MACDCrossSignal::None);
	if (macdData.size() < 2)
		return signals;

	const double epsilon = 1e-6; // 粘合阈值：差值小于此值视为未交叉
	const int MIN_CROSS_GAP = 10; // 两次交叉之间最少间隔10根K线，防止0轴附近频繁缠绕
	int lastCrossIdx = -MIN_CROSS_GAP; // 上次交叉位置

	for (size_t i = 1; i < macdData.size(); i++)
	{
		if (!macdData[i].valid || !macdData[i - 1].valid)
			continue;

		double difPrev = macdData[i - 1].dif;
		double deaPrev = macdData[i - 1].dea;
		double difNow = macdData[i].dif;
		double deaNow = macdData[i].dea;

		// 粘合状态：当前DIF与DEA差值过小，不触发新叉
		if (std::abs(difNow - deaNow) < epsilon)
			continue;

		// 金叉：上一时刻DIF < DEA，当前时刻DIF > DEA
		if (difPrev < deaPrev && difNow > deaNow)
		{
			if (static_cast<int>(i) - lastCrossIdx >= MIN_CROSS_GAP)
			{
				signals[i] = MACDCrossSignal::GoldenCross;
				lastCrossIdx = static_cast<int>(i);
			}
		}
		// 死叉：上一时刻DIF > DEA，当前时刻DIF < DEA
		else if (difPrev > deaPrev && difNow < deaNow)
		{
			if (static_cast<int>(i) - lastCrossIdx >= MIN_CROSS_GAP)
			{
				signals[i] = MACDCrossSignal::DeathCross;
				lastCrossIdx = static_cast<int>(i);
			}
		}
	}

	return signals;
}

CFloatingWnd::MACDCrossSignal CFloatingWnd::GetLatestMACDCross(const std::vector<MACDData>& macdData)
{
	auto signals = DetectMACDCross(macdData);
	for (int i = static_cast<int>(signals.size()) - 1; i >= 0; i--)
	{
		if (signals[i] != MACDCrossSignal::None)
			return signals[i];
	}
	return MACDCrossSignal::None;
}

// ========== T+0日内买卖点实时判定 ==========

CSignalAnalyzer::T0Signal CFloatingWnd::DetectBuySignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData)
{
	CSignalAnalyzer::T0Signal signal;
	signal.valid = false;
	signal.isBuy = true;
	signal.strength = 0;
	signal.price = 0;

	int n = static_cast<int>(timelinePoint.size());
	int m = static_cast<int>(macdData.size());
	if (n < 10 || m < 10)
		return signal;

	int last = n - 1;
	signal.price = timelinePoint[last].price;
	signal.time = CString(timelinePoint[last].time.c_str());

	// ========== 量能基准：前三根成交量均值 ==========
	long long volAvg3 = 0;
	{
		int volCount = min(3, last);
		for (int i = last - volCount; i < last; i++)
			volAvg3 += timelinePoint[i].volume;
		if (volCount > 0) volAvg3 /= volCount;
	}
	bool isShrinking = (volAvg3 > 0 && timelinePoint[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
	bool isExpanding = (volAvg3 > 0 && timelinePoint[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

	int strength = 0;
	CString reasons;

	// ========== 避坑规则：放量大跌不抄底 ==========
	{
		int lookback = min(5, n - 1);
		bool heavyDecline = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price >= timelinePoint[i - 1].price)
			{
				heavyDecline = false;
				break;
			}
		}
		// 持续下跌且量能递增 = 放量杀跌
		if (heavyDecline)
		{
			bool volIncreasing = true;
			for (int i = last - lookback + 2; i <= last; i++)
			{
				if (timelinePoint[i].volume < timelinePoint[i - 1].volume)
				{
					volIncreasing = false;
					break;
				}
			}
			if (volIncreasing)
				return signal; // 放量杀跌，坚决不抄底
		}
	}

	// ========== 避坑规则：无量阴跌不急于接盘 ==========
	{
		int lookback = min(10, n - 1);
		bool steadyDecline = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price >= timelinePoint[i - 1].price)
			{
				steadyDecline = false;
				break;
			}
		}
		bool lowVolume = isShrinking;
		bool macdStillDown = macdData[last].valid && macdData[last].dif < 0 && macdData[last].bar < 0;
		if (steadyDecline && lowVolume && macdStillDown)
			return signal; // 无量阴跌，MACD仍下行
	}

	// ========== 1. 量柱信号：缩量下跌 + 首根红量柱（需量能确认） ==========
	{
		int lookback = min(10, n - 1);
		int greenCount = 0;
		bool shrinking = true;
		long long prevGreenVol = 0;

		for (int i = last - lookback + 1; i <= last; i++)
		{
			bool isGreen = (timelinePoint[i].price < timelinePoint[i - 1].price);
			if (isGreen)
			{
				greenCount++;
				if (prevGreenVol > 0 && timelinePoint[i].volume > prevGreenVol)
					shrinking = false;
				prevGreenVol = timelinePoint[i].volume;
			}
		}

		// 最新一根是否为红柱（上涨），且之前有连续下跌
		bool firstRed = (timelinePoint[last].price > timelinePoint[last - 1].price) && greenCount >= 3;

		if (shrinking && greenCount >= 3 && isShrinking)
		{
			strength++;
			reasons += _T("缩量下跌抛压耗尽 ");
		}
		// 首根红量柱需要放量确认，否则可能是假反弹
		if (firstRed && isExpanding)
		{
			strength++;
			reasons += _T("放量红柱资金承接 ");
		}
	}

	// ========== 2. MACD信号 ==========
	if (macdData[last].valid)
	{
		// 绿柱缩短
		if (macdData[last].bar < 0)
		{
			int shrkCount = 0;
			for (int i = last; i > max(0, last - 5); i--)
			{
				if (macdData[i].valid && macdData[i].bar < 0 &&
					std::abs(macdData[i].bar) <= std::abs(macdData[i - 1].bar))
					shrkCount++;
			}
			if (shrkCount >= 3)
			{
				strength++;
				reasons += _T("MACD绿柱缩短 ");
			}
		}

		// 底背离检测：价格创新低，MACD未创新低
		int lookback = min(30, n - 1);
		int low1Idx = -1, low2Idx = -1;
		for (int i = last - lookback + 1; i < last; i++)
		{
			if (i > 0 && i < n - 1 &&
				timelinePoint[i].price < timelinePoint[i - 1].price &&
				timelinePoint[i].price < timelinePoint[i + 1].price)
			{
				if (low1Idx < 0)
					low1Idx = i;
				else if (i > low1Idx + 3 && low2Idx < 0)
					low2Idx = i;
			}
		}
		if (low1Idx >= 0 && low2Idx >= 0 &&
			timelinePoint[low2Idx].price < timelinePoint[low1Idx].price &&
			macdData[low2Idx].valid && macdData[low1Idx].valid &&
			macdData[low2Idx].dif > macdData[low1Idx].dif)
		{
			strength += 2;
			reasons += _T("MACD底背离 ");
		}
	}

	// ========== 3. 股价支撑（需量能确认） ==========
	{
		// 回踩均价支撑：需要缩量回踩才有效
		double avgPrice = timelinePoint[last].averagePrice;
		double price = timelinePoint[last].price;
		if (avgPrice > 0 && std::abs(price - avgPrice) / avgPrice < 0.003 && isShrinking)
		{
			strength++;
			reasons += _T("缩量回踩均价支撑 ");
		}
		// 分时底部企稳：需要放量止跌才可靠
		if (last >= 2 &&
			timelinePoint[last].price >= timelinePoint[last - 1].price &&
			timelinePoint[last - 1].price < timelinePoint[last - 2].price &&
			isExpanding)
		{
			strength++;
			reasons += _T("放量底部企稳 ");
		}
	}

	signal.strength = min(3, strength);
	signal.reason = reasons;
	signal.valid = (strength >= 2);
	return signal;
}

CSignalAnalyzer::T0Signal CFloatingWnd::DetectSellSignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData)
{
	CSignalAnalyzer::T0Signal signal;
	signal.valid = false;
	signal.isBuy = false;
	signal.strength = 0;
	signal.price = 0;

	int n = static_cast<int>(timelinePoint.size());
	int m = static_cast<int>(macdData.size());
	if (n < 10 || m < 10)
		return signal;

	int last = n - 1;
	signal.price = timelinePoint[last].price;
	signal.time = CString(timelinePoint[last].time.c_str());

	// ========== 量能基准：前三根成交量均值 ==========
	long long volAvg3 = 0;
	{
		int volCount = min(3, last);
		for (int i = last - volCount; i < last; i++)
			volAvg3 += timelinePoint[i].volume;
		if (volCount > 0) volAvg3 /= volCount;
	}
	bool isShrinking = (volAvg3 > 0 && timelinePoint[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
	bool isExpanding = (volAvg3 > 0 && timelinePoint[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

	int strength = 0;
	CString reasons;

	// ========== 避坑规则：放量大涨不卖出 ==========
	{
		int lookback = min(5, n - 1);
		bool strongRise = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price <= timelinePoint[i - 1].price)
			{
				strongRise = false;
				break;
			}
		}
		if (strongRise)
		{
			bool volIncreasing = true;
			for (int i = last - lookback + 2; i <= last; i++)
			{
				if (timelinePoint[i].volume < timelinePoint[i - 1].volume)
				{
					volIncreasing = false;
					break;
				}
			}
			if (volIncreasing)
				return signal; // 放量大涨，不卖出
		}
	}

	// ========== 1. 量柱信号：放量冲高 + 量能萎缩（需量能确认） ==========
	{
		// 检查近期是否有放量冲高（使用前三根均值判定放量）
		int lookback = min(10, n - 1);
		bool hadSpike = false;
		int spikeIdx = -1;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			// 计算该点的前三根均值
			long long localAvg3 = 0;
			int vc = min(3, i);
			for (int j = i - vc; j < i; j++)
				localAvg3 += timelinePoint[j].volume;
			if (vc > 0) localAvg3 /= vc;
			if (localAvg3 > 0 && timelinePoint[i].volume > localAvg3 * 3 / 2)
			{
				hadSpike = true;
				spikeIdx = i;
				break;
			}
		}
		// 放量后量能萎缩，且当前价格未明显回落（仍在高位）
		bool priceStillHigh = (hadSpike && spikeIdx >= 0 &&
			timelinePoint[last].price >= timelinePoint[spikeIdx].price * 0.995);
		bool shrinkingNow = false;
		if (hadSpike && last > spikeIdx + 1 &&
			timelinePoint[last].volume < timelinePoint[last - 1].volume &&
			isShrinking && priceStillHigh)
			shrinkingNow = true;

		if (hadSpike && shrinkingNow)
		{
			strength++;
			reasons += _T("放量冲高后缩量回落 ");
		}
	}

	// ========== 2. MACD信号 ==========
	if (macdData[last].valid)
	{
		// 红柱缩短
		if (macdData[last].bar > 0)
		{
			int shrkCount = 0;
			for (int i = last; i > max(0, last - 5); i--)
			{
				if (macdData[i].valid && macdData[i].bar > 0 &&
					macdData[i].bar <= macdData[i - 1].bar)
					shrkCount++;
			}
			if (shrkCount >= 3)
			{
				strength++;
				reasons += _T("MACD红柱缩短 ");
			}
		}

		// 死叉检测
		auto crossSignals = DetectMACDCross(macdData);
		if (last < static_cast<int>(crossSignals.size()) && crossSignals[last] == MACDCrossSignal::DeathCross)
		{
			strength++;
			reasons += _T("MACD死叉 ");
		}

		// 顶背离检测：价格创新高，MACD未创新高
		int lookback = min(30, n - 1);
		int high1Idx = -1, high2Idx = -1;
		for (int i = last - lookback + 1; i < last; i++)
		{
			if (i > 0 && i < n - 1 &&
				timelinePoint[i].price > timelinePoint[i - 1].price &&
				timelinePoint[i].price > timelinePoint[i + 1].price)
			{
				if (high1Idx < 0)
					high1Idx = i;
				else if (i > high1Idx + 3 && high2Idx < 0)
					high2Idx = i;
			}
		}
		if (high1Idx >= 0 && high2Idx >= 0 &&
			timelinePoint[high2Idx].price > timelinePoint[high1Idx].price &&
			macdData[high2Idx].valid && macdData[high1Idx].valid &&
			macdData[high2Idx].dif < macdData[high1Idx].dif)
		{
			strength += 2;
			reasons += _T("MACD顶背离 ");
		}
	}

	// ========== 3. 股价压力（需量能确认） ==========
	{
		// 遇均价压力：需要放量冲高遇阻才有效
		double avgPrice = timelinePoint[last].averagePrice;
		double price = timelinePoint[last].price;
		if (avgPrice > 0 && price > avgPrice && std::abs(price - avgPrice) / avgPrice < 0.005 && isExpanding)
		{
			strength++;
			reasons += _T("放量遇均价压力 ");
		}
		// 冲高滞涨：放量冲高后回落
		if (last >= 2 &&
			timelinePoint[last].price <= timelinePoint[last - 1].price &&
			timelinePoint[last - 1].price > timelinePoint[last - 2].price &&
			isShrinking)
		{
			strength++;
			reasons += _T("冲高缩量滞涨 ");
		}
	}

	signal.strength = min(3, strength);
	signal.reason = reasons;
	signal.valid = (strength >= 2);
	return signal;
}

void CFloatingWnd::DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData, int startIndex /* = 0 */, int visibleCount /* = -1 */)
{
	if (timelinePoint.empty() || macdData.empty())
		return;

	// 计算可见范围：visibleCount <= 0 表示显示全部
	int total = static_cast<int>(timelinePoint.size());
	int endIdx = total;
	if (visibleCount > 0)
	{
		endIdx = (std::min)(total, startIndex + visibleCount);
		startIndex = (std::max)(0, (std::min)(startIndex, total - 1));
		if (endIdx <= startIndex) endIdx = startIndex + 1;
	}
	else
	{
		startIndex = 0;
	}

	// 找到最大绝对值用于缩放
	double maxAbs = 0;
	for (const auto& m : macdData)
	{
		if (m.valid)
		{
			maxAbs = (std::max)(maxAbs, std::abs(m.dif));
			maxAbs = (std::max)(maxAbs, std::abs(m.dea));
			maxAbs = (std::max)(maxAbs, std::abs(m.bar));
		}
	}
	if (maxAbs == 0)
		return;

	// 零轴位置（中间）
	int zeroY = y + height / 2;
	float unitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(maxAbs);

	// 绘制零轴
	CPen zeroPen(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
	CPen* pOldPen = memDC.SelectObject(&zeroPen);
	memDC.MoveTo(x, zeroY);
	memDC.LineTo(x + width, zeroY);

	// 绘制MACD柱
	// 动态计算柱子宽度：间距固定1像素，柱子宽度随缩放增大
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int fixedGap = 1;
	int slotWidth = totalPts > 0 ? width / totalPts : 1;
	int barWidth = max(2, slotWidth - fixedGap);
	int halfSlot = slotWidth / 2;
	for (int i = startIndex; i < endIdx && i < static_cast<int>(macdData.size()); i++)
	{
		if (!macdData[i].valid)
			continue;

		int barX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + halfSlot - barWidth / 2;

		double barVal = macdData[i].bar;
		int barHeight = static_cast<int>(std::abs(barVal) * unitY);
		barHeight = max(1, barHeight);

		COLORREF color = (barVal >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CBrush brush(color);
		int barY = (barVal >= 0) ? zeroY - barHeight : zeroY;
		memDC.FillRect(CRect(barX, barY, barX + barWidth, zeroY + (barVal >= 0 ? 0 : barHeight)), &brush);
	}

	// 绘制DIF线
	CPen difPen(PS_SOLID, 1, COLOR_DARK_GRAY_BORDER);
	memDC.SelectObject(&difPen);
	bool difFirst = true;
	for (int i = startIndex; i < endIdx && i < static_cast<int>(macdData.size()); i++)
	{
		if (!macdData[i].valid)
			continue;

		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + halfSlot;

		int pointY = zeroY - static_cast<int>(macdData[i].dif * unitY);
		if (difFirst)
		{
			memDC.MoveTo(pointX, pointY);
			difFirst = false;
		}
		else
		{
			memDC.LineTo(pointX, pointY);
		}
	}

	// 绘制DEA线
	CPen deaPen(PS_SOLID, 1, COLOR_GOLDEN);
	memDC.SelectObject(&deaPen);
	bool deaFirst = true;
	for (int i = startIndex; i < endIdx && i < static_cast<int>(macdData.size()); i++)
	{
		if (!macdData[i].valid)
			continue;

		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + halfSlot;

		int pointY = zeroY - static_cast<int>(macdData[i].dea * unitY);
		if (deaFirst)
		{
			memDC.MoveTo(pointX, pointY);
			deaFirst = false;
		}
		else
		{
			memDC.LineTo(pointX, pointY);
		}
	}

	memDC.SelectObject(pOldPen);

	// 绘制金叉死叉标记
	auto crossSignals = DetectMACDCross(macdData);
	const int dotRadius = g_data.RDPI(3);
	const int labelOffset = 0;
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	for (int i = startIndex; i < endIdx && i < static_cast<int>(crossSignals.size()); i++)
	{
		if (crossSignals[i] == MACDCrossSignal::None)
			continue;

		// 计算交叉点X坐标
		int markX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + halfSlot;

		// 交叉点Y坐标（DIF≈DEA处）
		int markY = zeroY - static_cast<int>(macdData[i].dif * unitY);

		bool isGolden = (crossSignals[i] == MACDCrossSignal::GoldenCross);
		COLORREF dotColor = isGolden ? COLOR_GREEN_DOWN : COLOR_RED_UP;
		COLORREF labelColor = isGolden ? COLOR_GOLDEN : COLOR_GRAY_TEXT;
		CString label = isGolden ? _T("g") : _T("d");

		// 交叉点圆点
		CBrush dotBrush(dotColor);
		CPen dotPen(PS_SOLID, 1, dotColor);
		CBrush* pOldBrush = memDC.SelectObject(&dotBrush);
		CPen* pOldDotPen = memDC.SelectObject(&dotPen);
		memDC.Ellipse(markX - dotRadius, markY - dotRadius, markX + dotRadius, markY + dotRadius);

		// 文字位置：金叉"g"显示在圆点下方，死叉"d"显示在圆点上方
		memDC.SetTextColor(labelColor);
		CSize labelSize = memDC.GetTextExtent(label);
		int labelY;
		if (isGolden)
			labelY = markY + dotRadius + labelOffset;
		else
			labelY = markY - dotRadius - labelOffset - labelSize.cy;
		memDC.TextOut(markX - labelSize.cx / 2, labelY, label);

		memDC.SelectObject(pOldBrush);
		memDC.SelectObject(pOldDotPen);
	}
	memDC.SetBkMode(oldBkMode);
}

void CFloatingWnd::DrawMACDChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData, const std::vector<MACDData>& macdData, int startIndex /* = 0 */, int visibleCount /* = -1 */)
{
	if (klineData.empty() || macdData.empty())
		return;

	// 计算可见范围
	int total = static_cast<int>(macdData.size());
	int endIdx = total;
	if (visibleCount > 0)
	{
		endIdx = (std::min)(total, startIndex + visibleCount);
		startIndex = (std::max)(0, (std::min)(startIndex, total - 1));
		if (endIdx <= startIndex) endIdx = startIndex + 1;
	}
	else
	{
		startIndex = 0;
	}

	// 找到最大绝对值用于缩放
	double maxAbs = 0;
	for (const auto& m : macdData)
	{
		if (m.valid)
		{
			maxAbs = (std::max)(maxAbs, std::abs(m.dif));
			maxAbs = (std::max)(maxAbs, std::abs(m.dea));
			maxAbs = (std::max)(maxAbs, std::abs(m.bar));
		}
	}
	if (maxAbs == 0)
		return;

	// 零轴位置（中间）
	int zeroY = y + height / 2;
	float unitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(maxAbs);

	// 绘制零轴
	CPen zeroPen(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
	CPen* pOldPen = memDC.SelectObject(&zeroPen);
	memDC.MoveTo(x, zeroY);
	memDC.LineTo(x + width, zeroY);

	// 使用K线数据的displayCount计算柱宽
	int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
	int maxVisibleKlines = min(displayCount, width / 3);
	int finalStartIndex = max(0, static_cast<int>(klineData.size()) - maxVisibleKlines - m_scrollOffset);
	finalStartIndex = max(0, min(finalStartIndex, static_cast<int>(klineData.size()) - maxVisibleKlines));

	int totalVisible = static_cast<int>(klineData.size()) - finalStartIndex;
	if (totalVisible <= 0) { memDC.SelectObject(pOldPen); return; }

	int slotWidth = width / totalVisible;
	int barWidth = max(2, slotWidth - 1);
	int halfSlot = slotWidth / 2;

	// 调整startIndex和endIdx到可见范围
	int drawStart = max(startIndex, finalStartIndex);
	int drawEnd = min(endIdx, static_cast<int>(macdData.size()));

	// 绘制MACD柱
	for (int i = drawStart; i < drawEnd; i++)
	{
		if (!macdData[i].valid)
			continue;

		int barX = x + (i - finalStartIndex) * slotWidth + halfSlot - barWidth / 2;
		double barVal = macdData[i].bar;
		int barHeight = static_cast<int>(std::abs(barVal) * unitY);
		barHeight = max(1, barHeight);

		COLORREF color = (barVal >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CBrush brush(color);
		int barY = (barVal >= 0) ? zeroY - barHeight : zeroY;
		memDC.FillRect(CRect(barX, barY, barX + barWidth, zeroY + (barVal >= 0 ? 0 : barHeight)), &brush);
	}

	// 绘制DIF线
	CPen difPen(PS_SOLID, 1, COLOR_DARK_GRAY_BORDER);
	memDC.SelectObject(&difPen);
	bool difFirst = true;
	for (int i = drawStart; i < drawEnd; i++)
	{
		if (!macdData[i].valid)
			continue;

		int pointX = x + (i - finalStartIndex) * slotWidth + slotWidth / 2;
		int pointY = zeroY - static_cast<int>(macdData[i].dif * unitY);
		if (difFirst) { memDC.MoveTo(pointX, pointY); difFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	// 绘制DEA线
	CPen deaPen(PS_SOLID, 1, COLOR_GOLDEN);
	memDC.SelectObject(&deaPen);
	bool deaFirst = true;
	for (int i = drawStart; i < drawEnd; i++)
	{
		if (!macdData[i].valid)
			continue;

		int pointX = x + (i - finalStartIndex) * slotWidth + slotWidth / 2;
		int pointY = zeroY - static_cast<int>(macdData[i].dea * unitY);
		if (deaFirst) { memDC.MoveTo(pointX, pointY); deaFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
}

void CFloatingWnd::DrawTimelineMACDSection(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	auto macdData = CalculateTimelineMACD(timelinePoint);

	DrawMACDChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, macdData);

	// 绘制网格线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	int macdY = ctx.macdChartTop;
	memDC.MoveTo(0, macdY);
	memDC.LineTo(ctx.chartWidth, macdY);
	memDC.MoveTo(0, macdY + ctx.macdChartHeight);
	memDC.LineTo(ctx.chartWidth, macdY + ctx.macdChartHeight);

	// 时间竖线：4等分可见数据范围
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int numVLines = 4;
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, macdY);
			memDC.LineTo(xPos, macdY + ctx.macdChartHeight);
		}
	}
	memDC.SelectObject(pOldPen);

	// 绘制时间标签（X轴）在MACD图下方：5等分位置对应的时间
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = ctx.chartWidth * i / numVLines;
			CString timeLabel(timelinePoint[idx].time.c_str());
			if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.TextOut(labelX, ctx.macdChartTop + ctx.macdChartHeight + g_data.RDPI(2), timeLabel);
		}
	}
}

void CFloatingWnd::DrawTimelineKDJSection(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	// 使用完整分时数据计算KDJ，避免缩放/拖动时前N-1个点无有效RSV
	const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
	auto kdjData = CalculateTimelineKDJ(fullData);

	DrawTimelineKDJChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, kdjData, ctx.startIndex);

	// 绘制网格线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	int macdY = ctx.macdChartTop;
	memDC.MoveTo(0, macdY);
	memDC.LineTo(ctx.chartWidth, macdY);
	memDC.MoveTo(0, macdY + ctx.macdChartHeight);
	memDC.LineTo(ctx.chartWidth, macdY + ctx.macdChartHeight);

	// 时间竖线：4等分可见数据范围
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int numVLines = 4;
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, macdY);
			memDC.LineTo(xPos, macdY + ctx.macdChartHeight);
		}
	}
	memDC.SelectObject(pOldPen);

	// 绘制时间标签（X轴）在KDJ图下方：5等分位置对应的时间
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = ctx.chartWidth * i / numVLines;
			CString timeLabel(timelinePoint[idx].time.c_str());
			if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.TextOut(labelX, ctx.macdChartTop + ctx.macdChartHeight + g_data.RDPI(2), timeLabel);
		}
	}
}

void CFloatingWnd::DrawTimelineKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<KDJData>& kdjData, int startIndex /* = 0 */)
{
	if (timelinePoint.empty() || kdjData.empty())
		return;

	// KDJ 指标范围通常 [0, 100]，J 可能超出范围
	// 只对可见范围内的数据计算范围
	double minVal = 0;
	double maxVal = 100;
	int drawStart = startIndex;
	int drawEnd = startIndex + static_cast<int>(timelinePoint.size());
	if (drawEnd > static_cast<int>(kdjData.size())) drawEnd = static_cast<int>(kdjData.size());
	for (int i = drawStart; i < drawEnd; i++)
	{
		if (!kdjData[i].valid) continue;
		minVal = (std::min)(minVal, kdjData[i].j);
		maxVal = (std::max)(maxVal, kdjData[i].j);
	}
	if (minVal > 0) minVal = 0;
	if (maxVal < 100) maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	// 绘制20、50、80参考虚线
	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&refPen);
	double refValues[] = { 20.0, 50.0, 80.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int totalPts = static_cast<int>(timelinePoint.size());

	// 绘制K线（快线，红色）
	CPen kPen(PS_SOLID, 1, COLOR_RED_UP);
	memDC.SelectObject(&kPen);
	bool kFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(kdjData[idx].k);
		if (kFirst) { memDC.MoveTo(pointX, pointY); kFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	// 绘制D线（慢线，蓝色）
	CPen dPen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&dPen);
	bool dFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(kdjData[idx].d);
		if (dFirst) { memDC.MoveTo(pointX, pointY); dFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	// 绘制J线（深绿 #008822，波动最大调粗）
	CPen jPen(PS_SOLID, 2, RGB(0, 136, 34));
	memDC.SelectObject(&jPen);
	bool jFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(kdjData[idx].j);
		if (jFirst) { memDC.MoveTo(pointX, pointY); jFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
}

// ========== W&R威廉指标 ==========

std::vector<CFloatingWnd::WRData> CFloatingWnd::CalculateTimelineWR(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<WRData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.empty() || period1 <= 0 || period2 <= 0)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		WRData wr;
		wr.wr1 = 0;
		wr.wr2 = 0;
		wr.valid = false;

		// 计算WR1（短期，如N=6）
		if (i >= period1 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period1 + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				wr.wr1 = (static_cast<double>(highest) - timelinePoint[i].price) / (highest - lowest) * 100.0;
				if (wr.wr1 < 0) wr.wr1 = 0;
				if (wr.wr1 > 100) wr.wr1 = 100;
			}
			else if (hasValid && highest == lowest && timelinePoint[i].price > 0)
			{
				wr.wr1 = 0;  // 最高=最低时WR=0
			}
		}

		// 计算WR2（长期，如N=14）
		if (i >= period2 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period2 + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				wr.wr2 = (static_cast<double>(highest) - timelinePoint[i].price) / (highest - lowest) * 100.0;
				if (wr.wr2 < 0) wr.wr2 = 0;
				if (wr.wr2 > 100) wr.wr2 = 100;
			}
			else if (hasValid && highest == lowest && timelinePoint[i].price > 0)
			{
				wr.wr2 = 0;
			}
		}

		wr.valid = (i >= maxPeriod - 1);
		result.push_back(wr);
	}

	return result;
}

std::vector<CFloatingWnd::WRData> CFloatingWnd::CalculateKLineWR(const std::vector<STOCK::KLinePoint>& klineData, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<WRData> result;
	result.reserve(klineData.size());
	if (klineData.empty() || period1 <= 0 || period2 <= 0)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		WRData wr;
		wr.wr1 = 0;
		wr.wr2 = 0;
		wr.valid = false;

		// 计算WR1（短期，如N=6）—— 使用K线的high/low
		if (i >= period1 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period1 + 1; j <= i; j++)
			{
				if (klineData[j].high > 0 && klineData[j].low > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				wr.wr1 = (static_cast<double>(highest) - klineData[i].close) / (highest - lowest) * 100.0;
				if (wr.wr1 < 0) wr.wr1 = 0;
				if (wr.wr1 > 100) wr.wr1 = 100;
			}
			else if (hasValid && highest == lowest && klineData[i].close > 0)
			{
				wr.wr1 = 0;
			}
		}

		// 计算WR2（长期，如N=14）—— 使用K线的high/low
		if (i >= period2 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period2 + 1; j <= i; j++)
			{
				if (klineData[j].high > 0 && klineData[j].low > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				wr.wr2 = (static_cast<double>(highest) - klineData[i].close) / (highest - lowest) * 100.0;
				if (wr.wr2 < 0) wr.wr2 = 0;
				if (wr.wr2 > 100) wr.wr2 = 100;
			}
			else if (hasValid && highest == lowest && klineData[i].close > 0)
			{
				wr.wr2 = 0;
			}
		}

		wr.valid = (i >= maxPeriod - 1);
		result.push_back(wr);
	}

	return result;
}

void CFloatingWnd::DrawTimelineWRChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<WRData>& wrData, int startIndex /* = 0 */)
{
	if (timelinePoint.empty() || wrData.empty())
		return;

	// W&R 指标范围 [0, 100]
	double minVal = 0;
	double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	// 绘制20、50、80参考虚线
	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&refPen);
	double refValues[] = { 20.0, 50.0, 80.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int totalPts = static_cast<int>(timelinePoint.size());

	// 绘制WR1线（短期，蓝色）
	CPen wr1Pen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&wr1Pen);
	bool wr1First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(wrData.size()); i++)
	{
		int idx = startIndex + i;
		if (!wrData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(wrData[idx].wr1);
		if (wr1First) { memDC.MoveTo(pointX, pointY); wr1First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	// 绘制WR2线（长期，暗红色）
	CPen wr2Pen(PS_SOLID, 1, RGB(204, 34, 34));
	memDC.SelectObject(&wr2Pen);
	bool wr2First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(wrData.size()); i++)
	{
		int idx = startIndex + i;
		if (!wrData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(wrData[idx].wr2);
		if (wr2First) { memDC.MoveTo(pointX, pointY); wr2First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
}

void CFloatingWnd::DrawTimelineWRSection(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	// 5分钟/30分钟K线模式使用klineData计算（有high/low），其他模式使用timelinePoint
	std::vector<WRData> wrData;
	if ((m_isMin5KLineMode || m_isMin30KLineMode) && ctx.klineData)
	{
		wrData = CalculateKLineWR(*ctx.klineData);
	}
	else if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
	{
		// 日K线模式使用klineData
		if (ctx.klineData)
			wrData = CalculateKLineWR(*ctx.klineData);
	}
	else
	{
		// 分时模式使用完整分时数据计算，避免缩放/拖动时前N-1个点无效
		const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
		wrData = CalculateTimelineWR(fullData);
	}

	DrawTimelineWRChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, wrData, ctx.startIndex);

	// 绘制网格线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	int macdY = ctx.macdChartTop;
	memDC.MoveTo(0, macdY);
	memDC.LineTo(ctx.chartWidth, macdY);
	memDC.MoveTo(0, macdY + ctx.macdChartHeight);
	memDC.LineTo(ctx.chartWidth, macdY + ctx.macdChartHeight);

	// 时间竖线：4等分可见数据范围
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int numVLines = 4;
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, macdY);
			memDC.LineTo(xPos, macdY + ctx.macdChartHeight);
		}
	}
	memDC.SelectObject(pOldPen);

	// 绘制时间标签（X轴）在WR图下方：4等分位置对应的时间
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = ctx.chartWidth * i / numVLines;
			CString timeLabel(timelinePoint[idx].time.c_str());
			if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.TextOut(labelX, ctx.macdChartTop + ctx.macdChartHeight + g_data.RDPI(2), timeLabel);
		}
	}
}

// ========== RSI相对强弱指标 ==========

std::vector<CFloatingWnd::RSIData> CFloatingWnd::CalculateTimelineRSI(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<RSIData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.size() < 2 || period1 <= 1 || period2 <= 1)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	// 辅助lambda：计算指定周期的RSI序列
	auto calcRSI = [&](int period) -> std::vector<double> {
		std::vector<double> rsiValues(timelinePoint.size(), 0);
		// 初始AU/AD：前period根K线的平均涨跌幅
		double au = 0, ad = 0;
		int count = 0;
		for (int i = 1; i <= period && i < static_cast<int>(timelinePoint.size()); i++)
		{
			double delta = timelinePoint[i].price - timelinePoint[i - 1].price;
			if (delta > 0)
				au += delta;
			else
				ad += (-delta);
			count++;
		}
		if (count > 0)
		{
			au /= count;
			ad /= count;
		}

		// 第period根的RSI
		if (period < static_cast<int>(timelinePoint.size()))
		{
			if (au == 0 && ad == 0)
				rsiValues[period] = 50.0;
			else
				rsiValues[period] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}

		// 从第period+1根开始，使用SMA平滑
		for (int i = period + 1; i < static_cast<int>(timelinePoint.size()); i++)
		{
			double delta = timelinePoint[i].price - timelinePoint[i - 1].price;
			double up = (delta > 0) ? delta : 0.0;
			double dn = (delta < 0) ? (-delta) : 0.0;
			au = (au * (period - 1) + up) / period;
			ad = (ad * (period - 1) + dn) / period;
			if (au == 0 && ad == 0)
				rsiValues[i] = 50.0;
			else
				rsiValues[i] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}
		return rsiValues;
		};

	auto rsi1Values = calcRSI(period1);
	auto rsi2Values = calcRSI(period2);

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}

	return result;
}

std::vector<CFloatingWnd::RSIData> CFloatingWnd::CalculateKLineRSI(const std::vector<STOCK::KLinePoint>& klineData, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<RSIData> result;
	result.reserve(klineData.size());
	if (klineData.size() < 2 || period1 <= 1 || period2 <= 1)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	// 辅助lambda：计算指定周期的RSI序列
	auto calcRSI = [&](int period) -> std::vector<double> {
		std::vector<double> rsiValues(klineData.size(), 0);
		double au = 0, ad = 0;
		int count = 0;
		for (int i = 1; i <= period && i < static_cast<int>(klineData.size()); i++)
		{
			double delta = klineData[i].close - klineData[i - 1].close;
			if (delta > 0)
				au += delta;
			else
				ad += (-delta);
			count++;
		}
		if (count > 0)
		{
			au /= count;
			ad /= count;
		}

		if (period < static_cast<int>(klineData.size()))
		{
			if (au == 0 && ad == 0)
				rsiValues[period] = 50.0;
			else
				rsiValues[period] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}

		for (int i = period + 1; i < static_cast<int>(klineData.size()); i++)
		{
			double delta = klineData[i].close - klineData[i - 1].close;
			double up = (delta > 0) ? delta : 0.0;
			double dn = (delta < 0) ? (-delta) : 0.0;
			au = (au * (period - 1) + up) / period;
			ad = (ad * (period - 1) + dn) / period;
			if (au == 0 && ad == 0)
				rsiValues[i] = 50.0;
			else
				rsiValues[i] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}
		return rsiValues;
		};

	auto rsi1Values = calcRSI(period1);
	auto rsi2Values = calcRSI(period2);

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}

	return result;
}

void CFloatingWnd::DrawTimelineRSIChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<RSIData>& rsiData, int startIndex /* = 0 */)
{
	if (timelinePoint.empty() || rsiData.empty())
		return;

	// RSI 指标范围 [0, 100]
	double minVal = 0;
	double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	// 绘制30、50、70参考虚线
	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&refPen);
	double refValues[] = { 30.0, 50.0, 70.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int totalPts = static_cast<int>(timelinePoint.size());

	// 绘制RSI1线（短期，蓝色）
	CPen rsi1Pen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&rsi1Pen);
	bool rsi1First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(rsiData.size()); i++)
	{
		int idx = startIndex + i;
		if (!rsiData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(rsiData[idx].rsi1);
		if (rsi1First) { memDC.MoveTo(pointX, pointY); rsi1First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	// 绘制RSI2线（长期，暗红色）
	CPen rsi2Pen(PS_SOLID, 1, RGB(204, 34, 34));
	memDC.SelectObject(&rsi2Pen);
	bool rsi2First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(rsiData.size()); i++)
	{
		int idx = startIndex + i;
		if (!rsiData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + static_cast<int>(width / static_cast<float>(totalPts) / 2);
		int pointY = valueToY(rsiData[idx].rsi2);
		if (rsi2First) { memDC.MoveTo(pointX, pointY); rsi2First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
}

void CFloatingWnd::DrawTimelineRSISection(CDC& memDC, const TimelineDrawContext& ctx)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	// 5分钟/30分钟K线模式使用klineData计算，其他模式使用timelinePoint
	std::vector<RSIData> rsiData;
	if ((m_isMin5KLineMode || m_isMin30KLineMode) && ctx.klineData)
	{
		rsiData = CalculateKLineRSI(*ctx.klineData);
	}
	else if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
	{
		if (ctx.klineData)
			rsiData = CalculateKLineRSI(*ctx.klineData);
	}
	else
	{
		const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
		rsiData = CalculateTimelineRSI(fullData);
	}

	DrawTimelineRSIChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, rsiData, ctx.startIndex);

	// 绘制网格线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	int macdY = ctx.macdChartTop;
	memDC.MoveTo(0, macdY);
	memDC.LineTo(ctx.chartWidth, macdY);
	memDC.MoveTo(0, macdY + ctx.macdChartHeight);
	memDC.LineTo(ctx.chartWidth, macdY + ctx.macdChartHeight);

	// 时间竖线：4等分可见数据范围
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int numVLines = 4;
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int xPos = ctx.chartWidth * i / numVLines;
			memDC.MoveTo(xPos, macdY);
			memDC.LineTo(xPos, macdY + ctx.macdChartHeight);
		}
	}
	memDC.SelectObject(pOldPen);

	// 绘制时间标签（X轴）在RSI图下方
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (totalPts > 0)
	{
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = ctx.chartWidth * i / numVLines;
			CString timeLabel(timelinePoint[idx].time.c_str());
			if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.TextOut(labelX, ctx.macdChartTop + ctx.macdChartHeight + g_data.RDPI(2), timeLabel);
		}
	}
}

void CFloatingWnd::DrawVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const STOCK::StockInfo* stockInfo /* = nullptr */, int startIndex /* = 0 */, int visibleCount /* = -1 */)
{
	if (timelinePoint.empty())
		return;

	// 计算可见范围：visibleCount <= 0 表示显示全部
	int total = static_cast<int>(timelinePoint.size());
	int endIdx = total;
	if (visibleCount > 0)
	{
		endIdx = (std::min)(total, startIndex + visibleCount);
		startIndex = (std::max)(0, (std::min)(startIndex, total - 1));
		if (endIdx <= startIndex) endIdx = startIndex + 1;
	}
	else
	{
		startIndex = 0;
	}

	STOCK::Volume maxVolume = 0;
	for (int i = startIndex; i < endIdx; i++)
	{
		if (timelinePoint[i].volume > maxVolume)
			maxVolume = timelinePoint[i].volume;
	}

	if (maxVolume == 0)
		return;

	// 动态计算柱子宽度：间距固定1像素，柱子宽度随缩放增大
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int fixedGap = 1;
	int slotWidth = totalPts > 0 ? width / totalPts : 1;
	int barWidth = max(2, slotWidth - fixedGap);
	int halfSlot = slotWidth / 2;

	for (int i = startIndex; i < endIdx; i++)
	{
		const auto& item = timelinePoint[i];
		int barX = x + static_cast<int>(width / static_cast<float>(totalPts) * i) + halfSlot - barWidth / 2;

		float ratio = static_cast<float>(item.volume) / maxVolume;
		int barHeight = static_cast<int>(ratio * height);
		barHeight = max(1, barHeight);

		int barY = y + height - barHeight;

		COLORREF color = COLOR_GREEN_DOWN;
		if (i > 0)
		{
			if (item.price >= timelinePoint[i - 1].price)
				color = COLOR_RED_UP;
		}

		CBrush brush(color);
		memDC.FillRect(CRect(barX, barY, barX + barWidth, y + height), &brush);
	}

	if (m_isHoveringVolume && m_hoveredBarIndex >= 0 && m_hoveredBarIndex < timelinePoint.size())
	{
		const auto& item = timelinePoint[m_hoveredBarIndex];
		int barX = x + static_cast<int>(width / static_cast<float>(totalPts) * m_hoveredBarIndex) + halfSlot - barWidth / 2;

		float ratio = static_cast<float>(timelinePoint[m_hoveredBarIndex].volume) / maxVolume;
		int barHeight = static_cast<int>(ratio * height);
		barHeight = max(1, barHeight);
		int barY = y + height - barHeight;

		COLORREF color = COLOR_GREEN_DOWN;
		if (m_hoveredBarIndex > 0)
		{
			if (timelinePoint[m_hoveredBarIndex].price >= timelinePoint[m_hoveredBarIndex - 1].price)
				color = COLOR_RED_UP;
		}

		CPen highlightPen(PS_SOLID, 2, color);
		CPen* pOldPen = memDC.SelectObject(&highlightPen);
		memDC.Rectangle(CRect(barX - 1, barY - 1, barX + barWidth + 1, y + height + 1));
		memDC.SelectObject(pOldPen);
	}
}

// ========== K线图公共辅助函数 ==========

CFloatingWnd::KLineDrawData CFloatingWnd::PrepareKLineDrawData(int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData)
{
	KLineDrawData d = {};
	d.x = x; d.y = y; d.w = w; d.h = h;
	d.klineData = &klineData;

	d.displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
	d.startIndex = klineData.size() - d.displayCount;

	d.maxPrice = 0;
	d.minPrice = (std::numeric_limits<STOCK::Price>::max)();
	for (int i = d.startIndex; i < klineData.size(); i++)
	{
		if (klineData[i].high > 0)
			d.maxPrice = max(d.maxPrice, klineData[i].high);
		if (klineData[i].low > 0)
			d.minPrice = min(d.minPrice, klineData[i].low);
	}

	const int minBarWidth = 7;
	const int gap = 1;
	d.maxVisibleKlines = min(d.displayCount, w / (minBarWidth + gap));
	d.scrollRange = max(0, d.displayCount - d.maxVisibleKlines);
	d.scrollPos = min(m_scrollOffset, d.scrollRange);
	d.finalStartIndex = max(d.startIndex, static_cast<int>(klineData.size()) - d.maxVisibleKlines - d.scrollPos);
	d.barWidth = max(minBarWidth, (w - gap * (d.maxVisibleKlines - 1)) / d.maxVisibleKlines);
	d.gap = gap;

	if (d.maxPrice > d.minPrice)
		d.unitY = h / (d.maxPrice - d.minPrice);

	return d;
}

void CFloatingWnd::CalculatePeriodHighsLows(const KLineDrawData& drawData, PeriodPoint periodHighs[3], PeriodPoint periodLows[3], bool useClose)
{
	const auto& klineData = *drawData.klineData;
	const int DAYS_PER_YEAR = 250;

	for (int p = 1; p <= 3; p++)
	{
		int rangeEnd = klineData.size() - (p - 1) * DAYS_PER_YEAR;
		int rangeStart = max(drawData.startIndex, static_cast<int>(klineData.size()) - p * DAYS_PER_YEAR);
		if (rangeStart >= rangeEnd) continue;

		STOCK::Price hh = 0, ll = (std::numeric_limits<STOCK::Price>::max)();
		int hIdx = -1, lIdx = -1;
		for (int i = rangeStart; i < rangeEnd; i++)
		{
			STOCK::Price price = useClose ? klineData[i].close : klineData[i].high;
			STOCK::Price lowPrice = useClose ? klineData[i].close : klineData[i].low;
			if (price > 0 && price > hh) { hh = price; hIdx = i; }
			if (lowPrice > 0 && lowPrice < ll) { ll = lowPrice; lIdx = i; }
		}
		periodHighs[p - 1] = { hIdx, hh, hIdx >= 0 ? klineData[hIdx].day : "" };
		periodLows[p - 1] = { lIdx, ll, lIdx >= 0 ? klineData[lIdx].day : "" };
	}
}

std::vector<CFloatingWnd::LabelInfo> CFloatingWnd::DrawKLineMonthLines(CDC& memDC, const KLineDrawData& drawData)
{
	const auto& klineData = *drawData.klineData;
	std::vector<LabelInfo> labelInfos;

	int interval = 1;
	if (m_klinePeriodDays <= 250) interval = 1;
	else if (m_klinePeriodDays <= 500) interval = 2;
	else interval = 3;

	int startMonth = -1, startYear = -1;
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		if (item.day.length() >= 7)
		{
			startYear = atoi(item.day.substr(0, 4).c_str());
			startMonth = atoi(item.day.substr(5, 2).c_str());
			if (startMonth > 0 && startYear > 0) break;
		}
	}
	if (startMonth <= 0 || startYear <= 0) return labelInfos;

	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);

	int lastMonth = -1, lastYear = -1;
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		int year = -1, month = -1;
		if (item.day.length() >= 7)
		{
			year = atoi(item.day.substr(0, 4).c_str());
			month = atoi(item.day.substr(5, 2).c_str());
		}

		if (month > 0 && year > 0)
		{
			int monthDiff = (year - startYear) * 12 + (month - startMonth);
			if (monthDiff % interval == 0 && (month != lastMonth || year != lastYear))
			{
				int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
				memDC.MoveTo(barX, drawData.y);
				memDC.LineTo(barX, drawData.y + drawData.h);
				labelInfos.push_back({ year, month, barX });
				lastMonth = month;
				lastYear = year;
			}
		}
	}
	return labelInfos;
}

void CFloatingWnd::DrawKLineMonthLabels(CDC& memDC, const KLineDrawData& drawData, const std::vector<LabelInfo>& labelInfos)
{
	if (labelInfos.empty()) return;

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	int lastLabelYear = -1;
	int labelTop = drawData.y + drawData.h + g_data.RDPI(2);
	CString tempLabel = _T("2025年12月");
	CSize tempSize = memDC.GetTextExtent(tempLabel);
	int labelBottom = labelTop + tempSize.cy;

	CRect fullLabelClearRect(drawData.x - tempSize.cx, labelTop, drawData.x + drawData.w + tempSize.cx, labelBottom + 4);
	memDC.FillSolidRect(fullLabelClearRect, COLOR_WHITE);

	lastLabelYear = -1;
	for (size_t i = 0; i < labelInfos.size(); i++)
	{
		const LabelInfo& info = labelInfos[i];
		int year = info.year;
		int month = info.month;
		int barX = info.barX;

		if (barX < drawData.x - tempSize.cx || barX > drawData.x + drawData.w + tempSize.cx)
		{
			if (lastLabelYear != -1) lastLabelYear = year;
			continue;
		}

		CString label;
		if (lastLabelYear == -1 || year != lastLabelYear)
			label.Format(_T("%d年%d月"), year, month);
		else
			label.Format(_T("%d月"), month);

		CSize labelSize = memDC.GetTextExtent(label);
		int labelX = barX - labelSize.cx / 2;
		labelX = max(drawData.x, labelX);
		if (labelX + labelSize.cx > drawData.x + drawData.w)
			labelX = drawData.x + drawData.w - labelSize.cx;
		labelX = max(drawData.x, labelX);

		memDC.SetBkMode(OPAQUE);
		memDC.SetBkColor(COLOR_WHITE);
		memDC.ExtTextOut(labelX, labelTop, 0, NULL, label, NULL);

		lastLabelYear = year;
	}
}

void CFloatingWnd::DrawKLineGrid(CDC& memDC, const KLineDrawData& drawData)
{
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);
	for (int i = 0; i <= 6; i++)
	{
		int gridY = drawData.y + static_cast<int>(i * drawData.h / 6.0f);
		memDC.MoveTo(drawData.x, gridY);
		memDC.LineTo(drawData.x + drawData.w, gridY);
	}
}

void CFloatingWnd::DrawYearAverageLines(CDC& memDC, const KLineDrawData& drawData)
{
	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto* klineObj = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;
	if (!klineObj) return;

	STOCK::Price avg1Year = klineObj->CalculateMAPeriod(1, 1);
	STOCK::Price avg2Year = klineObj->CalculateMAPeriod(2, 1);
	STOCK::Price avg3Year = klineObj->CalculateMAPeriod(3, 1);

	auto drawLine = [&](STOCK::Price price, COLORREF color) {
		if (price > 0 && price >= drawData.minPrice && price <= drawData.maxPrice)
		{
			int avgY = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
			CPen avgPen(PS_DOT, 1, color);
			memDC.SelectObject(&avgPen);
			memDC.MoveTo(drawData.x, avgY);
			memDC.LineTo(drawData.x + drawData.w, avgY);
		}
		};

	drawLine(avg1Year, COLOR_BLUE_AVG1);
	drawLine(avg2Year, COLOR_GREEN_AVG2);
	drawLine(avg3Year, COLOR_GREEN_AVG3);

	auto drawLabel = [&](STOCK::Price price, COLORREF color) {
		if (price > 0 && price >= drawData.minPrice && price <= drawData.maxPrice)
		{
			int avgY = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
			memDC.SetTextColor(color);
			CString priceTxt = CCommon::FormatFloat(price);
			CSize priceSize = memDC.GetTextExtent(priceTxt);
			memDC.TextOut(drawData.x + g_data.RDPI(2), avgY - priceSize.cy, priceTxt);
		}
		};

	drawLabel(avg1Year, COLOR_BLUE_AVG1);
	drawLabel(avg2Year, COLOR_GREEN_AVG2);
	drawLabel(avg3Year, COLOR_GREEN_AVG3);
}

void CFloatingWnd::DrawMAIndicators(CDC& memDC, const KLineDrawData& drawData)
{
	if (!m_showMA) return;

	struct MAConfig {
		int period;
		COLORREF color;
		int width;
	};
	const MAConfig maConfigs[] = {
		{ 5, RGB(100, 100, 100), 1 },
		{ 13, RGB(255, 130, 0), 1 },
		{ 34, RGB(0, 80, 200), 1 },
		{ 55, RGB(0, 120, 60), 1 },
	};

	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto* klineObj = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;

	for (const auto& config : maConfigs)
	{
		double maValue = klineObj ? klineObj->CalculateMA(config.period) : 0;
		if (maValue > 0 && maValue >= drawData.minPrice && maValue <= drawData.maxPrice)
		{
			int maY = drawData.y + static_cast<int>((drawData.maxPrice - maValue) * drawData.unitY);
			CPen maPen(PS_DOT, config.width, config.color);
			memDC.SelectObject(&maPen);
			memDC.MoveTo(drawData.x, maY);
			memDC.LineTo(drawData.x + drawData.w, maY);

			CString maLabel;
			maLabel.Format(_T("MA%d"), config.period);
			memDC.SetTextColor(config.color);
			CSize labelSize = memDC.GetTextExtent(maLabel);
			memDC.TextOut(drawData.x + drawData.w - labelSize.cx - g_data.RDPI(2), maY - labelSize.cy, maLabel);
		}
	}
}

void CFloatingWnd::DrawCurrentPriceLine(CDC& memDC, const KLineDrawData& drawData)
{
	const auto& stockInfo = *drawData.stockInfo;
	if (stockInfo.currentPrice <= 0 || stockInfo.currentPrice < drawData.minPrice || stockInfo.currentPrice > drawData.maxPrice)
		return;

	int currentPriceY = drawData.y + static_cast<int>((drawData.maxPrice - stockInfo.currentPrice) * drawData.unitY);
	COLORREF currentPriceColor = stockInfo.currentPrice >= stockInfo.prevClosePrice ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	CPen currentPricePen(PS_DASH, 1, currentPriceColor);
	memDC.SelectObject(&currentPricePen);
	memDC.MoveTo(drawData.x, currentPriceY);
	memDC.LineTo(drawData.x + drawData.w, currentPriceY);

	CString currentPriceTxt = CCommon::FormatFloat(stockInfo.currentPrice);
	memDC.SetTextColor(currentPriceColor);
	CSize cpSize = memDC.GetTextExtent(currentPriceTxt);
	memDC.TextOut(drawData.x + g_data.RDPI(2), currentPriceY - cpSize.cy, currentPriceTxt);

	CString latestTxt = _T("new");
	CSize latestSize = memDC.GetTextExtent(latestTxt);
	memDC.TextOut(drawData.x + drawData.w - latestSize.cx - g_data.RDPI(2), currentPriceY - latestSize.cy, latestTxt);
}

void CFloatingWnd::DrawPriceLabels(CDC& memDC, const KLineDrawData& drawData)
{
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	double step = (drawData.maxPrice - drawData.minPrice) / 6.0;
	for (int i = 0; i <= 6; i++)
	{
		double price = drawData.maxPrice - i * step;
		CString priceTxt = CCommon::FormatFloat(price);
		int y = drawData.y + static_cast<int>(i * drawData.h / 6.0f);
		int textH = memDC.GetTextExtent(priceTxt).cy;
		// 顶部标签紧贴网格线下方，底部标签紧贴网格线上方，中间居中
		int labelY;
		if (i == 0) labelY = y + g_data.RDPI(1);
		else if (i == 6) labelY = y - textH - g_data.RDPI(1);
		else labelY = y - textH / 2;
		memDC.TextOut(drawData.x + g_data.RDPI(2), labelY, priceTxt);
	}
}

void CFloatingWnd::DrawAverageLabels(CDC& memDC, const KLineDrawData& drawData)
{
	(void)memDC; (void)drawData;
}

// ========== K线图绘制 ==========

void CFloatingWnd::DrawKLineBars(CDC& memDC, const KLineDrawData& drawData)
{
	const auto& klineData = *drawData.klineData;
	std::wstring buyDate = g_data.GetBuyDate(m_stock_id);

	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);

		int openY = drawData.y + static_cast<int>((drawData.maxPrice - item.open) * drawData.unitY);
		int closeY = drawData.y + static_cast<int>((drawData.maxPrice - item.close) * drawData.unitY);
		int highY = drawData.y + static_cast<int>((drawData.maxPrice - item.high) * drawData.unitY);
		int lowY = drawData.y + static_cast<int>((drawData.maxPrice - item.low) * drawData.unitY);

		COLORREF color = (item.close >= item.open) ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		CPen linePen(PS_SOLID, 1, color);
		memDC.SelectObject(&linePen);
		memDC.MoveTo(barX + drawData.barWidth / 2, highY);
		memDC.LineTo(barX + drawData.barWidth / 2, lowY);

		int bodyTop = min(openY, closeY);
		int bodyBottom = max(openY, closeY);
		CBrush brush(color);
		memDC.FillRect(CRect(barX, bodyTop, barX + drawData.barWidth, bodyBottom), &brush);

		if (!buyDate.empty())
		{
			std::string itemDayStr(item.day.begin(), item.day.end());
			std::string buyDateStr(buyDate.begin(), buyDate.end());
			if (itemDayStr == buyDateStr)
			{
				CString buyTxt = _T("买");
				CSize buySize = memDC.GetTextExtent(buyTxt);
				int circleRadius = max(buySize.cx, buySize.cy) / 2;
				int buyX = barX + drawData.barWidth / 2;
				int buyY = highY - g_data.RDPI(2) - buySize.cy / 2;

				CPen circlePen(PS_SOLID, 2, COLOR_GOLDEN);
				CPen* pOldPen = memDC.SelectObject(&circlePen);
				CBrush circleBrush(COLOR_GOLDEN);
				memDC.SelectObject(&circleBrush);
				memDC.Ellipse(buyX - circleRadius, buyY - circleRadius, buyX + circleRadius, buyY + circleRadius);
				memDC.SetTextColor(COLOR_BLACK);
				memDC.TextOut(buyX - buySize.cx / 2, buyY - buySize.cy / 2, buyTxt);
				memDC.SelectObject(pOldPen);
			}
		}
	}
}

void CFloatingWnd::DrawKLineBuyMarkers(CDC& memDC, const KLineDrawData& drawData)
{
	const auto& klineData = *drawData.klineData;
	std::wstring buyDate = g_data.GetBuyDate(m_stock_id);
	if (buyDate.empty()) return;

	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		std::string itemDayStr(item.day.begin(), item.day.end());
		std::string buyDateStr(buyDate.begin(), buyDate.end());
		if (itemDayStr == buyDateStr)
		{
			int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
			int highY = drawData.y + static_cast<int>((drawData.maxPrice - item.high) * drawData.unitY);

			CString buyTxt = _T("买");
			CSize buySize = memDC.GetTextExtent(buyTxt);
			int circleRadius = max(buySize.cx, buySize.cy) / 2;
			int buyX = barX + drawData.barWidth / 2;
			int buyY = highY - g_data.RDPI(2) - buySize.cy / 2;

			CPen circlePen(PS_SOLID, 2, COLOR_GOLDEN);
			CPen* pOldPen = memDC.SelectObject(&circlePen);
			CBrush circleBrush(COLOR_GOLDEN);
			memDC.SelectObject(&circleBrush);
			memDC.Ellipse(buyX - circleRadius, buyY - circleRadius, buyX + circleRadius, buyY + circleRadius);
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(buyX - buySize.cx / 2, buyY - buySize.cy / 2, buyTxt);

			double costPrice = g_data.GetCostPrice(m_stock_id);
			if (costPrice > 0)
			{
				CString priceTxt = CCommon::FormatFloat(costPrice);
				CSize priceSize = memDC.GetTextExtent(priceTxt);

				CString percentTxt;
				CSize percentSize;
				if (drawData.stockInfo && drawData.stockInfo->currentPrice > 0)
				{
					double currentPrice = drawData.stockInfo->currentPrice;
					double changePercent = (costPrice - currentPrice) / currentPrice * 100;
					if (changePercent >= 0)
						percentTxt.Format(_T(" +%.1f%%"), changePercent);
					else
						percentTxt.Format(_T(" %.1f%%"), changePercent);
					percentSize = memDC.GetTextExtent(percentTxt);
				}

				int totalTextWidth = priceSize.cx + (percentTxt.IsEmpty() ? 0 : g_data.RDPI(2) + percentSize.cx);
				bool drawOnRight = (buyX + circleRadius + g_data.RDPI(2) + totalTextWidth <= drawData.x + drawData.w);

				memDC.SetTextColor(COLOR_GOLDEN);
				if (drawOnRight)
				{
					int priceX = buyX + circleRadius + g_data.RDPI(2);
					int priceY = buyY - priceSize.cy / 2;
					memDC.TextOut(priceX, priceY, priceTxt);

					if (!percentTxt.IsEmpty())
					{
						int percentX = priceX + priceSize.cx + g_data.RDPI(2);
						int percentY = buyY - percentSize.cy / 2;
						memDC.TextOut(percentX, percentY, percentTxt);
					}
				}
				else
				{
					int priceX = buyX - circleRadius - g_data.RDPI(2) - priceSize.cx;
					int priceY = buyY - priceSize.cy / 2;
					memDC.TextOut(priceX, priceY, priceTxt);

					if (!percentTxt.IsEmpty())
					{
						int percentX = priceX - g_data.RDPI(2) - percentSize.cx;
						int percentY = buyY - percentSize.cy / 2;
						memDC.TextOut(percentX, percentY, percentTxt);
					}
				}
			}

			memDC.SelectObject(pOldPen);
			break;
		}
	}
}

void CFloatingWnd::DrawKLinePeriodMarkers(CDC& memDC, const KLineDrawData& drawData, const PeriodPoint periodHighs[3], const PeriodPoint periodLows[3])
{
	std::vector<std::pair<PeriodPoint, bool>> markers;
	for (int p = 0; p < 3; p++)
	{
		if (periodHighs[p].index >= 0) markers.push_back({ periodHighs[p], true });
		if (periodLows[p].index >= 0) markers.push_back({ periodLows[p], false });
	}

	auto drawMarker = [&](const PeriodPoint& pt, bool isHigh) {
		if (pt.index < drawData.finalStartIndex || pt.index >= drawData.klineData->size()) return;
		int barX = drawData.x + (pt.index - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
		int markerY = drawData.y + static_cast<int>((drawData.maxPrice - pt.price) * drawData.unitY);

		CString txt = isHigh ? _T("高") : _T("低");
		COLORREF circleColor = isHigh ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CSize txtSize = memDC.GetTextExtent(txt);
		int circleRadius = max(txtSize.cx, txtSize.cy) / 2;
		int centerX = barX + drawData.barWidth / 2;
		int centerY = isHigh ? markerY - g_data.RDPI(2) - circleRadius : markerY + g_data.RDPI(2) + circleRadius;

		CPen circlePen(PS_SOLID, 2, circleColor);
		CPen* pOldPen = memDC.SelectObject(&circlePen);
		CBrush circleBrush(circleColor);
		memDC.SelectObject(&circleBrush);
		memDC.Ellipse(centerX - circleRadius, centerY - circleRadius, centerX + circleRadius, centerY + circleRadius);
		memDC.SetTextColor(COLOR_WHITE);
		memDC.TextOut(centerX - txtSize.cx / 2, centerY - txtSize.cy / 2, txt);

		CString priceTxt = CCommon::FormatFloat(pt.price);
		CSize priceSize = memDC.GetTextExtent(priceTxt);

		CString percentTxt;
		CSize percentSize;
		if (drawData.stockInfo && drawData.stockInfo->currentPrice > 0)
		{
			double currentPrice = drawData.stockInfo->currentPrice;
			double changePercent = (pt.price - currentPrice) / currentPrice * 100;
			if (changePercent >= 0)
				percentTxt.Format(_T(" +%.1f%%"), changePercent);
			else
				percentTxt.Format(_T(" %.1f%%"), changePercent);
			percentSize = memDC.GetTextExtent(percentTxt);
		}

		int totalTextWidth = priceSize.cx + (percentTxt.IsEmpty() ? 0 : g_data.RDPI(2) + percentSize.cx);
		bool drawOnRight = (centerX + circleRadius + g_data.RDPI(2) + totalTextWidth <= drawData.x + drawData.w);

		memDC.SetTextColor(circleColor);
		if (drawOnRight)
		{
			int priceX = centerX + circleRadius + g_data.RDPI(2);
			int priceY = centerY - priceSize.cy / 2;
			memDC.TextOut(priceX, priceY, priceTxt);

			if (!percentTxt.IsEmpty())
			{
				int percentX = priceX + priceSize.cx + g_data.RDPI(2);
				int percentY = centerY - percentSize.cy / 2;
				memDC.TextOut(percentX, percentY, percentTxt);
			}
		}
		else
		{
			int priceX = centerX - circleRadius - g_data.RDPI(2) - priceSize.cx;
			int priceY = centerY - priceSize.cy / 2;
			memDC.TextOut(priceX, priceY, priceTxt);

			if (!percentTxt.IsEmpty())
			{
				int percentX = priceX - g_data.RDPI(2) - percentSize.cx;
				int percentY = centerY - percentSize.cy / 2;
				memDC.TextOut(percentX, percentY, percentTxt);
			}
		}

		memDC.SelectObject(pOldPen);
		};

	for (const auto& m : markers) drawMarker(m.first, m.second);
}

void CFloatingWnd::DrawKLineChart(CDC& memDC, int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData, const STOCK::StockInfo& stockInfo)
{
	if (klineData.empty())
		return;

	const int xAxisLabelHeight = g_data.RDPI(20);
	const int totalHeight = h + xAxisLabelHeight + g_data.RDPI(4);
	const int paddingY = g_data.RDPI(10);
	memDC.SaveDC();
	memDC.IntersectClipRect(CRect(0, y, w, y + totalHeight));
	memDC.FillSolidRect(CRect(0, y, w, y + totalHeight), COLOR_WHITE);

	KLineDrawData drawData = PrepareKLineDrawData(x, y + paddingY, w, h - paddingY * 2, klineData);
	drawData.stockInfo = &stockInfo;

	// 只统计实际可见范围（finalStartIndex到末尾）的最高最低价，避免缩放后Y轴包含不可见数据导致走势被压缩
	drawData.maxPrice = 0;
	drawData.minPrice = (std::numeric_limits<STOCK::Price>::max)();
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		if (klineData[i].high > 0)
			drawData.maxPrice = max(drawData.maxPrice, klineData[i].high);
		if (klineData[i].low > 0)
			drawData.minPrice = min(drawData.minPrice, klineData[i].low);
	}

	// 计算周期高低点（仅用于标记显示）
	PeriodPoint periodHighs[3] = {};
	PeriodPoint periodLows[3] = {};
	CalculatePeriodHighsLows(drawData, periodHighs, periodLows);

	// Y轴固定6等分7根横线：以可见最高/最低价的中点为Y轴中线，向上下对称扩展（与5分钟K线一致）
	if (drawData.maxPrice > drawData.minPrice)
	{
		const double DIV_COUNT = 6.0;
		const double MIN_STEP = 0.001;
		double range = drawData.maxPrice - drawData.minPrice;
		if (range <= 0) range = MIN_STEP;

		double rawStep = range / (DIV_COUNT - 2.0);
		if (rawStep <= 0) rawStep = MIN_STEP;
		double mag = pow(10.0, floor(log10(rawStep)));
		double norm = rawStep / mag;
		double niceStep;
		if (norm <= 1.0) niceStep = 1.0 * mag;
		else if (norm <= 2.0) niceStep = 2.0 * mag;
		else if (norm <= 5.0) niceStep = 5.0 * mag;
		else niceStep = 10.0 * mag;
		niceStep = (std::max)(niceStep, MIN_STEP);

		double centerPrice = (drawData.minPrice + drawData.maxPrice) / 2.0;
		double halfAxisRange = (DIV_COUNT / 2.0) * niceStep;
		drawData.minPrice = centerPrice - halfAxisRange;
		drawData.maxPrice = centerPrice + halfAxisRange;
		drawData.unitY = drawData.h / (drawData.maxPrice - drawData.minPrice);
	}

	if (drawData.maxPrice <= drawData.minPrice) { memDC.RestoreDC(-1); return; }

	// 绘制各部分
	DrawKLineGrid(memDC, drawData);
	DrawYearAverageLines(memDC, drawData);
	DrawPriceLabels(memDC, drawData);
	DrawMAIndicators(memDC, drawData);
	std::vector<LabelInfo> labelInfos = DrawKLineMonthLines(memDC, drawData);
	// 布林带：在K线下层绘制（仅当 m_showBollBands 开启时）
	if (m_showBollBands)
	{
		DrawBollBands(memDC, drawData);
	}
	// 振幅上下线：在K线图中绘制振幅曲线（仅当 m_showAmplitudeBands 开启时）
	if (m_showAmplitudeBands)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		auto* dayKLineObj = stockData ? stockData->getKLineData() : nullptr;
		double avgAmplitude = dayKLineObj ? dayKLineObj->CalculateAverageAmplitude(5) : 0;
		if (avgAmplitude > 0)
		{
			double ampRatio = avgAmplitude / 100.0 / 2.0;

			// 获取分时均价数据
			auto* tlObj = stockData ? stockData->getTimelineData() : nullptr;
			if (tlObj && !tlObj->data.empty() && drawData.klineData && !drawData.klineData->empty())
			{
				const auto& klineRef = *drawData.klineData;
				int n = (int)klineRef.size();
				auto priceToY = [&](double price) -> int {
					int py = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
					return max(drawData.y, min(py, drawData.y + drawData.h));
					};

				// 绘制振幅上轨曲线（红色）
				{
					CPen upperPen(PS_SOLID, 1, COLOR_RED_UP);
					memDC.SelectObject(&upperPen);
					bool first = true;
					for (int i = drawData.finalStartIndex; i < n && i < drawData.finalStartIndex + drawData.displayCount; i++)
					{
						// 将日K收盘价作为"均价"近似值来计算振幅上下轨
						double avgP = klineRef[i].close;
						if (avgP <= 0) { first = true; continue; }
						double upperPrice = avgP * (1 + ampRatio);
						int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
						int py = priceToY(upperPrice);
						if (first) { memDC.MoveTo(barX + drawData.barWidth / 2, py); first = false; }
						else { memDC.LineTo(barX + drawData.barWidth / 2, py); }
					}
				}
				// 绘制振幅下轨曲线（绿色）
				{
					CPen lowerPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
					memDC.SelectObject(&lowerPen);
					bool first = true;
					for (int i = drawData.finalStartIndex; i < n && i < drawData.finalStartIndex + drawData.displayCount; i++)
					{
						double avgP = klineRef[i].close;
						if (avgP <= 0) { first = true; continue; }
						double lowerPrice = avgP * (1 - ampRatio);
						int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
						int py = priceToY(lowerPrice);
						if (first) { memDC.MoveTo(barX + drawData.barWidth / 2, py); first = false; }
						else { memDC.LineTo(barX + drawData.barWidth / 2, py); }
					}
				}
			}

			// 在右端标注价格（基于分时最后均价）
			double lastAvgP = 0;
			if (tlObj && !tlObj->data.empty())
				lastAvgP = tlObj->data.back().averagePrice;
			if (lastAvgP <= 0 && drawData.stockInfo)
				lastAvgP = drawData.stockInfo->currentPrice;
			if (lastAvgP > 0)
			{
				auto priceToY = [&](double price) -> int {
					int py = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
					return max(drawData.y, min(py, drawData.y + drawData.h));
					};
				CString upperLabel, lowerLabel;
				upperLabel.Format(_T("%.2f"), lastAvgP * (1 + ampRatio));
				lowerLabel.Format(_T("%.2f"), lastAvgP * (1 - ampRatio));
				int labelX = drawData.x + drawData.w + 2;
				int upperY = priceToY(lastAvgP * (1 + ampRatio));
				int lowerY = priceToY(lastAvgP * (1 - ampRatio));
				memDC.SetTextColor(COLOR_RED_UP);
				memDC.TextOut(labelX, upperY - memDC.GetTextExtent(upperLabel).cy / 2, upperLabel);
				memDC.SetTextColor(COLOR_GREEN_DOWN);
				memDC.TextOut(labelX, lowerY - memDC.GetTextExtent(lowerLabel).cy / 2, lowerLabel);
			}
		}
	}
	DrawKLineBars(memDC, drawData);
	DrawKLinePeriodMarkers(memDC, drawData, periodHighs, periodLows);
	DrawCurrentPriceLine(memDC, drawData);

	// X轴区域
	memDC.FillSolidRect(CRect(0, y + h + 1, w, y + totalHeight), COLOR_WHITE);
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);
	memDC.MoveTo(x, y + h);
	memDC.LineTo(x + w, y + h);

	DrawKLineMonthLabels(memDC, drawData, labelInfos);
	// 5分钟/30分钟K线模式：在 X 轴标签区域绘制整点时间线
	if (m_isMin5KLineMode || m_isMin30KLineMode)
	{
		DrawMin5HourLines(memDC, drawData);
	}

	memDC.RestoreDC(-1);

	// 悬停提示
	if (m_isHoveringKLine || m_isHoveringKLineVolume)
	{
		int hoveredIdx = m_klineHoveredBarIndex;
		if (hoveredIdx >= drawData.finalStartIndex && hoveredIdx < klineData.size())
		{
			int barX = drawData.x + (hoveredIdx - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
			CPen crossPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&crossPen);
			memDC.MoveTo(barX + drawData.barWidth / 2, y);
			memDC.LineTo(barX + drawData.barWidth / 2, y + h);

			if (!m_klineHoverTip.IsEmpty())
			{
				memDC.SetTextColor(COLOR_GRAY_TEXT);
				CSize tipSize = memDC.GetTextExtent(m_klineHoverTip);
				int textX = w / 2 - tipSize.cx / 2;
				textX = max(g_data.RDPI(5), min(textX, w - g_data.RDPI(5) - tipSize.cx));
				int textY = g_data.RDPI(26) + g_data.RDPI(2);
				memDC.DrawText(m_klineHoverTip, CRect(textX, textY, textX + tipSize.cx, textY + tipSize.cy), DT_LEFT | DT_TOP | DT_SINGLELINE);
			}
		}
	}
}

// ========== 走势图绘制 ==========

void CFloatingWnd::DrawKLineTrendCurve(CDC& memDC, const KLineDrawData& drawData, std::vector<CPoint>& outPoints)
{
	const auto& klineData = *drawData.klineData;
	outPoints.clear();

	CPen pLine(PS_SOLID, 1, COLOR_DARK_GRAY_BORDER);
	memDC.SelectObject(&pLine);

	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		int pointX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap) + drawData.barWidth / 2;
		int pointY = drawData.y + static_cast<int>((drawData.maxPrice - item.close) * drawData.unitY);
		outPoints.push_back(CPoint(pointX, pointY));
	}

	if (!outPoints.empty())
	{
		memDC.MoveTo(outPoints[0].x, outPoints[0].y);
		for (size_t i = 1; i < outPoints.size(); i++)
			memDC.LineTo(outPoints[i].x, outPoints[i].y);
	}
}

void CFloatingWnd::DrawKLineTrendBuyMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints)
{
	const auto& klineData = *drawData.klineData;
	std::wstring buyDate = g_data.GetBuyDate(m_stock_id);
	if (buyDate.empty() || closePoints.empty()) return;

	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		std::string itemDayStr(item.day.begin(), item.day.end());
		std::string buyDateStr(buyDate.begin(), buyDate.end());
		if (itemDayStr == buyDateStr)
		{
			int pointIdx = i - drawData.finalStartIndex;
			if (pointIdx >= 0 && pointIdx < closePoints.size())
			{
				CString buyTxt = _T("买");
				CSize buySize = memDC.GetTextExtent(buyTxt);
				int circleRadius = max(buySize.cx, buySize.cy) / 2;
				int buyX = closePoints[pointIdx].x;
				int buyY = closePoints[pointIdx].y - g_data.RDPI(2) - buySize.cy / 2;

				CPen circlePen(PS_SOLID, 2, COLOR_GOLDEN);
				CPen* pOldPen = memDC.SelectObject(&circlePen);
				CBrush circleBrush(COLOR_GOLDEN);
				memDC.SelectObject(&circleBrush);
				memDC.Ellipse(buyX - circleRadius, buyY - circleRadius, buyX + circleRadius, buyY + circleRadius);
				memDC.SetTextColor(COLOR_BLACK);
				memDC.TextOut(buyX - buySize.cx / 2, buyY - buySize.cy / 2, buyTxt);

				double costPrice = g_data.GetCostPrice(m_stock_id);
				if (costPrice > 0)
				{
					CString priceTxt = CCommon::FormatFloat(costPrice);
					CSize priceSize = memDC.GetTextExtent(priceTxt);

					CString percentTxt;
					CSize percentSize;
					if (drawData.stockInfo && drawData.stockInfo->currentPrice > 0)
					{
						double currentPrice = drawData.stockInfo->currentPrice;
						double changePercent = (costPrice - currentPrice) / currentPrice * 100;
						if (changePercent >= 0)
							percentTxt.Format(_T(" +%.1f%%"), changePercent);
						else
							percentTxt.Format(_T(" %.1f%%"), changePercent);
						percentSize = memDC.GetTextExtent(percentTxt);
					}

					int totalTextWidth = priceSize.cx + (percentTxt.IsEmpty() ? 0 : g_data.RDPI(2) + percentSize.cx);
					bool drawOnRight = (buyX + circleRadius + g_data.RDPI(2) + totalTextWidth <= drawData.x + drawData.w);

					memDC.SetTextColor(COLOR_GOLDEN);
					if (drawOnRight)
					{
						int priceX = buyX + circleRadius + g_data.RDPI(2);
						int priceY = buyY - priceSize.cy / 2;
						memDC.TextOut(priceX, priceY, priceTxt);

						if (!percentTxt.IsEmpty())
						{
							int percentX = priceX + priceSize.cx + g_data.RDPI(2);
							int percentY = buyY - percentSize.cy / 2;
							memDC.TextOut(percentX, percentY, percentTxt);
						}
					}
					else
					{
						int priceX = buyX - circleRadius - g_data.RDPI(2) - priceSize.cx;
						int priceY = buyY - priceSize.cy / 2;
						memDC.TextOut(priceX, priceY, priceTxt);

						if (!percentTxt.IsEmpty())
						{
							int percentX = priceX - g_data.RDPI(2) - percentSize.cx;
							int percentY = buyY - percentSize.cy / 2;
							memDC.TextOut(percentX, percentY, percentTxt);
						}
					}
				}

				memDC.SelectObject(pOldPen);
			}
			break;
		}
	}
}

void CFloatingWnd::DrawKLineTrendPeriodMarkers(CDC& memDC, const KLineDrawData& drawData, const std::vector<CPoint>& closePoints, const PeriodPoint periodHighs[3], const PeriodPoint periodLows[3])
{
	std::vector<std::pair<PeriodPoint, bool>> markers;
	for (int p = 0; p < 3; p++)
	{
		if (periodHighs[p].index >= 0) markers.push_back({ periodHighs[p], true });
		if (periodLows[p].index >= 0) markers.push_back({ periodLows[p], false });
	}

	auto drawMarker = [&](const PeriodPoint& pt, bool isHigh) {
		if (pt.index < drawData.finalStartIndex || pt.index >= drawData.klineData->size()) return;
		int pointIdx = pt.index - drawData.finalStartIndex;
		if (pointIdx < 0 || pointIdx >= closePoints.size()) return;
		CPoint ptPos = closePoints[pointIdx];

		CString txt = isHigh ? _T("高") : _T("低");
		COLORREF circleColor = isHigh ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CSize txtSize = memDC.GetTextExtent(txt);
		int circleRadius = max(txtSize.cx, txtSize.cy) / 2;
		int centerX = ptPos.x;
		int centerY = isHigh ? ptPos.y - g_data.RDPI(2) - circleRadius : ptPos.y + g_data.RDPI(2) + circleRadius;

		CPen circlePen(PS_SOLID, 2, circleColor);
		CPen* pOldPen = memDC.SelectObject(&circlePen);
		CBrush circleBrush(circleColor);
		memDC.SelectObject(&circleBrush);
		memDC.Ellipse(centerX - circleRadius, centerY - circleRadius, centerX + circleRadius, centerY + circleRadius);
		memDC.SetTextColor(COLOR_WHITE);
		memDC.TextOut(centerX - txtSize.cx / 2, centerY - txtSize.cy / 2, txt);

		CString priceTxt = CCommon::FormatFloat(pt.price);
		CSize priceSize = memDC.GetTextExtent(priceTxt);

		CString percentTxt;
		CSize percentSize;
		if (drawData.stockInfo && drawData.stockInfo->currentPrice > 0)
		{
			double currentPrice = drawData.stockInfo->currentPrice;
			double changePercent = (pt.price - currentPrice) / currentPrice * 100;
			if (changePercent >= 0)
				percentTxt.Format(_T(" +%.1f%%"), changePercent);
			else
				percentTxt.Format(_T(" %.1f%%"), changePercent);
			percentSize = memDC.GetTextExtent(percentTxt);
		}

		int totalTextWidth = priceSize.cx + (percentTxt.IsEmpty() ? 0 : g_data.RDPI(2) + percentSize.cx);
		bool drawOnRight = (centerX + circleRadius + g_data.RDPI(2) + totalTextWidth <= drawData.x + drawData.w);

		memDC.SetTextColor(circleColor);
		if (drawOnRight)
		{
			int priceX = centerX + circleRadius + g_data.RDPI(2);
			int priceY = centerY - priceSize.cy / 2;
			memDC.TextOut(priceX, priceY, priceTxt);

			if (!percentTxt.IsEmpty())
			{
				int percentX = priceX + priceSize.cx + g_data.RDPI(2);
				int percentY = centerY - percentSize.cy / 2;
				memDC.TextOut(percentX, percentY, percentTxt);
			}
		}
		else
		{
			int priceX = centerX - circleRadius - g_data.RDPI(2) - priceSize.cx;
			int priceY = centerY - priceSize.cy / 2;
			memDC.TextOut(priceX, priceY, priceTxt);

			if (!percentTxt.IsEmpty())
			{
				int percentX = priceX - g_data.RDPI(2) - percentSize.cx;
				int percentY = centerY - percentSize.cy / 2;
				memDC.TextOut(percentX, percentY, percentTxt);
			}
		}

		memDC.SelectObject(pOldPen);
		};

	for (const auto& m : markers) drawMarker(m.first, m.second);
}

void CFloatingWnd::DrawKLineTrendChart(CDC& memDC, int x, int y, int w, int h, const std::vector<STOCK::KLinePoint>& klineData, const STOCK::StockInfo& stockInfo)
{
	if (klineData.empty())
		return;

	const int xAxisLabelHeight = g_data.RDPI(20);
	const int totalHeight = h + xAxisLabelHeight + g_data.RDPI(4);
	const int paddingY = g_data.RDPI(10);
	memDC.SaveDC();
	memDC.IntersectClipRect(CRect(0, y, w, y + totalHeight));
	memDC.FillSolidRect(CRect(0, y, w, y + totalHeight), COLOR_WHITE);

	KLineDrawData drawData = PrepareKLineDrawData(x, y + paddingY, w, h - paddingY * 2, klineData);
	drawData.stockInfo = &stockInfo;

	// 只统计实际可见范围（finalStartIndex到末尾）的最高最低价，避免缩放后Y轴包含不可见数据导致走势被压缩
	drawData.maxPrice = 0;
	drawData.minPrice = (std::numeric_limits<STOCK::Price>::max)();
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		if (klineData[i].high > 0)
			drawData.maxPrice = max(drawData.maxPrice, klineData[i].high);
		if (klineData[i].low > 0)
			drawData.minPrice = min(drawData.minPrice, klineData[i].low);
	}

	PeriodPoint periodHighs[3] = {};
	PeriodPoint periodLows[3] = {};
	CalculatePeriodHighsLows(drawData, periodHighs, periodLows, true);

	// Y轴固定6等分7根横线：以可见最高/最低价的中点为Y轴中线，向上下对称扩展（与5分钟K线一致）
	if (drawData.maxPrice > drawData.minPrice)
	{
		const double DIV_COUNT = 6.0;
		const double MIN_STEP = 0.001;
		double range = drawData.maxPrice - drawData.minPrice;
		if (range <= 0) range = MIN_STEP;

		double rawStep = range / (DIV_COUNT - 2.0);
		if (rawStep <= 0) rawStep = MIN_STEP;
		double mag = pow(10.0, floor(log10(rawStep)));
		double norm = rawStep / mag;
		double niceStep;
		if (norm <= 1.0) niceStep = 1.0 * mag;
		else if (norm <= 2.0) niceStep = 2.0 * mag;
		else if (norm <= 5.0) niceStep = 5.0 * mag;
		else niceStep = 10.0 * mag;
		niceStep = (std::max)(niceStep, MIN_STEP);

		double centerPrice = (drawData.minPrice + drawData.maxPrice) / 2.0;
		double halfAxisRange = (DIV_COUNT / 2.0) * niceStep;
		drawData.minPrice = centerPrice - halfAxisRange;
		drawData.maxPrice = centerPrice + halfAxisRange;
		drawData.unitY = drawData.h / (drawData.maxPrice - drawData.minPrice);
	}

	if (drawData.maxPrice <= drawData.minPrice) { memDC.RestoreDC(-1); return; }

	DrawKLineGrid(memDC, drawData);
	DrawYearAverageLines(memDC, drawData);
	DrawPriceLabels(memDC, drawData);
	DrawMAIndicators(memDC, drawData);
	DrawCurrentPriceLine(memDC, drawData);

	std::vector<LabelInfo> labelInfos = DrawKLineMonthLines(memDC, drawData);
	// 布林带：在走势曲线下层绘制（仅当 m_showBollBands 开启时）
	if (m_showBollBands)
	{
		DrawBollBands(memDC, drawData);
	}
	// 振幅上下线（仅当 m_showAmplitudeBands 开启时）
	if (m_showAmplitudeBands)
	{
		auto stockData = g_data.GetStockData(m_stock_id);
		auto* dayKLineObj = stockData ? stockData->getKLineData() : nullptr;
		double avgAmplitude = dayKLineObj ? dayKLineObj->CalculateAverageAmplitude(5) : 0;
		if (avgAmplitude > 0)
		{
			double ampRatio = avgAmplitude / 100.0 / 2.0;

			auto* tlObj = stockData ? stockData->getTimelineData() : nullptr;
			if (tlObj && !tlObj->data.empty() && drawData.klineData && !drawData.klineData->empty())
			{
				const auto& klineRef = *drawData.klineData;
				int n = (int)klineRef.size();
				auto priceToY = [&](double price) -> int {
					int py = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
					return max(drawData.y, min(py, drawData.y + drawData.h));
					};

				// 绘制振幅上轨曲线（红色）
				{
					CPen upperPen(PS_SOLID, 1, COLOR_RED_UP);
					memDC.SelectObject(&upperPen);
					bool first = true;
					for (int i = drawData.finalStartIndex; i < n && i < drawData.finalStartIndex + drawData.displayCount; i++)
					{
						double avgP = klineRef[i].close;
						if (avgP <= 0) { first = true; continue; }
						double upperPrice = avgP * (1 + ampRatio);
						int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
						int py = priceToY(upperPrice);
						if (first) { memDC.MoveTo(barX + drawData.barWidth / 2, py); first = false; }
						else { memDC.LineTo(barX + drawData.barWidth / 2, py); }
					}
				}
				// 绘制振幅下轨曲线（绿色）
				{
					CPen lowerPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
					memDC.SelectObject(&lowerPen);
					bool first = true;
					for (int i = drawData.finalStartIndex; i < n && i < drawData.finalStartIndex + drawData.displayCount; i++)
					{
						double avgP = klineRef[i].close;
						if (avgP <= 0) { first = true; continue; }
						double lowerPrice = avgP * (1 - ampRatio);
						int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
						int py = priceToY(lowerPrice);
						if (first) { memDC.MoveTo(barX + drawData.barWidth / 2, py); first = false; }
						else { memDC.LineTo(barX + drawData.barWidth / 2, py); }
					}
				}
			}

			double lastAvgP = 0;
			if (tlObj && !tlObj->data.empty())
				lastAvgP = tlObj->data.back().averagePrice;
			if (lastAvgP <= 0 && drawData.stockInfo)
				lastAvgP = drawData.stockInfo->currentPrice;
			if (lastAvgP > 0)
			{
				auto priceToY = [&](double price) -> int {
					int py = drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
					return max(drawData.y, min(py, drawData.y + drawData.h));
					};
				CString upperLabel, lowerLabel;
				upperLabel.Format(_T("%.2f"), lastAvgP * (1 + ampRatio));
				lowerLabel.Format(_T("%.2f"), lastAvgP * (1 - ampRatio));
				int labelX = drawData.x + drawData.w + 2;
				int upperY = priceToY(lastAvgP * (1 + ampRatio));
				int lowerY = priceToY(lastAvgP * (1 - ampRatio));
				memDC.SetTextColor(COLOR_RED_UP);
				memDC.TextOut(labelX, upperY - memDC.GetTextExtent(upperLabel).cy / 2, upperLabel);
				memDC.SetTextColor(COLOR_GREEN_DOWN);
				memDC.TextOut(labelX, lowerY - memDC.GetTextExtent(lowerLabel).cy / 2, lowerLabel);
			}
		}
	}

	// 走势曲线
	std::vector<CPoint> closePoints;
	DrawKLineTrendCurve(memDC, drawData, closePoints);
	DrawKLineTrendBuyMarkers(memDC, drawData, closePoints);
	DrawKLineTrendPeriodMarkers(memDC, drawData, closePoints, periodHighs, periodLows);

	// 悬停提示
	if (m_isHoveringKLine || m_isHoveringKLineVolume)
	{
		int hoveredIdx = m_klineHoveredBarIndex;
		if (hoveredIdx >= drawData.finalStartIndex && hoveredIdx < klineData.size())
		{
			int dotIdx = hoveredIdx - drawData.finalStartIndex;
			if (dotIdx >= 0 && dotIdx < closePoints.size())
			{
				CPen crossPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
				memDC.SelectObject(&crossPen);
				memDC.MoveTo(closePoints[dotIdx].x, y);
				memDC.LineTo(closePoints[dotIdx].x, y + h);

				COLORREF dotColor = (hoveredIdx > 0 && klineData[hoveredIdx].close >= klineData[hoveredIdx - 1].close) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
				CPen dotPen(PS_SOLID, 4, dotColor);
				memDC.SelectObject(&dotPen);
				memDC.Ellipse(closePoints[dotIdx].x - 3, closePoints[dotIdx].y - 3, closePoints[dotIdx].x + 3, closePoints[dotIdx].y + 3);
			}

			// 价格信息显示在顶部居中（不含日期）
			if (!m_klineTrendHoverTip.IsEmpty())
			{
				memDC.SetTextColor(COLOR_GRAY_TEXT);
				CSize tipSize = memDC.GetTextExtent(m_klineTrendHoverTip);
				int textX = w / 2 - tipSize.cx / 2;
				textX = max(g_data.RDPI(5), min(textX, w - g_data.RDPI(5) - tipSize.cx));
				int textY = g_data.RDPI(26) + g_data.RDPI(2);
				memDC.DrawText(m_klineTrendHoverTip, CRect(textX, textY, textX + tipSize.cx, textY + tipSize.cy), DT_LEFT | DT_TOP | DT_SINGLELINE);
			}

			// 日期显示在竖线对应的X轴下方
			CString dateStr(klineData[hoveredIdx].day.c_str());
			dateStr.Replace(_T("-"), _T("/"));
			CSize dateSize = memDC.GetTextExtent(dateStr);
			int barCenterX = drawData.x + (hoveredIdx - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap) + drawData.barWidth / 2;
			int dateX = barCenterX - dateSize.cx / 2;
			dateX = max(g_data.RDPI(2), min(dateX, w - dateSize.cx - g_data.RDPI(2)));
			int dateY = y + h + g_data.RDPI(2);
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(dateX, dateY, dateStr);
		}
	}

	// X轴
	memDC.FillSolidRect(CRect(0, y + h + 1, w, y + h + xAxisLabelHeight + g_data.RDPI(4)), COLOR_WHITE);
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);
	memDC.MoveTo(x, y + h);
	memDC.LineTo(x + w, y + h);

	DrawKLineMonthLabels(memDC, drawData, labelInfos);
	memDC.RestoreDC(-1);
}

void CFloatingWnd::DrawKLineVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData)
{
	if (klineData.empty())
		return;

	memDC.SaveDC();
	memDC.IntersectClipRect(CRect(0, y - g_data.RDPI(18), width, y + height));

	// 复用 PrepareKLineDrawData 获取一致的绘制参数
	KLineDrawData drawData = PrepareKLineDrawData(x, y, width, height, klineData);
	drawData.x = x;
	drawData.w = width;
	drawData.h = height;

	STOCK::Volume maxVolume = 0;
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		if (klineData[i].volume > maxVolume)
			maxVolume = klineData[i].volume;
	}

	if (maxVolume == 0)
		return;

	// 绘制量柱
	for (int i = drawData.finalStartIndex; i < klineData.size(); i++)
	{
		const auto& item = klineData[i];
		int barX = x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);

		float ratio = static_cast<float>(item.volume) / maxVolume;
		int barHeight = static_cast<int>(ratio * height);
		barHeight = max(1, barHeight);
		int barY = y + height - barHeight;

		COLORREF color = (item.close >= item.open) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CBrush brush(color);
		memDC.FillRect(CRect(barX, barY, barX + drawData.barWidth, y + height), &brush);
	}

	// 绘制横线
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);
	for (int i = 1; i <= 3; i++)
	{
		int gridY = y + static_cast<int>(i * height / 4.0f);
		memDC.MoveTo(x, gridY);
		memDC.LineTo(x + width, gridY);
	}

	// 复用 DrawKLineMonthLines 绘制月份竖线
	DrawKLineMonthLines(memDC, drawData);

	// 成交量统计
	STOCK::Volume totalVolume = 0;
	for (int i = drawData.startIndex; i < klineData.size(); i++)
	{
		totalVolume += klineData[i].volume;
	}

	CString volumeTxt;
	STOCK::Volume totalVolumeInLots = totalVolume / 100;
	volumeTxt.Format(_T("成交量: %s"), CCommon::FormatVolumeInt(totalVolumeInLots));

	// 绘制鼠标悬停提示（量柱图和K线图同步）
	if (m_isHoveringKLine || m_isHoveringKLineVolume)
	{
		int hoveredIdx = m_klineHoveredBarIndex;

		if (hoveredIdx >= drawData.finalStartIndex && hoveredIdx < klineData.size())
		{
			int barX = x + (hoveredIdx - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);

			// 绘制十字竖线
			CPen crossPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
			CPen* pOldPen = memDC.SelectObject(&crossPen);
			memDC.MoveTo(barX + drawData.barWidth / 2, y);
			memDC.LineTo(barX + drawData.barWidth / 2, y + height);
			memDC.SelectObject(pOldPen);

			// 绘制量柱提示信息 - 居中显示在原标题位置
			if (!m_klineVolumeHoverTip.IsEmpty())
			{
				memDC.SetTextColor(COLOR_GRAY_TEXT);
				CSize tipSize = memDC.GetTextExtent(m_klineVolumeHoverTip);
				// 水平居中
				int textX = x + width / 2 - tipSize.cx / 2;
				textX = max(g_data.RDPI(5), min(textX, x + width - g_data.RDPI(5) - tipSize.cx));

				// 垂直位置在成交量图上方标题区域，居中显示
				int textY = y - g_data.RDPI(18) + (g_data.RDPI(18) - tipSize.cy) / 2;

				memDC.DrawText(m_klineVolumeHoverTip, CRect(textX, textY, textX + tipSize.cx, textY + tipSize.cy), DT_LEFT | DT_TOP | DT_SINGLELINE);
			}
		}
	}

	memDC.RestoreDC(-1);
}

void CFloatingWnd::DrawKLinePositionInfo(CDC& memDC, int x, int y, int chartWidth, const STOCK::StockInfo& realtimeData)
{
	double costPrice = g_data.GetCostPrice(m_stock_id);
	double holdingCount = g_data.GetHoldingCount(m_stock_id);

	CString countLabel = _T("持仓:");
	CString countValue;
	if (holdingCount > 0)
		countValue = CCommon::FormatVolumeInt(holdingCount);
	else
		countValue = _T("--");

	STOCK::StockData::PositionInfo posInfo = {};
	{
		auto stockDataPtr = g_data.GetStockData(m_stock_id);
		if (stockDataPtr)
		{
			posInfo = stockDataPtr->CalculatePositionInfo(costPrice, holdingCount);
		}
	}

	CString totalCostLabel = _T(" 成本:");
	CString totalCostValue;
	if (posInfo.totalCost > 0)
		totalCostValue = CCommon::FormatAmount(posInfo.totalCost);
	else
		totalCostValue = _T("--");

	CString marketValueLabel = _T(" 市值:");
	CString marketValueValue;
	if (posInfo.marketValue > 0)
		marketValueValue = CCommon::FormatAmount(posInfo.marketValue);
	else
		marketValueValue = _T("--");

	CString profitLossLabel = _T(" 盈亏:");
	CString profitLossValue;
	COLORREF profitLossColor = COLOR_BLACK;
	if (posInfo.totalCost > 0)
	{
		profitLossColor = posInfo.profitLoss >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		profitLossValue = CCommon::FormatProfitLoss(posInfo.profitLossPercent, posInfo.profitLoss, true);
	}
	else
		profitLossValue = _T("--");

	CString todayProfitLossLabel = _T(" 当日盈亏:");
	CString todayProfitLossValue;
	COLORREF todayProfitLossColor = COLOR_BLACK;
	if (holdingCount > 0 && realtimeData.prevClosePrice != 0 && realtimeData.currentPrice > 0)
	{
		double todayProfitLoss = (realtimeData.currentPrice - realtimeData.prevClosePrice) * holdingCount;
		double todayProfitLossPercent = (realtimeData.currentPrice - realtimeData.prevClosePrice) / realtimeData.prevClosePrice * 100;
		todayProfitLossColor = todayProfitLoss >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		todayProfitLossValue = CCommon::FormatProfitLoss(todayProfitLossPercent, todayProfitLoss, false);
	}
	else
		todayProfitLossValue = _T("--");

	CString labelsTxt = countLabel + totalCostLabel + marketValueLabel + profitLossLabel + todayProfitLossLabel;
	CString valuesTxt = countValue + totalCostValue + marketValueValue + profitLossValue + todayProfitLossValue;
	CSize labelsSize = memDC.GetTextExtent(labelsTxt);
	CSize valuesSize = memDC.GetTextExtent(valuesTxt);
	int totalWidth = labelsSize.cx + valuesSize.cx;

	int startX = (chartWidth - totalWidth) / 2;
	startX = max(g_data.RDPI(5), startX);

	memDC.SetTextColor(COLOR_BLACK);
	memDC.TextOut(startX, y, countLabel);
	memDC.TextOut(startX + memDC.GetTextExtent(countLabel).cx, y, countValue);

	int posX = startX + memDC.GetTextExtent(countLabel).cx + memDC.GetTextExtent(countValue).cx;
	memDC.TextOut(posX, y, totalCostLabel);
	memDC.TextOut(posX + memDC.GetTextExtent(totalCostLabel).cx, y, totalCostValue);

	posX += memDC.GetTextExtent(totalCostLabel).cx + memDC.GetTextExtent(totalCostValue).cx;
	memDC.TextOut(posX, y, marketValueLabel);
	memDC.TextOut(posX + memDC.GetTextExtent(marketValueLabel).cx, y, marketValueValue);

	posX += memDC.GetTextExtent(marketValueLabel).cx + memDC.GetTextExtent(marketValueValue).cx;
	memDC.TextOut(posX, y, profitLossLabel);
	memDC.SetTextColor(profitLossColor);
	memDC.TextOut(posX + memDC.GetTextExtent(profitLossLabel).cx, y, profitLossValue);

	posX += memDC.GetTextExtent(profitLossLabel).cx + memDC.GetTextExtent(profitLossValue).cx;
	memDC.SetTextColor(COLOR_BLACK);
	memDC.TextOut(posX, y, todayProfitLossLabel);
	memDC.SetTextColor(todayProfitLossColor);
	memDC.TextOut(posX + memDC.GetTextExtent(todayProfitLossLabel).cx, y, todayProfitLossValue);
}

void CFloatingWnd::DrawKLineInfoPanel(CDC& memDC, int left, int right, int bottomY, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData)
{
	int textX = left + g_data.RDPI(5) + 3;
	int topY = g_data.RDPI(24);
	// bottomY 由调用方传入，即持仓信息栏的起始位置
	int availableHeight = bottomY - topY;
	if (availableHeight <= 0) return;

	memDC.SetBkMode(TRANSPARENT);

	std::wstring buyDate = g_data.GetBuyDate(m_stock_id);
	double costPrice = g_data.GetCostPrice(m_stock_id);
	double holdingCount = g_data.GetHoldingCount(m_stock_id);

	const int DAYS_PER_YEAR = 250;
	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto* klineObj = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;

	struct PeriodStats
	{
		STOCK::Price maxPrice;
		STOCK::Price minPrice;
		std::string maxDate;
		std::string minDate;
	};

	// 分段计算最高价和最低价：1年=最近250天，2年=251-500天，3年=501-750天
	std::map<int, PeriodStats> periodStatsMap;
	for (int year : {1, 2, 3})
	{
		PeriodStats stats = { 0, 0, "", "" };
		int endIdx = 0, startIdx = 0;

		if (klineObj && !klineObj->data.empty())
		{
			int dataSize = static_cast<int>(klineObj->data.size());
			// 1年: [dataSize-250, dataSize-1], 2年: [dataSize-500, dataSize-251], 3年: [dataSize-750, dataSize-501]
			endIdx = dataSize - (year - 1) * DAYS_PER_YEAR - 1;
			startIdx = max(0, dataSize - year * DAYS_PER_YEAR);

			if (startIdx <= endIdx && endIdx >= 0)
			{
				double maxPrice = 0;
				double minPrice = (std::numeric_limits<double>::max)();
				for (int i = startIdx; i <= endIdx && i < dataSize; i++)
				{
					if (klineObj->data[i].high > maxPrice)
					{
						maxPrice = klineObj->data[i].high;
						stats.maxDate = klineObj->data[i].day;
					}
					if (klineObj->data[i].low < minPrice)
					{
						minPrice = klineObj->data[i].low;
						stats.minDate = klineObj->data[i].day;
					}
				}
				stats.maxPrice = maxPrice;
				stats.minPrice = (minPrice == (std::numeric_limits<double>::max)()) ? 0 : minPrice;
			}
		}
		else if (!klineData.empty())
		{
			int dataSize = static_cast<int>(klineData.size());
			endIdx = dataSize - (year - 1) * DAYS_PER_YEAR - 1;
			startIdx = max(0, dataSize - year * DAYS_PER_YEAR);

			if (startIdx <= endIdx && endIdx >= 0)
			{
				double maxPrice = 0;
				double minPrice = (std::numeric_limits<double>::max)();
				for (int i = startIdx; i <= endIdx && i < dataSize; i++)
				{
					if (klineData[i].high > maxPrice)
					{
						maxPrice = klineData[i].high;
						stats.maxDate = klineData[i].day;
					}
					if (klineData[i].low < minPrice)
					{
						minPrice = klineData[i].low;
						stats.minDate = klineData[i].day;
					}
				}
				stats.maxPrice = maxPrice;
				stats.minPrice = (minPrice == (std::numeric_limits<double>::max)()) ? 0 : minPrice;
			}
		}
		periodStatsMap[year] = stats;
	}

	// 计算每个周期的平均价格
	auto getAvgPrice = [&](int days) -> double {
		if (klineObj)
			return klineObj->CalculateMAPeriod(days, 1);
		else if (!klineData.empty())
		{
			int startIdx = max(0, static_cast<int>(klineData.size()) - days * DAYS_PER_YEAR);
			int endIdx = static_cast<int>(klineData.size()) - 1;
			double sumPrice = 0;
			int count = 0;
			for (int i = startIdx; i <= endIdx; i++) { sumPrice += klineData[i].close; count++; }
			return count > 0 ? sumPrice / count : 0;
		}
		return 0;
		};

	// 收集所有要绘制的项目（文本行或分隔线），用于精确计算行高
	enum ItemType { TEXT_ROW, SEPARATOR };
	struct DrawItem {
		ItemType type;
		CString text;
		COLORREF color;
	};

	std::vector<DrawItem> items;

	// 基本信息区（4行）
	CString buyDateStr(buyDate.c_str());
	buyDateStr.Replace(_T("-"), _T("/"));
	items.push_back({ TEXT_ROW, !buyDate.empty() ? CString(_T("买入:")) + buyDateStr : CString(_T("买入:--")), COLOR_BLACK });

	if (!buyDate.empty())
	{
		int year = 0, month = 0, day = 0;
		if (swscanf_s(buyDate.c_str(), L"%d-%d-%d", &year, &month, &day) == 3)
		{
			std::tm buyTm = { 0 };
			buyTm.tm_year = year - 1900;
			buyTm.tm_mon = month - 1;
			buyTm.tm_mday = day;
			std::time_t buyTime = std::mktime(&buyTm);
			std::time_t now = std::time(nullptr);
			double diffDays = std::difftime(now, buyTime) / (60 * 60 * 24);
			CString holdingTimeStr;
			if (diffDays < 365)
				holdingTimeStr.Format(_T("持有:%d天"), static_cast<int>(diffDays));
			else
			{
				int years = static_cast<int>(diffDays / 365);
				int days = static_cast<int>(diffDays) % 365;
				holdingTimeStr.Format(_T("持有:%d年%d天"), years, days);
			}
			items.push_back({ TEXT_ROW, holdingTimeStr, COLOR_BLACK });
		}
		else
			items.push_back({ TEXT_ROW, _T("持有:--"), COLOR_BLACK });
	}
	else
		items.push_back({ TEXT_ROW, _T("持有:--"), COLOR_BLACK });

	if (costPrice > 0 && holdingCount > 0 && stockInfo.currentPrice > 0)
	{
		STOCK::StockData::PositionInfo posInfo = {};
		auto stockDataPtr2 = g_data.GetStockData(m_stock_id);
		if (stockDataPtr2)
			posInfo = stockDataPtr2->CalculatePositionInfo(costPrice, holdingCount);
		COLORREF profitLossColor = posInfo.profitLoss >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CString profitLossStr;
		if (posInfo.profitLossPercent >= 0)
			profitLossStr.Format(_T("盈亏:+%.2f%%(+%g)"), posInfo.profitLossPercent, posInfo.profitLoss);
		else
			profitLossStr.Format(_T("盈亏:%.2f%%(%g)"), posInfo.profitLossPercent, posInfo.profitLoss);
		items.push_back({ TEXT_ROW, profitLossStr, profitLossColor });
	}
	else
		items.push_back({ TEXT_ROW, _T("盈亏:--"), COLOR_BLACK });

	if (costPrice > 0 && holdingCount > 0 && stockInfo.currentPrice > 0 && !buyDate.empty())
	{
		double annualizedReturn = 0;
		auto stockDataPtr2 = g_data.GetStockData(m_stock_id);
		if (stockDataPtr2)
			annualizedReturn = stockDataPtr2->CalculateAnnualizedReturn(costPrice, holdingCount, buyDate);
		COLORREF annualColor = annualizedReturn >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CString annualStr;
		if (annualizedReturn >= 0)
			annualStr.Format(_T("年化: +%.2f%%"), annualizedReturn);
		else
			annualStr.Format(_T("年化: %.2f%%"), annualizedReturn);
		items.push_back({ TEXT_ROW, annualStr, annualColor });
	}
	else
		items.push_back({ TEXT_ROW, _T("年化:--"), COLOR_BLACK });

	// 周期指标 - 按类型分组：最高价、最低价、平均价
	struct PeriodInfo { int days; COLORREF color; };
	std::vector<PeriodInfo> periods = { {1, COLOR_BLUE_AVG1}, {2, COLOR_GREEN_AVG2}, {3, COLOR_GREEN_AVG3} };

	// 最高价分组
	items.push_back({ SEPARATOR, _T(""), COLOR_GRAY_GRID });
	for (const auto& p : periods)
	{
		auto it = periodStatsMap.find(p.days);
		if (it == periodStatsMap.end()) continue;
		const PeriodStats& stats = it->second;
		if (!(stats.maxPrice > 0)) continue;

		CString maxTxt;
		if (stockInfo.currentPrice > 0)
		{
			double maxDiff = stats.maxPrice - stockInfo.currentPrice;
			double maxDiffPercent = (maxDiff / stats.maxPrice) * 100;
			if (maxDiff >= 0)
				maxTxt.Format(_T("[high%d]:%s +%.2f%%"), p.days, CCommon::FormatFloat(stats.maxPrice), maxDiffPercent);
			else
				maxTxt.Format(_T("[high%d]:%s %.2f%%"), p.days, CCommon::FormatFloat(stats.maxPrice), maxDiffPercent);
		}
		else
			maxTxt.Format(_T("[high%d]:%s"), p.days, CCommon::FormatFloat(stats.maxPrice));
		items.push_back({ TEXT_ROW, maxTxt, p.color });

		CString maxDateStr(stats.maxDate.c_str());
		maxDateStr.Replace(_T("-"), _T("/"));
		items.push_back({ TEXT_ROW, CString(_T("时间:")) + maxDateStr, p.color });
	}

	// 最低价分组
	items.push_back({ SEPARATOR, _T(""), COLOR_GRAY_GRID });
	for (const auto& p : periods)
	{
		auto it = periodStatsMap.find(p.days);
		if (it == periodStatsMap.end()) continue;
		const PeriodStats& stats = it->second;
		if (!(stats.minPrice > 0 && stats.minPrice < (std::numeric_limits<STOCK::Price>::max)() / 2)) continue;

		CString minTxt;
		if (stockInfo.currentPrice > 0)
		{
			double minDiff = stats.minPrice - stockInfo.currentPrice;
			double minDiffPercent = (minDiff / stats.minPrice) * 100;
			if (minDiff >= 0)
				minTxt.Format(_T("[low%d]:%s +%.2f%%"), p.days, CCommon::FormatFloat(stats.minPrice), minDiffPercent);
			else
				minTxt.Format(_T("[low%d]:%s %.2f%%"), p.days, CCommon::FormatFloat(stats.minPrice), minDiffPercent);
		}
		else
			minTxt.Format(_T("[low%d]:%s"), p.days, CCommon::FormatFloat(stats.minPrice));
		items.push_back({ TEXT_ROW, minTxt, p.color });

		CString minDateStr(stats.minDate.c_str());
		minDateStr.Replace(_T("-"), _T("/"));
		items.push_back({ TEXT_ROW, CString(_T("时间:")) + minDateStr, p.color });
	}

	// 平均价分组
	items.push_back({ SEPARATOR, _T(""), COLOR_GRAY_GRID });
	for (const auto& p : periods)
	{
		double avgPrice = getAvgPrice(p.days);
		CString avgTxt;
		if (stockInfo.currentPrice > 0)
		{
			double avgDiff = avgPrice - stockInfo.currentPrice;
			double avgDiffPercent = (avgDiff / avgPrice) * 100;
			if (avgDiff >= 0)
				avgTxt.Format(_T("[avg%d]:%s +%.2f%%"), p.days, CCommon::FormatFloat(avgPrice), avgDiffPercent);
			else
				avgTxt.Format(_T("[avg%d]:%s %.2f%%"), p.days, CCommon::FormatFloat(avgPrice), avgDiffPercent);
		}
		else
			avgTxt.Format(_T("[avg%d]:%s"), p.days, CCommon::FormatFloat(avgPrice));
		items.push_back({ TEXT_ROW, avgTxt, p.color });
	}

	// 计算行高：以字体高度为最小值，确保文字不被截断
	int separatorCount = 0;
	int textCount = 0;
	for (const auto& item : items) {
		if (item.type == SEPARATOR) separatorCount++;
		else textCount++;
	}
	TEXTMETRIC tm;
	memDC.GetTextMetrics(&tm);
	int fontHeight = tm.tmHeight;
	int separatorHeight = g_data.RDPI(4);
	int totalSeparatorHeight = separatorCount * separatorHeight;
	// 均分可用高度，但每行不低于字体高度
	int textRowHeight = textCount > 0 ? (availableHeight - totalSeparatorHeight) / textCount : fontHeight;
	textRowHeight = max(textRowHeight, fontHeight);

	// 绘制所有项目
	int currentY = topY;
	CPen pDashLine(PS_DASH, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pDashLine);

	for (const auto& item : items)
	{
		if (item.type == SEPARATOR)
		{
			memDC.MoveTo(left, currentY);
			memDC.LineTo(right, currentY);
			currentY += separatorHeight;
		}
		else
		{
			memDC.SetTextColor(item.color);
			memDC.TextOut(textX, currentY, item.text);
			currentY += textRowHeight;
		}
	}

	memDC.SelectObject(pOldPen);
}

// ========== DrawIndexSection ==========
// 绘制大盘指数区域：每个指数占一列，显示名称(黑色)、当前价格(红涨绿跌)、涨跌额和涨跌幅(红涨绿跌)
void CFloatingWnd::DrawIndexSection(CDC& memDC, int x, int y, int w, const std::vector<std::pair<std::wstring, STOCK::StockInfo>>& indices)
{
	if (indices.empty())
		return;

	const int indexCount = (int)indices.size();
	const int colWidth = w / indexCount;
	const int sectionHeight = g_data.RDPI(56);

	// 背景
	memDC.FillSolidRect(x, y, w, sectionHeight, RGB(245, 245, 245));

	memDC.SetBkMode(TRANSPARENT);

	// 创建大号字体用于显示价格
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = g_data.RDPI(22);
	lf.lfWeight = FW_BOLD;
	wcscpy_s(lf.lfFaceName, _T("Microsoft YaHei"));
	CFont largeFont;
	largeFont.CreateFontIndirect(&lf);

	// 获取两种字体的高度，用于均匀分配垂直空间
	TEXTMETRIC tmLarge, tmNormal;
	CFont* pOldFont = memDC.SelectObject(&largeFont);
	memDC.GetTextMetrics(&tmLarge);
	int largeFontHeight = tmLarge.tmHeight;

	memDC.SelectObject(pOldFont);
	memDC.GetTextMetrics(&tmNormal);
	int normalFontHeight = tmNormal.tmHeight;

	for (int i = 0; i < indexCount; i++)
	{
		const auto& code = indices[i].first;
		const auto& info = indices[i].second;

		int colX = x + i * colWidth;
		int centerX = colX + colWidth / 2;

		double displayPrice = info.currentPrice > 0 ? info.currentPrice : info.prevClosePrice;
		double diff = displayPrice - info.prevClosePrice;
		double diffPercent = info.prevClosePrice != 0 ? (diff / info.prevClosePrice) * 100 : 0;

		COLORREF priceColor = CCommon::GetProfitLossColor(diffPercent);

		// 均匀分配垂直空间：名称、价格、涨跌幅三者等间距排列
		int totalTextHeight = normalFontHeight + largeFontHeight + normalFontHeight;
		int gap = (sectionHeight - totalTextHeight) / 3;
		int nameY = y + gap;
		int priceY = nameY + normalFontHeight + gap;
		int changeY = priceY + largeFontHeight + gap;

		// 名称（黑色，普通字体）
		CString name = info.GetStockListName();
		memDC.SelectObject(pOldFont);
		CSize nameSz = memDC.GetTextExtent(name);
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(centerX - nameSz.cx / 2, nameY, name);

		// 当前价格（红涨绿跌，大号字体）
		memDC.SelectObject(&largeFont);
		CString priceStr;
		priceStr.Format(_T("%.2f"), displayPrice);
		CSize priceSz = memDC.GetTextExtent(priceStr);
		memDC.SetTextColor(priceColor);
		memDC.TextOut(centerX - priceSz.cx / 2, priceY, priceStr);

		// 涨跌额和涨跌幅（红涨绿跌，普通字体）
		memDC.SelectObject(pOldFont);
		CString changeStr;
		if (diff >= 0)
			changeStr.Format(_T("+%.2f  +%.2f%%"), diff, diffPercent);
		else
			changeStr.Format(_T("%.2f  %.2f%%"), diff, diffPercent);

		CSize changeSz = memDC.GetTextExtent(changeStr);
		memDC.SetTextColor(priceColor);
		memDC.TextOut(centerX - changeSz.cx / 2, changeY, changeStr);
	}

	memDC.SelectObject(pOldFont);
}

void CFloatingWnd::DrawOverviewTable(CDC& memDC, int x, int y, int w, int h, int vScrollOffset, int totalHeight)
{
	const int headerHeight = g_data.RDPI(26);
	const int colCount = 12;
	const CString headers[] = {
		_T("名称"), _T("昨收"), _T("现价"), _T("涨额"), _T("涨幅"),
		_T("成本"), _T("持股"), _T("仓值"), _T("市值"), _T("收益"), _T("收益率"), _T("操作")
	};

	// 绘制表头背景
	memDC.FillSolidRect(x, y, w, headerHeight, RGB(230, 230, 230));

	// 计算列宽
	int colWidths[12] = { 0 };
	colWidths[11] = g_data.RDPI(40);
	int totalWidth = colWidths[11];

	for (int i = 0; i < colCount - 1; i++)
	{
		CSize sz = memDC.GetTextExtent(headers[i]);
		colWidths[i] = sz.cx + g_data.RDPI(10);
		totalWidth += colWidths[i];
	}

	// 按内容调整列宽
	struct StockRowData {
		std::wstring code;  // 股票代码
		CString name;
		CString prevClose;
		CString current;
		CString changeAmount;
		CString changePercent;
		CString cost;
		CString holding;
		CString totalCost;
		CString marketValue;
		CString profitLoss;
		CString profitLossPercent;
		COLORREF changeColor;
		COLORREF profitColor;
		bool isIndex;  // 标记是否是指数
		double changePercentValue;  // 涨幅百分比数值
		double profitLossPercentValue;  // 盈亏百分比数值
	};

	std::vector<StockRowData> rows;
	auto stockCodes = g_data.m_setting_data.m_stock_codes;  // 拷贝一份用于排序

	// 对股票列表进行排序，优先展示大盘指数
	std::sort(stockCodes.begin(), stockCodes.end(), CompareStockPriority);

	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);

		for (const auto& code : stockCodes)
		{
			auto stockData = g_data.GetStockData(code);
			if (!stockData)
				continue;

			const auto& info = stockData->info;
			if (!info.is_ok)
				continue;

			// 跳过指数（已在顶部独立区域显示）
			if (GetStockPriority(code) < 200)
				continue;

			StockRowData row;
			row.code = code;  // 保存股票代码
			row.name = info.GetStockListName();
			row.isIndex = false;  // 表格中不再包含指数

			double costPrice = g_data.GetCostPrice(code);
			double holdingCount = g_data.GetHoldingCount(code);

			// 当现价为0时，使用昨收价作为显示价格
			double displayPrice = info.currentPrice > 0 ? info.currentPrice : info.prevClosePrice;

			// 大盘股（指数）价格格式化为整数显示
			if (row.isIndex)
			{
				row.prevClose.Format(_T("%.0f"), info.prevClosePrice);
				row.current.Format(_T("%.0f"), displayPrice);
			}
			else
			{
				row.prevClose = CCommon::FormatFloat(info.prevClosePrice);
				row.current = CCommon::FormatFloat(displayPrice);
			}

			double diff = displayPrice - info.prevClosePrice;
			double diffPercent = info.prevClosePrice != 0 ? (diff / info.prevClosePrice) * 100 : 0;
			row.changeColor = diff > 0 ? COLOR_RED_UP : (diff < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
			row.changePercentValue = diffPercent;  // 保存涨幅数值

			if (diff >= 0)
				row.changePercent.Format(_T("+%.2f%%"), diffPercent);
			else
				row.changePercent.Format(_T("%.2f%%"), diffPercent);

			// 大盘股（指数）涨额格式化为整数显示
			if (row.isIndex)
			{
				if (diff >= 0)
					row.changeAmount.Format(_T("+%.0f"), diff);
				else
					row.changeAmount.Format(_T("%.0f"), diff);
			}
			else
			{
				if (diff >= 0)
					row.changeAmount.Format(_T("+%s"), CCommon::FormatFloat(diff));
				else
					row.changeAmount.Format(_T("%s"), CCommon::FormatFloat(diff));
			}

			if (costPrice > 0)
				row.cost = CCommon::FormatFloat(costPrice);
			else
				row.cost = _T("--");

			if (holdingCount > 0)
				row.holding = CCommon::FormatVolumeInt(holdingCount);
			else
				row.holding = _T("--");

			if (costPrice > 0 && holdingCount > 0)
				row.totalCost = CCommon::FormatAmount(costPrice * holdingCount);
			else
				row.totalCost = _T("--");

			if (holdingCount > 0 && displayPrice > 0)
				row.marketValue = CCommon::FormatAmount(displayPrice * holdingCount);
			else
				row.marketValue = _T("--");

			if (costPrice > 0 && holdingCount > 0 && displayPrice > 0)
			{
				double totalCost = costPrice * holdingCount;
				double marketValue = displayPrice * holdingCount;
				double profitLoss = marketValue - totalCost;
				double profitLossPercent = totalCost != 0 ? (profitLoss / totalCost) * 100 : 0;
				row.profitColor = profitLoss > 0 ? COLOR_RED_UP : (profitLoss < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
				row.profitLossPercentValue = profitLossPercent;  // 保存盈亏数值

				CString formattedAmount = CCommon::FormatAmount(abs(profitLoss));
				if (profitLoss >= 0)
					row.profitLoss = _T("+") + formattedAmount;
				else
					row.profitLoss = _T("-") + formattedAmount;

				if (profitLossPercent >= 0)
					row.profitLossPercent.Format(_T("+%.2f%%"), profitLossPercent);
				else
					row.profitLossPercent.Format(_T("%.2f%%"), profitLossPercent);
			}
			else
			{
				row.profitLoss = _T("--");
				row.profitLossPercent = _T("--");
				row.profitColor = COLOR_BLACK;
				row.profitLossPercentValue = 0;  // 默认0
			}

			rows.push_back(row);

			// 更新列宽
			CString allFields[] = { row.name, row.prevClose, row.current,
				row.changeAmount, row.changePercent, row.cost, row.holding,
				row.totalCost, row.marketValue, row.profitLoss, row.profitLossPercent };
			for (int i = 0; i < colCount - 1; i++)
			{
				CSize sz = memDC.GetTextExtent(allFields[i]);
				int needed = sz.cx + g_data.RDPI(10);
				if (needed > colWidths[i])
					colWidths[i] = needed;
			}
		}
	}

	totalWidth = 0;
	for (int i = 0; i < colCount; i++)
		totalWidth += colWidths[i];

	// 将剩余空间分配给各列，铺满窗口宽度
	if (totalWidth < w)
	{
		int extra = w - totalWidth;
		for (int i = 0; i < colCount - 1; i++)  // 操作列不参与分配
		{
			colWidths[i] += extra / (colCount - 1);
		}
		// 余数分配给第一列（名称列）
		colWidths[0] += extra % (colCount - 1);
		totalWidth = w;
	}

	// 计算总行数和总高度
	int totalRows = (int)rows.size();
	int totalTableH = headerHeight + totalRows * headerHeight;

	// 计算可滚动范围
	int maxScrollOffset = max(0, totalTableH - h);

	// 限制滚动偏移
	if (vScrollOffset < 0) vScrollOffset = 0;
	if (vScrollOffset > maxScrollOffset) vScrollOffset = maxScrollOffset;

	// 绘制表头
	int currentX = x;
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);

	// 计算列起始位置，用于后续绘制竖线
	std::vector<int> colStartX;
	colStartX.reserve(colCount);
	for (int i = 0; i < colCount; i++)
	{
		colStartX.push_back(currentX);
		CSize sz = memDC.GetTextExtent(headers[i]);
		int txtX = currentX + (colWidths[i] - sz.cx) / 2;
		memDC.TextOut(txtX, y + g_data.RDPI(3), headers[i]);
		currentX += colWidths[i];
	}

	// 绘制数据行（裁剪到表头以下区域，防止滚动时覆盖表头）
	CRect dataClipRect(x, y + headerHeight, x + w, y + h);
	int savedDC = memDC.SaveDC();
	memDC.IntersectClipRect(&dataClipRect);

	int rowY = y + headerHeight - vScrollOffset;
	int rowIndex = 0;

	// 记录行信息用于双击处理
	m_overviewRows.clear();
	m_overviewRows.reserve(rows.size());

	for (const auto& row : rows)
	{
		// 行高
		int rowH = headerHeight;

		// 计算删除按钮位置
		int deleteBtnStartX = totalWidth - colWidths[colCount - 1];
		int deleteBtnEndX = totalWidth;

		// 保存行信息（考虑滚动偏移）
		OverviewRowInfo info;
		info.code = row.code;
		info.rowY = rowY + vScrollOffset;
		info.rowH = rowH;
		info.nameColWidth = colWidths[0];
		info.deleteBtnStartX = deleteBtnStartX;
		info.deleteBtnEndX = deleteBtnEndX;
		m_overviewRows.push_back(info);

		// 如果当前行超出可视区域，跳过绘制
		if (rowY + rowH < y || rowY >= y + h)
		{
			rowY += rowH;
			rowIndex++;
			continue;
		}

		// 交替行背景
		if (rowIndex % 2 == 1)
		{
			memDC.FillSolidRect(x, rowY, w, rowH, RGB(250, 250, 250));
		}

		currentX = x;
		CString fields[] = { row.name, row.prevClose, row.current,
			row.changeAmount, row.changePercent, row.cost, row.holding,
			row.totalCost, row.marketValue, row.profitLoss, row.profitLossPercent };

		for (int i = 0; i < colCount; i++)
		{
			COLORREF textColor = COLOR_BLACK;
			COLORREF bgColor = COLOR_WHITE;  // 默认白色背景

			if (i == colCount - 1) // 操作列（删除按钮）
			{
				// 绘制删除按钮背景
				CBrush btnBrush(RGB(230, 230, 230));
				memDC.FillRect(CRect(currentX + g_data.RDPI(4), rowY + g_data.RDPI(3),
					currentX + colWidths[i] - g_data.RDPI(4), rowY + rowH - g_data.RDPI(3)), &btnBrush);

				// 绘制删除文字
				CString delText = _T("删除");
				memDC.SetTextColor(COLOR_BLACK);
				CSize sz = memDC.GetTextExtent(delText);
				int txtX = currentX + (colWidths[i] - sz.cx) / 2;
				memDC.TextOut(txtX, rowY + g_data.RDPI(3), delText);
				currentX += colWidths[i];
				continue;
			}

			if (i == 3) // 涨额列
			{
				textColor = row.changeColor;
			}
			else if (i == 4) // 涨幅列
			{
				textColor = row.changeColor;
				bgColor = GetCellBgColor(row.changePercentValue);
			}
			else if (i == 9) // 盈亏列
			{
				textColor = row.profitColor;
			}
			else if (i == 10) // 盈比列
			{
				textColor = row.profitColor;
				bgColor = GetCellBgColor(row.profitLossPercentValue);
			}

			// 如果需要特殊背景色，先绘制背景
			if (bgColor != COLOR_WHITE)
			{
				CBrush bgBrush(bgColor);
				int paddingX = (i == 0) ? g_data.RDPI(3) : 0;
				memDC.FillRect(CRect(currentX + paddingX, rowY, currentX + colWidths[i] - paddingX, rowY + rowH), &bgBrush);
				textColor = COLOR_WHITE;  // 有背景色时使用白色文字
			}

			memDC.SetTextColor(textColor);

			CSize sz = memDC.GetTextExtent(fields[i]);
			int txtX;
			if (i == 0) // 名称列左对齐
				txtX = currentX + g_data.RDPI(3);
			else // 其余数字列右对齐
				txtX = currentX + colWidths[i] - sz.cx - g_data.RDPI(3);

			memDC.TextOut(txtX, rowY + g_data.RDPI(3), fields[i]);
			currentX += colWidths[i];
		}

		rowY += rowH;
		rowIndex++;
	}

	// 恢复DC（取消裁剪区域）
	memDC.RestoreDC(savedDC);

	// 绘制汇总行：总成本、总市值、总盈亏、收益率（在横线之前绘制，避免覆盖横线）
	double sumCost = 0, sumMarket = 0;
	for (const auto& row : rows)
	{
		if (row.isIndex) continue;
		auto stockData = g_data.GetStockData(row.code);
		if (!stockData) continue;
		double costPrice = g_data.GetCostPrice(row.code);
		double holdingCount = g_data.GetHoldingCount(row.code);
		double displayPrice = stockData->info.currentPrice > 0 ? stockData->info.currentPrice : stockData->info.prevClosePrice;
		if (costPrice > 0 && holdingCount > 0)
		{
			sumCost += costPrice * holdingCount;
			if (displayPrice > 0)
				sumMarket += displayPrice * holdingCount;
		}
	}

	if (sumCost > 0 || sumMarket > 0)
	{
		double sumProfit = sumMarket - sumCost;
		double profitRate = sumCost > 0 ? (sumProfit / sumCost) * 100 : 0;

		CString costStr = CCommon::FormatAmount(sumCost);
		CString marketStr = CCommon::FormatAmount(sumMarket);
		CString profitStr = CCommon::FormatAmount(abs(sumProfit));
		if (sumProfit >= 0)
			profitStr = _T("+") + profitStr;
		else
			profitStr = _T("-") + profitStr;

		CString rateStr;
		if (profitRate >= 0)
			rateStr.Format(_T("+%.2f%%"), profitRate);
		else
			rateStr.Format(_T("%.2f%%"), profitRate);

		COLORREF rateColor = CCommon::GetProfitLossColor(profitRate);

		// 总盈亏颜色
		COLORREF profitColor = sumProfit > 0 ? COLOR_RED_UP : (sumProfit < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);

		// 分段计算总宽度以实现居中
		struct TextSeg { CString text; COLORREF color; };
		TextSeg segs[] = {
			{ _T("总成本: "), COLOR_BLACK }, { costStr, COLOR_BLACK },
			{ _T("  总市值: "), COLOR_BLACK }, { marketStr, COLOR_BLACK },
			{ _T("  总收益: "), COLOR_BLACK }, { profitStr, profitColor },
			{ _T("  收益率: "), COLOR_BLACK }, { rateStr, rateColor }
		};
		int totalWidth = 0;
		for (const auto& seg : segs)
			totalWidth += memDC.GetTextExtent(seg.text).cx;

		// 绘制到对话框最底部作为状态栏（使用 totalHeight 确保紧贴底部）
		CSize textSize = memDC.GetTextExtent(_T("Ay"));
		const int statusBarHeight = textSize.cy + g_data.RDPI(6);
		int summaryY = totalHeight - statusBarHeight;
		memDC.FillSolidRect(x, summaryY, w, statusBarHeight, RGB(240, 240, 240));
		memDC.SetBkMode(TRANSPARENT);
		int drawX = x + max(0, (w - totalWidth) / 2);
		int textY = summaryY + g_data.RDPI(3);
		for (const auto& seg : segs)
		{
			memDC.SetTextColor(seg.color);
			memDC.TextOut(drawX, textY, seg.text);
			drawX += memDC.GetTextExtent(seg.text).cx;
		}
	}

	// 在所有背景和文本绘制完成后，统一画横线和竖线（避免被FillSolidRect覆盖）
	memDC.SelectObject(&gridPen);

	// 画横线（从表头顶部开始，按headerHeight间隔绘制）
	for (int lineY = y; lineY <= y + h; lineY += headerHeight)
	{
		memDC.MoveTo(x, lineY);
		memDC.LineTo(x + w, lineY);
	}

	// 画竖线（画到表格区域底部，与横线对齐）
	for (int i = 1; i <= colCount; i++)
	{
		int lineX = colStartX[0];
		for (int j = 1; j <= i; j++)
			lineX += colWidths[j - 1];
		memDC.MoveTo(lineX, y);
		memDC.LineTo(lineX, y + h);
	}
}

void CFloatingWnd::DrawOrderBook(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo, const std::vector<STOCK::KLinePoint>& klineData)
{
	const int MAX_LEVEL = STOCK::StockInfo::MAX_LEVEL;
	// 布局：0=最高, 1=卖三, 2=卖二, 3=卖一, 4=买一, 5=买二, 6=买三, 7=最低, 8=趋势, 9=净比00, 10-13=净比01/05/10/20, 14=净比99, 15=振幅, 16=换手率, 17=委比
	const int totalRows = 18;
	const int headerHeight = g_data.RDPI(26);  // 主标题栏高度
	const int obTitleH = g_data.RDPI(16);       // 盘口标题栏高度，与走势图标题栏一致
	const int topOffset = headerHeight + obTitleH;  // 内容从主标题栏+盘口标题栏下方开始
	const int panelW = right - left;
	// 绘制盘口标题栏背景（在主标题栏下方）
	memDC.FillSolidRect(left, headerHeight, panelW, obTitleH, RGB(245, 245, 245));
	const int rowHeight = (height - obTitleH) / totalRows;  // 每行基础高度
	const int contentH = height - obTitleH;  // 内容区域总高度
	const int rem = contentH % totalRows;    // 余数：前rem行多1px
	// 辅助：计算第i行(0-based)的Y坐标
	auto rowY = [&](int i) -> int {
		if (i < rem) return topOffset + i * (rowHeight + 1);
		else return topOffset + rem * (rowHeight + 1) + (i - rem) * rowHeight;
		};
	// 辅助：计算第i行的高度
	auto rowH = [&](int i) -> int {
		return (i < rem) ? (rowHeight + 1) : rowHeight;
		};
	// 填充内容区域背景，避免底部空白
	memDC.FillSolidRect(left, topOffset, panelW, contentH, RGB(250, 250, 250));

	memDC.SetBkMode(TRANSPARENT);

	STOCK::Volume innerVol = stockInfo.innerVolume / 100;
	STOCK::Volume outerVol = stockInfo.outerVolume / 100;

	int textX = left + g_data.RDPI(5) + 3;

	// 更新买一/卖一挂盘数量变化方向
	auto stockDataForAccum = g_data.GetStockData(m_stock_id);
	auto GetOrderPriceAccumLots = [&](STOCK::Price price) -> STOCK::Volume {
		if (!stockDataForAccum || price <= 0)
			return 0;
		auto it = stockDataForAccum->orderPriceAccumMap.find(price);
		if (it == stockDataForAccum->orderPriceAccumMap.end())
			return 0;
		return it->second.accumSellVolume / 100;
		};
	CString ask1VolumeTrend, bid1VolumeTrend;
	{
		static std::map<std::wstring, STOCK::Volume> lastAsk1VolumeMap;
		static std::map<std::wstring, STOCK::Volume> lastBid1VolumeMap;
		static std::map<std::wstring, CString> lastAsk1VolumeTrendMap;
		static std::map<std::wstring, CString> lastBid1VolumeTrendMap;
		STOCK::Volume curAsk1Volume = stockInfo.askLevels[0].volume;
		STOCK::Volume curBid1Volume = stockInfo.bidLevels[0].volume;

		auto lastAsk1VolumeIt = lastAsk1VolumeMap.find(m_stock_id);
		if (lastAsk1VolumeIt != lastAsk1VolumeMap.end())
		{
			if (curAsk1Volume > lastAsk1VolumeIt->second)
				ask1VolumeTrend = _T("↑");
			else if (curAsk1Volume < lastAsk1VolumeIt->second)
				ask1VolumeTrend = _T("↓");
			else
			{
				auto lastTrendIt = lastAsk1VolumeTrendMap.find(m_stock_id);
				if (lastTrendIt != lastAsk1VolumeTrendMap.end())
					ask1VolumeTrend = lastTrendIt->second;
			}
		}
		lastAsk1VolumeMap[m_stock_id] = curAsk1Volume;
		if (!ask1VolumeTrend.IsEmpty())
			lastAsk1VolumeTrendMap[m_stock_id] = ask1VolumeTrend;

		auto lastBid1VolumeIt = lastBid1VolumeMap.find(m_stock_id);
		if (lastBid1VolumeIt != lastBid1VolumeMap.end())
		{
			if (curBid1Volume > lastBid1VolumeIt->second)
				bid1VolumeTrend = _T("↑");
			else if (curBid1Volume < lastBid1VolumeIt->second)
				bid1VolumeTrend = _T("↓");
			else
			{
				auto lastTrendIt = lastBid1VolumeTrendMap.find(m_stock_id);
				if (lastTrendIt != lastBid1VolumeTrendMap.end())
					bid1VolumeTrend = lastTrendIt->second;
			}
		}
		lastBid1VolumeMap[m_stock_id] = curBid1Volume;
		if (!bid1VolumeTrend.IsEmpty())
			lastBid1VolumeTrendMap[m_stock_id] = bid1VolumeTrend;
	}
	struct OrderBookRow
	{
		STOCK::Price price;
		CString text;
		CString smallSuffix;
		CString rightAlignSuffix;  // 右对齐的累加成交量
		COLORREF textColor;
		bool fillBackground{ false };
		COLORREF backgroundColor;
		bool drawSmallSuffix{ false };
		bool darkBackground{ false };  // 深色背景时文字改白色
	};

	std::vector<OrderBookRow> priceRows;
	priceRows.reserve(8);

	// 最高价（固定在卖三上面）
	{
		CString highTxt;
		double highDiff = stockInfo.highPrice - stockInfo.currentPrice;
		if (highDiff >= 0)
			highTxt.Format(_T("最高: %s +%s"), CCommon::FormatFloat(stockInfo.highPrice), CCommon::FormatFloat(highDiff));
		else
			highTxt.Format(_T("最高: %s %s"), CCommon::FormatFloat(stockInfo.highPrice), CCommon::FormatFloat(highDiff));

		OrderBookRow row;
		row.price = stockInfo.highPrice;
		row.text = highTxt;
		row.textColor = RGB(128, 0, 128);
		row.fillBackground = true;
		row.backgroundColor = RGB(220, 235, 250);
		priceRows.push_back(row);
	}

	// 卖三~卖一
	for (int idx = 2; idx >= 0; --idx)
	{
		STOCK::Price price = stockInfo.askLevels[idx].price;
		STOCK::Volume volume = stockInfo.askLevels[idx].volume / 100;
		CString volumeStr = CCommon::FormatVolumeInt(volume);
		CString priceStr = stockInfo.IsETF() ? CCommon::FormatETFPrice(price) : CCommon::FormatFloat(price);
		CString askTxt;
		askTxt.Format(_T("S%d:%s"), idx + 1, priceStr);
		CString askSuffix;
		askSuffix.Format(_T(" %s"), volumeStr.GetString());
		STOCK::Volume accum = GetOrderPriceAccumLots(price);
		CString accumStr;
		if (accum > 0)
		{
			accumStr = CCommon::FormatVolumeInt(accum);
			if (idx == 0 && !ask1VolumeTrend.IsEmpty())
				askSuffix.AppendFormat(_T("%s"), ask1VolumeTrend.GetString());
		}

		OrderBookRow row;
		row.price = price;
		row.text = askTxt;
		row.smallSuffix = askSuffix;
		row.rightAlignSuffix = accumStr;
		row.drawSmallSuffix = true;
		row.textColor = COLOR_RED_UP;
		// 卖一背景色按3档强度区分（仅当前价格=卖一时显示）
		if (idx == 0 && stockInfo.currentPrice > 0 && price > 0 && stockInfo.currentPrice == price)
		{
			row.fillBackground = true;
			if (volume > accum)
				row.backgroundColor = RGB(255, 200, 200);   // 弱：挂单量>累加量
			else if (accum > volume * 2)
			{
				row.backgroundColor = RGB(180, 50, 50);     // 强：累加量/挂单量>2
				row.darkBackground = true;
			}
			else
			{
				row.backgroundColor = RGB(240, 40, 40);     // 中：1~2之间
				row.darkBackground = true;
			}
		}
		else
		{
			row.fillBackground = (stockInfo.currentPrice > 0 && price > 0 && stockInfo.currentPrice == price);
			row.backgroundColor = RGB(255, 200, 200);
		}
		priceRows.push_back(row);
	}

	// 买一~买三
	for (int i = 0; i < 3; i++)
	{
		STOCK::Price price = stockInfo.bidLevels[i].price;
		STOCK::Volume volume = stockInfo.bidLevels[i].volume / 100;
		CString volumeStr = CCommon::FormatVolumeInt(volume);
		CString priceStr = stockInfo.IsETF() ? CCommon::FormatETFPrice(price) : CCommon::FormatFloat(price);
		CString bidTxt;
		bidTxt.Format(_T("B%d:%s"), i + 1, priceStr);
		CString bidSuffix;
		bidSuffix.Format(_T(" %s"), volumeStr.GetString());
		STOCK::Volume accum = GetOrderPriceAccumLots(price);
		CString accumStr;
		if (accum > 0)
		{
			accumStr = CCommon::FormatVolumeInt(accum);
			if (i == 0 && !bid1VolumeTrend.IsEmpty())
				bidSuffix.AppendFormat(_T("%s"), bid1VolumeTrend.GetString());
		}

		OrderBookRow row;
		row.price = price;
		row.text = bidTxt;
		row.smallSuffix = bidSuffix;
		row.rightAlignSuffix = accumStr;
		row.drawSmallSuffix = true;
		row.textColor = COLOR_GREEN_DOWN;
		// 买一背景色按3档强度区分（仅当前价格=买一时显示）
		if (i == 0 && stockInfo.currentPrice > 0 && price > 0 && stockInfo.currentPrice == price)
		{
			row.fillBackground = true;
			if (volume > accum)
				row.backgroundColor = RGB(200, 255, 200);   // 弱：挂单量>累加量
			else if (accum > volume * 2)
			{
				row.backgroundColor = RGB(25, 120, 25);     // 强：累加量/挂单量>2
				row.darkBackground = true;
			}
			else
			{
				row.backgroundColor = RGB(50, 180, 50);     // 中：1~2之间
				row.darkBackground = true;
			}
		}
		else
		{
			row.fillBackground = (stockInfo.currentPrice > 0 && price > 0 && stockInfo.currentPrice == price);
			row.backgroundColor = RGB(200, 255, 200);
		}
		priceRows.push_back(row);
	}

	// 最低价（固定在买三下面）
	{
		CString lowTxt;
		double lowDiff = stockInfo.lowPrice - stockInfo.currentPrice;
		if (lowDiff >= 0)
			lowTxt.Format(_T("最低: %s +%s"), CCommon::FormatFloat(stockInfo.lowPrice), CCommon::FormatFloat(lowDiff));
		else
			lowTxt.Format(_T("最低: %s %s"), CCommon::FormatFloat(stockInfo.lowPrice), CCommon::FormatFloat(lowDiff));

		OrderBookRow row;
		row.price = stockInfo.lowPrice;
		row.text = lowTxt;
		row.textColor = RGB(0, 100, 0);
		row.fillBackground = true;
		row.backgroundColor = RGB(220, 235, 250);
		priceRows.push_back(row);
	}

	auto DrawOrderBookRowText = [&](const OrderBookRow& row, int x, int y, int rowWidth) {
		COLORREF textColor = row.darkBackground ? RGB(255, 255, 255) : row.textColor;
		memDC.SetTextColor(textColor);
		memDC.TextOut(x, y, row.text);
		if (!row.drawSmallSuffix || row.smallSuffix.IsEmpty())
		{
			// 只有右对齐后缀
			if (!row.rightAlignSuffix.IsEmpty())
			{
				CFont* oldFont = memDC.GetCurrentFont();
				LOGFONT lf;
				oldFont->GetLogFont(&lf);
				lf.lfHeight = lf.lfHeight * 7 / 8;
				CFont smallFont;
				smallFont.CreateFontIndirect(&lf);
				memDC.SelectObject(&smallFont);
				memDC.SetTextColor(row.darkBackground ? RGB(255, 255, 200) : textColor);
				int suffixW = memDC.GetTextExtent(row.rightAlignSuffix).cx;
				memDC.TextOut(x + rowWidth - suffixW - g_data.RDPI(4), y + g_data.RDPI(1), row.rightAlignSuffix);
				memDC.SelectObject(oldFont);
			}
			return;
		}

		int suffixX = x + memDC.GetTextExtent(row.text).cx;
		CFont* oldFont = memDC.GetCurrentFont();
		LOGFONT lf;
		oldFont->GetLogFont(&lf);
		lf.lfHeight = lf.lfHeight * 7 / 8;
		CFont smallFont;
		smallFont.CreateFontIndirect(&lf);
		memDC.SelectObject(&smallFont);
		memDC.SetTextColor(row.darkBackground ? RGB(255, 255, 200) : textColor);
		memDC.TextOut(suffixX, y + g_data.RDPI(1), row.smallSuffix);
		// 右对齐绘制累加成交量
		if (!row.rightAlignSuffix.IsEmpty())
		{
			int raSuffixW = memDC.GetTextExtent(row.rightAlignSuffix).cx;
			memDC.TextOut(x + rowWidth - raSuffixW - g_data.RDPI(4), y + g_data.RDPI(1), row.rightAlignSuffix);
		}
		memDC.SelectObject(oldFont);
		};

	for (int i = 0; i < static_cast<int>(priceRows.size()); i++)
	{
		int y = rowY(i);
		int h = rowH(i);
		if (priceRows[i].fillBackground)
			memDC.FillSolidRect(left, y, right - left, h, priceRows[i].backgroundColor);
		int textVCenter = max(0, (h - memDC.GetTextExtent(priceRows[i].text).cy) / 2);
		DrawOrderBookRowText(priceRows[i], textX, y + textVCenter, right - textX);
	}

	static const COLORREF NET_RATIO_RED_COLORS[] = {
		RGB(240, 40, 40),   // 0-30
		RGB(180, 50, 50),   // 30-60
		RGB(130, 20, 40)    // 60以上
	};
	static const COLORREF NET_RATIO_GREEN_COLORS[] = {
		RGB(40, 240, 40),  // 0~30 浅亮绿（弱多）
		RGB(50, 180, 50),  // 30~60 中草绿（中多）
		RGB(20, 130, 40)   // 60以上 深墨绿（强多）
	};
	auto GetNetRatioColorIndex = [](double ratio) -> int {
		double absRatioValue = std::abs(ratio);
		if (absRatioValue <= 30) return 0;
		if (absRatioValue <= 60) return 1;
		return 2;
		};

	auto DrawNetRatioBarText = [&](int x, int y, int w, int h, const CString& ratioText, const CString& diffText) {
		CFont* oldFont = memDC.GetCurrentFont();
		LOGFONT lf;
		oldFont->GetLogFont(&lf);
		lf.lfHeight = lf.lfHeight * 27 / 32;
		CFont smallFont;
		smallFont.CreateFontIndirect(&lf);
		memDC.SelectObject(&smallFont);
		memDC.SetTextColor(RGB(255, 255, 255));
		int vCenter = max(0, (h - memDC.GetTextExtent(ratioText).cy) / 2);
		memDC.TextOut(x + g_data.RDPI(3), y + vCenter, ratioText);
		CSize diffSize = memDC.GetTextExtent(diffText);
		memDC.TextOut(x + w - diffSize.cx - g_data.RDPI(3), y + vCenter, diffText);
		memDC.SelectObject(oldFont);
		};

	auto DrawRatioBar = [&](int x, int y, int w, int h, double ratio, STOCK::Volume diff) {
		if (w <= 0)
			return;
		COLORREF redColor = NET_RATIO_RED_COLORS[GetNetRatioColorIndex(ratio)];
		COLORREF greenColor = NET_RATIO_GREEN_COLORS[GetNetRatioColorIndex(ratio)];
		int midX = x + w / 2;
		int halfW = w / 2;
		int fillW = static_cast<int>(std::sqrt(std::abs(ratio) / 100.0) * halfW);
		fillW = min(fillW, halfW);
		memDC.FillSolidRect(x, y, w, h, RGB(230, 230, 230));
		int dominantW = min(w, halfW + fillW);
		if (diff > 0)
		{
			memDC.FillSolidRect(x, y, dominantW, h, redColor);
			memDC.FillSolidRect(x + dominantW, y, w - dominantW, h, greenColor);
		}
		else if (diff < 0)
		{
			memDC.FillSolidRect(x, y, dominantW, h, greenColor);
			memDC.FillSolidRect(x + dominantW, y, w - dominantW, h, redColor);
		}
		memDC.FillSolidRect(midX - 1, y, 2, h, RGB(180, 180, 180));
		CPen borderPen(PS_SOLID, 1, RGB(255, 255, 255));
		CPen* oldPen = memDC.SelectObject(&borderPen);
		CBrush* oldBrush = static_cast<CBrush*>(memDC.SelectStockObject(NULL_BRUSH));
		memDC.Rectangle(x, y, x + w, y + h);
		memDC.SelectObject(oldBrush);
		memDC.SelectObject(oldPen);
		};

	// 委比（行18，最下面）
	STOCK::Volume bidTotal = 0;
	STOCK::Volume askTotal = 0;
	for (int i = 0; i < MAX_LEVEL; i++)
	{
		bidTotal += stockInfo.bidLevels[i].volume / 100;
		askTotal += stockInfo.askLevels[i].volume / 100;
	}

	double wbRatio = 0.0;
	if (bidTotal + askTotal > 0)
	{
		wbRatio = (double)(bidTotal - askTotal) / (bidTotal + askTotal) * 100;
	}

	CString wbLabel = _T("委  比:");
	int wbBarY = rowY(17);
	int wbBarH = rowH(17);
	memDC.SetTextColor(wbRatio > 0 ? COLOR_RED_UP : (wbRatio < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK));
	memDC.TextOut(textX, wbBarY + max(0, (wbBarH - memDC.GetTextExtent(wbLabel).cy) / 2), wbLabel);
	int wbBarX = textX + memDC.GetTextExtent(wbLabel).cx + g_data.RDPI(4);
	int wbBarW = right - wbBarX - g_data.RDPI(4);
	DrawRatioBar(wbBarX, wbBarY, wbBarW, wbBarH, wbRatio, bidTotal - askTotal);
	CString wbTxt;
	wbTxt.Format(_T("%.2f"), std::abs(wbRatio));
	DrawNetRatioBarText(wbBarX, wbBarY, wbBarW, wbBarH, wbTxt, _T(""));

	// 趋势、净比、振幅和换手率

	// 趋势判定（行9）
	// 根据当前视图分别计算对应周期的趋势：分时/5分/30分；日K视图保持双周期综合判定
	{
		CString trendTxt = _T("趋势: --");
		COLORREF trendColor = COLOR_GRAY_TEXT;
		auto stockDataForTrend = g_data.GetStockData(m_stock_id);
		if (stockDataForTrend)
		{
			STOCK::TrendDir finalDir = STOCK::TrendDir::DIR_SIDE;
			bool isShortPullback = false;
			bool isShortRebound = false;
			STOCK::SideTag sideTag = STOCK::SideTag::SIDE_MID;
			bool valid = false;

			if (m_isMin30KLineMode)
			{
				// 30分钟视图：基于30分钟K线波段结构判定
				auto* min30Obj = stockDataForTrend->getMin30KLineData();
				if (min30Obj && min30Obj->data.size() >= 25)
				{
					std::vector<STOCK::Bar> bars30;
					bars30.reserve(min30Obj->data.size());
					for (const auto& kp : min30Obj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

					if (CSignalAnalyzer::Calc30UpStruct(bars30))
						finalDir = STOCK::TrendDir::DIR_UP;
					else if (CSignalAnalyzer::Calc30DownStruct(bars30))
						finalDir = STOCK::TrendDir::DIR_DOWN;
					else
						finalDir = STOCK::TrendDir::DIR_SIDE;

					// 30分钟震荡区间时，用状态标记低吸/高抛
					STOCK::TrendState30m state30 = CSignalAnalyzer::Get30mTrendState(bars30);
					if (finalDir == STOCK::TrendDir::DIR_SIDE)
					{
						if (state30 == STOCK::TrendState30m::STATE_STRONG)
							sideTag = STOCK::SideTag::SIDE_LONG_POINT;
						else if (state30 == STOCK::TrendState30m::STATE_WEAK)
							sideTag = STOCK::SideTag::SIDE_SHORT_POINT;
					}
					valid = true;
				}
			}
			else if (m_isMin5KLineMode)
			{
				// 5分钟视图：基于5分钟K线短线多空判定
				auto* min5Obj = stockDataForTrend->getMin5KLineData();
				if (min5Obj && min5Obj->data.size() >= 20)
				{
					std::vector<STOCK::Bar> bars5;
					bars5.reserve(min5Obj->data.size());
					for (const auto& kp : min5Obj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));

					bool b5Up = CSignalAnalyzer::Calc5MinUp(bars5);
					bool b5Down = CSignalAnalyzer::Calc5MinDown(bars5);
					if (b5Up)
						finalDir = STOCK::TrendDir::DIR_UP;
					else if (b5Down)
						finalDir = STOCK::TrendDir::DIR_DOWN;
					else
						finalDir = STOCK::TrendDir::DIR_SIDE;
					valid = true;
				}
			}
			else if (!m_isKLineMode)
			{
				// 分时视图：基于分时数据判定（前后半段均价对比 + 当前价与累计均价关系）
				auto* tlObj = stockDataForTrend->getTimelineData();
				if (tlObj && tlObj->data.size() >= 10)
				{
					const auto& pts = tlObj->data;
					const auto& last = pts.back();
					double curPrice = last.price;
					double avgPrice = last.averagePrice;

					size_t n = pts.size();
					size_t half = n / 2;
					double firstHalfAvg = 0, secondHalfAvg = 0;
					size_t firstCnt = 0, secondCnt = 0;
					for (size_t i = 0; i < half && i < n; ++i) { firstHalfAvg += pts[i].price; ++firstCnt; }
					for (size_t i = half; i < n; ++i) { secondHalfAvg += pts[i].price; ++secondCnt; }
					if (firstCnt > 0) firstHalfAvg /= firstCnt;
					if (secondCnt > 0) secondHalfAvg /= secondCnt;

					bool priceUpTrend = (secondHalfAvg > firstHalfAvg) && (curPrice >= avgPrice);
					bool priceDownTrend = (secondHalfAvg < firstHalfAvg) && (curPrice <= avgPrice);

					if (priceUpTrend)
						finalDir = STOCK::TrendDir::DIR_UP;
					else if (priceDownTrend)
						finalDir = STOCK::TrendDir::DIR_DOWN;
					else
						finalDir = STOCK::TrendDir::DIR_SIDE;
					valid = true;
				}
			}
			else
			{
				// 日K视图：保持原双周期综合判定
				auto* min5Obj = stockDataForTrend->getMin5KLineData();
				auto* min30Obj = stockDataForTrend->getMin30KLineData();
				if (min5Obj && min5Obj->data.size() >= 20 && min30Obj && min30Obj->data.size() >= 25)
				{
					std::vector<STOCK::Bar> bars5, bars30;
					bars5.reserve(min5Obj->data.size());
					for (const auto& kp : min5Obj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));
					bars30.reserve(min30Obj->data.size());
					for (const auto& kp : min30Obj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

					STOCK::Volume outerVolTrend = stockInfo.outerVolume;
					STOCK::Volume innerVolTrend = stockInfo.innerVolume;
					STOCK::TrendResult trendResult = CSignalAnalyzer::CalcTrend(bars5, bars30, outerVolTrend, innerVolTrend);
					finalDir = trendResult.FinalTrend;
					isShortPullback = trendResult.IsShortPullback;
					isShortRebound = trendResult.IsShortRebound;
					sideTag = trendResult.SideTagValue;
					valid = true;
				}
			}

			if (valid)
			{
				CString trendLabel;
				if (finalDir == STOCK::TrendDir::DIR_UP)
				{
					trendLabel = _T("上涨");
					trendColor = COLOR_RED_UP;
					if (isShortPullback)
						trendLabel += _T("(回调)");
				}
				else if (finalDir == STOCK::TrendDir::DIR_DOWN)
				{
					trendLabel = _T("下跌");
					trendColor = COLOR_GREEN_DOWN;
					if (isShortRebound)
						trendLabel += _T("(反弹)");
				}
				else
				{
					trendLabel = _T("震荡");
					trendColor = COLOR_GRAY_TEXT;
					if (sideTag == STOCK::SideTag::SIDE_LONG_POINT)
						trendLabel += _T("(低吸)");
					else if (sideTag == STOCK::SideTag::SIDE_SHORT_POINT)
						trendLabel += _T("(高抛)");
				}
				trendTxt.Format(_T("趋势: %s"), trendLabel.GetString());
			}
		}
		memDC.SetTextColor(trendColor);
		memDC.TextOut(textX, rowY(8) + max(0, (rowH(8) - memDC.GetTextExtent(trendTxt).cy) / 2), trendTxt);
	}

	auto stockDataPtr = g_data.GetStockData(m_stock_id);

	// 净比00：最近10根5秒净比条形图（行9），条形图宽度与净比01对齐
	{
		CString label00 = _T("净比00:");
		int row9H = rowH(9);
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(textX, rowY(9) + max(0, (rowH(9) - memDC.GetTextExtent(label00).cy) / 2), label00);

		// 用"净比01:"的宽度计算起始位置，保证与下方净比01~20条形图对齐
		CString alignLabel;
		alignLabel.Format(_T("净比%02d:"), 1);
		int periodBarX = textX + memDC.GetTextExtent(alignLabel).cx + g_data.RDPI(4);
		int periodBarW = right - periodBarX - g_data.RDPI(4);
		int periodBarRight = periodBarX + periodBarW;  // 右边界

		if (periodBarW > 0 && stockDataPtr)
		{
			std::vector<double> recentRatios;
			const int barCount = 12;
			if (stockDataPtr->GetRecentNetRatios(barCount, recentRatios))
			{
				int baseSlotW = periodBarW / barCount;
				int remSlotW = periodBarW % barCount;
				int barPad = max(1, baseSlotW / 8);
				int barW = baseSlotW - barPad * 2;
				if (barW < 1) barW = 1;
				int slotX = periodBarX;
				for (int i = 0; i < barCount; i++)
				{
					int curSlotW = baseSlotW + (i < remSlotW ? 1 : 0);
					int curBarX = slotX + barPad;
					int curBarW = curSlotW - barPad * 2;
					if (curBarW < 1) curBarW = 1;

					double r = recentRatios[i];
					COLORREF barColor;
					if (r > 0) barColor = COLOR_RED_UP;
					else if (r < 0) barColor = COLOR_GREEN_DOWN;
					else barColor = COLOR_BLACK;

					int maxBarH = row9H - g_data.RDPI(4);
					int barH = static_cast<int>((std::min)(std::sqrt(std::abs(r) / 100.0), 1.0) * maxBarH);
					if (barH < 2) barH = 2;

					int barY = rowY(9) + (row9H - barH) / 2;
					memDC.FillSolidRect(curBarX, barY, curBarW, barH, barColor);
					slotX += curSlotW;
				}
			}
		}
	}

	STOCK::Volume netDiff = outerVol - innerVol;
	STOCK::Volume totalInnerOuter = outerVol + innerVol;
	double netRatio = totalInnerOuter > 0 ? static_cast<double>(netDiff) / totalInnerOuter * 100 : 0;
	static std::map<std::wstring, double> lastNetRatioMap;
	static std::map<std::wstring, CString> lastNetRatioTrendMap;
	CString netRatioTrend;
	double absNetRatio = std::abs(netRatio);
	auto lastNetRatioIt = lastNetRatioMap.find(m_stock_id);
	if (lastNetRatioIt != lastNetRatioMap.end())
	{
		double lastAbsNetRatio = std::abs(lastNetRatioIt->second);
		if (absNetRatio > lastAbsNetRatio)
		{
			netRatioTrend = _T("↑");
			lastNetRatioMap[m_stock_id] = netRatio;
			lastNetRatioTrendMap[m_stock_id] = netRatioTrend;
		}
		else if (absNetRatio < lastAbsNetRatio)
		{
			netRatioTrend = _T("↓");
			lastNetRatioMap[m_stock_id] = netRatio;
			lastNetRatioTrendMap[m_stock_id] = netRatioTrend;
		}
		else
		{
			auto lastTrendIt = lastNetRatioTrendMap.find(m_stock_id);
			if (lastTrendIt != lastNetRatioTrendMap.end())
				netRatioTrend = lastTrendIt->second;
		}
	}
	else
	{
		double previousRatio = 0;
		if (stockDataPtr && stockDataPtr->GetPreviousInnerOuterTotalRatio(previousRatio))
		{
			double previousAbsRatio = std::abs(previousRatio);
			if (absNetRatio > previousAbsRatio)
			{
				netRatioTrend = _T("↑");
				lastNetRatioTrendMap[m_stock_id] = netRatioTrend;
			}
			else if (absNetRatio < previousAbsRatio)
			{
				netRatioTrend = _T("↓");
				lastNetRatioTrendMap[m_stock_id] = netRatioTrend;
			}
		}
		lastNetRatioMap[m_stock_id] = netRatio;
	}
	CString netDiffStr = CCommon::FormatVolumeInt(std::abs(netDiff));

	int barY = rowY(14);
	int barH = rowHeight;
	COLORREF netRatioRedColor = NET_RATIO_RED_COLORS[GetNetRatioColorIndex(netRatio)];
	COLORREF netRatioGreenColor = NET_RATIO_GREEN_COLORS[GetNetRatioColorIndex(netRatio)];
	CString netRatioLabel = _T("净比99:");
	memDC.SetTextColor(netDiff > 0 ? COLOR_RED_UP : (netDiff < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK));
	memDC.TextOut(textX, barY + max(0, (barH - memDC.GetTextExtent(netRatioLabel).cy) / 2), netRatioLabel);
	int barX = textX + memDC.GetTextExtent(netRatioLabel).cx + g_data.RDPI(4);
	int barW = right - barX - g_data.RDPI(4);
	if (barW > 0)
	{
		DrawRatioBar(barX, barY, barW, barH, netRatio, netDiff);

		CString diffSign = netDiff >= 0 ? _T("+") : _T("-");
		CString netRatioTxt;
		netRatioTxt.Format(_T("%.2f%s"), std::abs(netRatio), netRatioTrend.GetString());
		CString netDiffTxt;
		netDiffTxt.Format(_T("%s%s"), diffSign.GetString(), netDiffStr.GetString());
		DrawNetRatioBarText(barX, barY, barW, barH, netRatioTxt, netDiffTxt);
	}

	// 净差1/5/10/20（行13-16），使用净比条形图样式
	const int netPeriods[] = { 1, 5, 10, 20 };
	static std::map<std::wstring, std::map<int, double>> lastPeriodRatioMap;
	static std::map<std::wstring, std::map<int, CString>> lastPeriodRatioTrendMap;
	for (int i = 0; i < 4; i++)
	{
		int periodBarY = rowY(10 + i);
		int periodBarH = rowHeight;
		CString periodLabel;
		periodLabel.Format(_T("净比%02d:"), netPeriods[i]);
		COLORREF periodLabelColor = COLOR_BLACK;
		STOCK::Volume diff = 0;
		double ratio = 0;
		bool hasData = stockDataPtr && stockDataPtr->GetInnerOuterNetDiff(netPeriods[i], diff, ratio);
		COLORREF periodRedColor = NET_RATIO_RED_COLORS[GetNetRatioColorIndex(ratio)];
		COLORREF periodGreenColor = NET_RATIO_GREEN_COLORS[GetNetRatioColorIndex(ratio)];
		if (hasData)
			periodLabelColor = diff > 0 ? COLOR_RED_UP : (diff < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
		memDC.SetTextColor(periodLabelColor);
		memDC.TextOut(textX, periodBarY + max(0, (periodBarH - memDC.GetTextExtent(periodLabel).cy) / 2), periodLabel);

		int periodBarX = textX + memDC.GetTextExtent(periodLabel).cx + g_data.RDPI(4);
		int periodBarW = right - periodBarX - g_data.RDPI(4);
		if (periodBarW <= 0)
			continue;

		DrawRatioBar(periodBarX, periodBarY, periodBarW, periodBarH, ratio, diff);

		CString periodTxt;
		CString periodDiffTxt;
		if (hasData)
		{
			CString diffStr = CCommon::FormatVolumeInt(std::abs(diff) / 100.0);
			CString ratioTrend;
			auto stockPeriodRatioIt = lastPeriodRatioMap.find(m_stock_id);
			if (stockPeriodRatioIt != lastPeriodRatioMap.end())
			{
				auto lastRatioIt = stockPeriodRatioIt->second.find(netPeriods[i]);
				if (lastRatioIt != stockPeriodRatioIt->second.end())
				{
					double absRatio = std::abs(ratio);
					double lastAbsRatio = std::abs(lastRatioIt->second);
					if (absRatio > lastAbsRatio)
					{
						ratioTrend = _T("↑");
						lastPeriodRatioMap[m_stock_id][netPeriods[i]] = ratio;
						lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
					}
					else if (absRatio < lastAbsRatio)
					{
						ratioTrend = _T("↓");
						lastPeriodRatioMap[m_stock_id][netPeriods[i]] = ratio;
						lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
					}
					else
					{
						auto stockPeriodTrendIt = lastPeriodRatioTrendMap.find(m_stock_id);
						if (stockPeriodTrendIt != lastPeriodRatioTrendMap.end())
						{
							auto lastTrendIt = stockPeriodTrendIt->second.find(netPeriods[i]);
							if (lastTrendIt != stockPeriodTrendIt->second.end())
								ratioTrend = lastTrendIt->second;
						}
					}
				}
				else
				{
					STOCK::Volume previousDiff = 0;
					double previousRatio = 0;
					if (stockDataPtr->GetPreviousInnerOuterNetDiff(netPeriods[i], previousDiff, previousRatio))
					{
						double absRatio = std::abs(ratio);
						double previousAbsRatio = std::abs(previousRatio);
						if (absRatio > previousAbsRatio)
						{
							ratioTrend = _T("↑");
							lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
						}
						else if (absRatio < previousAbsRatio)
						{
							ratioTrend = _T("↓");
							lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
						}
					}
					lastPeriodRatioMap[m_stock_id][netPeriods[i]] = ratio;
				}
			}
			else
			{
				STOCK::Volume previousDiff = 0;
				double previousRatio = 0;
				if (stockDataPtr->GetPreviousInnerOuterNetDiff(netPeriods[i], previousDiff, previousRatio))
				{
					double absRatio = std::abs(ratio);
					double previousAbsRatio = std::abs(previousRatio);
					if (absRatio > previousAbsRatio)
					{
						ratioTrend = _T("↑");
						lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
					}
					else if (absRatio < previousAbsRatio)
					{
						ratioTrend = _T("↓");
						lastPeriodRatioTrendMap[m_stock_id][netPeriods[i]] = ratioTrend;
					}
				}
				lastPeriodRatioMap[m_stock_id][netPeriods[i]] = ratio;
			}

			CString diffSign = diff >= 0 ? _T("+") : _T("-");
			periodTxt.Format(_T("%.2f%s"), std::abs(ratio), ratioTrend.GetString());
			periodDiffTxt.Format(_T("%s%s"), diffSign.GetString(), diffStr.GetString());
		}
		else
		{
			periodTxt = _T("--");
			periodDiffTxt = _T("--");
		}

		DrawNetRatioBarText(periodBarX, periodBarY, periodBarW, periodBarH, periodTxt, periodDiffTxt);
	}

	// 振幅、换手率（所有净比下方）
	if (!klineData.empty())
	{
		int textXAmp = left + g_data.RDPI(5) + 3;

		CString ampTxt;
		float fluctuation = stockInfo.highPrice - stockInfo.lowPrice;
		float fluctuationPercent = stockInfo.prevClosePrice != 0 ? (fluctuation / stockInfo.prevClosePrice) * 100 : 0;

		auto stockDataPtr2 = g_data.GetStockData(m_stock_id);
		auto* klinePtr2 = stockDataPtr2 ? stockDataPtr2->getKLineData() : nullptr;
		double avgAmplitude5 = klinePtr2 ? klinePtr2->CalculateAverageAmplitude(5) : 0;
		CString amp5Str;
		if (avgAmplitude5 > 0)
			amp5Str.Format(_T("%.2f%%"), avgAmplitude5);
		else
			amp5Str = _T("--");
		ampTxt.Format(_T("振幅: 01:%.2f%% 05:%s"), fluctuationPercent, amp5Str.GetString());
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(textXAmp, rowY(15) + max(0, (rowH(15) - memDC.GetTextExtent(ampTxt).cy) / 2), ampTxt);
	}

	CString turnoverTxt;
	turnoverTxt.Format(_T("换手率: %.2f%%"), stockInfo.turnoverRate);
	if (stockInfo.turnoverRate >= 5)
		memDC.SetTextColor(COLOR_RED_UP);
	else
		memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(textX, rowY(16) + max(0, (rowH(16) - memDC.GetTextExtent(turnoverTxt).cy) / 2), turnoverTxt);
}

void CFloatingWnd::DrawChipPeakPanel(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo, const STOCK::ChipDistribution& chipData, const std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	// 按比例分配行高，与盘口面板一致
	const int totalRows = 19;
	const int headerHeight = g_data.RDPI(26);  // 主标题栏高度
	const int obTitleH = g_data.RDPI(16);       // 盘口标题栏高度，与走势图标题栏一致
	const int topOffset = headerHeight + obTitleH;  // 内容从主标题栏+盘口标题栏下方开始
	const int panelW = right - left;
	// 绘制盘口标题栏背景（在主标题栏下方）
	memDC.FillSolidRect(left, headerHeight, panelW, obTitleH, RGB(245, 245, 245));
	const int rowHeight = (height - obTitleH) / totalRows;
	const int panelH = height - obTitleH;
	if (panelW <= 0 || panelH <= 0)
		return;
	const int rem = panelH % totalRows;    // 余数：前rem行多1px
	// 辅助：计算第i行(0-based)的Y坐标
	auto rowY = [&](int i) -> int {
		if (i < rem) return topOffset + i * (rowHeight + 1);
		else return topOffset + rem * (rowHeight + 1) + (i - rem) * rowHeight;
		};
	// 辅助：计算第i行的高度
	auto rowH = [&](int i) -> int {
		return (i < rem) ? (rowHeight + 1) : rowHeight;
		};

	memDC.FillSolidRect(left, topOffset, panelW, panelH, RGB(250, 250, 250));
	memDC.SetBkMode(TRANSPARENT);

	if (!chipData.IsValid())
	{
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		memDC.TextOut(left + g_data.RDPI(5), topOffset + max(0, (panelH - memDC.GetTextExtent(_T("暂无筹码数据")).cy) / 2), _T("暂无筹码数据"));
		return;
	}

	std::vector<STOCK::ChipPoint> points = chipData.points;
	if (!m_isKLineMode && stockInfo.circulatingAShares > 0 && !timelinePoint.empty())
	{
		const double CHIP_ATTRITION_N = 1.3;
		const double MAX_EFFECT_TURN = 0.85;
		const double PRICE_STEP = 0.01;
		const double FLOAT_CORRECT_THRESHOLD = 0.01;

		double yMin = 999999.0;
		double yMax = 0.0;
		for (const auto& point : chipData.points)
		{
			if (point.price > 0)
			{
				yMin = min(yMin, point.price);
				yMax = max(yMax, point.price);
			}
		}

		double limitDown = stockInfo.prevClosePrice > 0 && stockInfo.priceLimit > 0 ? stockInfo.prevClosePrice - stockInfo.priceLimit : yMin;
		double limitUp = stockInfo.prevClosePrice > 0 && stockInfo.priceLimit > 0 ? stockInfo.prevClosePrice + stockInfo.priceLimit : yMax;
		double gridMin = floor(min(yMin, limitDown) * 100.0) / 100.0;
		double gridMax = ceil(max(yMax, limitUp) * 100.0) / 100.0;
		if (gridMax > gridMin)
		{
			std::vector<double> priceLevels;
			for (double cur = gridMin; cur <= gridMax + PRICE_STEP / 2; cur += PRICE_STEP)
				priceLevels.push_back(round(cur * 100.0) / 100.0);
			std::vector<double> chipArray(priceLevels.size(), 0.0);
			auto findPriceIndex = [&](double price) -> int {
				auto it = std::lower_bound(priceLevels.begin(), priceLevels.end(), round(price * 100.0) / 100.0);
				return static_cast<int>(it - priceLevels.begin());
				};

			for (const auto& point : chipData.points)
			{
				int idx = findPriceIndex(point.price);
				if (idx >= 0 && idx < static_cast<int>(chipArray.size()))
					chipArray[idx] += point.percent * stockInfo.circulatingAShares;
			}

			std::set<std::string> processedMinuteSet;
			for (const auto& item : timelinePoint)
			{
				if (item.volume <= 0 || processedMinuteSet.find(item.time) != processedMinuteSet.end())
					continue;
				processedMinuteSet.insert(item.time);

				double minuteTurn = static_cast<double>(item.volume) / static_cast<double>(stockInfo.circulatingAShares);
				double effTurn = min(MAX_EFFECT_TURN, minuteTurn * CHIP_ATTRITION_N);
				double retainRate = 1.0 - effTurn;
				double addTotalShare = effTurn * stockInfo.circulatingAShares;
				for (auto& val : chipArray)
					val *= retainRate;

				double price = item.price > 0 ? item.price : item.averagePrice;
				int idx = findPriceIndex(price);
				if (idx >= 0 && idx < static_cast<int>(chipArray.size()))
					chipArray[idx] += addTotalShare;

				double sumAll = 0.0;
				for (auto val : chipArray)
					sumAll += val;
				if (sumAll > 0 && fabs(sumAll - stockInfo.circulatingAShares) > FLOAT_CORRECT_THRESHOLD)
				{
					double scale = static_cast<double>(stockInfo.circulatingAShares) / sumAll;
					for (auto& val : chipArray)
						val *= scale;
				}
			}

			double totalShares = static_cast<double>(stockInfo.circulatingAShares);
			points.clear();
			points.reserve(priceLevels.size());
			double weightSum = 0.0;
			double profitShare = 0.0;
			double currentPrice = stockInfo.currentPrice > 0 ? stockInfo.currentPrice : stockInfo.prevClosePrice;
			for (size_t i = 0; i < priceLevels.size(); ++i)
			{
				weightSum += priceLevels[i] * chipArray[i];
				if (priceLevels[i] < currentPrice)
					profitShare += chipArray[i];
				STOCK::ChipPoint point;
				point.price = priceLevels[i];
				point.percent = totalShares > 0 ? chipArray[i] / totalShares : 0.0;
				points.push_back(point);
			}
		}
	}

	double totalPercent = 0.0;
	double weightSum = 0.0;
	double profitPercent = 0.0;
	double maxPercent = 0.0;
	double currentPrice = stockInfo.currentPrice > 0 ? stockInfo.currentPrice : stockInfo.prevClosePrice;
	for (const auto& point : points)
	{
		if (point.percent <= 0) continue;
		totalPercent += point.percent;
		weightSum += point.price * point.percent;
		if (point.price < currentPrice)
			profitPercent += point.percent;
		maxPercent = max(maxPercent, point.percent);
	}
	if (totalPercent <= 0 || maxPercent <= 0)
		return;

	double avgCost = weightSum / totalPercent;
	double cumPercent = 0.0;
	double chip90Low = 0.0;
	double chip90High = 0.0;
	bool findLow = false;
	std::sort(points.begin(), points.end(), [](const STOCK::ChipPoint& a, const STOCK::ChipPoint& b) { return a.price < b.price; });
	for (const auto& point : points)
	{
		cumPercent += point.percent;
		if (!findLow && cumPercent >= totalPercent * 0.05)
		{
			chip90Low = point.price;
			findLow = true;
		}
		if (cumPercent >= totalPercent * 0.95)
		{
			chip90High = point.price;
			break;
		}
	}

	// 筹码峰图：从行0开始，到底部倒数第3行
	const int chartRowEnd = totalRows - 3;
	const int chartTop = topOffset + g_data.RDPI(2);
	const int chartBottom = rowY(chartRowEnd);
	const int chartLeft = left + g_data.RDPI(6);
	const int chartRight = right - g_data.RDPI(6);
	const int chartH = chartBottom - chartTop;
	const int chartW = chartRight - chartLeft;
	if (chartH <= 0 || chartW <= 0)
		return;

	double minPrice = points.front().price;
	double maxPrice = points.back().price;
	if (maxPrice <= minPrice)
		return;

	CPen borderPen(PS_SOLID, 1, RGB(220, 220, 220));
	CPen* oldPen = memDC.SelectObject(&borderPen);
	memDC.Rectangle(chartLeft, chartTop, chartRight, chartBottom);
	memDC.SelectObject(oldPen);

	auto priceToY = [&](double price) -> int {
		return chartBottom - static_cast<int>((price - minPrice) / (maxPrice - minPrice) * chartH);
		};

	for (const auto& point : points)
	{
		if (point.percent <= 0) continue;
		int y = priceToY(point.price);
		int barW = max(1, static_cast<int>(point.percent / maxPercent * chartW));
		COLORREF color = point.price < currentPrice ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		memDC.FillSolidRect(chartLeft, y, barW, max(1, g_data.RDPI(1)), color);
	}

	if (avgCost > minPrice && avgCost < maxPrice)
	{
		int avgY = priceToY(avgCost);
		CPen avgPen(PS_DOT, 1, RGB(0, 80, 204));
		oldPen = memDC.SelectObject(&avgPen);
		memDC.MoveTo(chartLeft, avgY);
		memDC.LineTo(chartRight, avgY);
		memDC.SelectObject(oldPen);
		// 均价标签绘制在线条右侧
		CString avgLabel;
		avgLabel.Format(_T("均:%s"), CCommon::FormatFloat(avgCost));
		memDC.SetTextColor(RGB(0, 80, 204));
		CSize avgLabelSize = memDC.GetTextExtent(avgLabel);
		int avgLabelX = chartRight - avgLabelSize.cx - g_data.RDPI(2);
		int avgLabelY = avgY - avgLabelSize.cy / 2;
		// 避免标签超出图表区域
		avgLabelY = max(chartTop, min(avgLabelY, chartBottom - avgLabelSize.cy));
		memDC.TextOut(avgLabelX, avgLabelY, avgLabel);
	}

	if (currentPrice > minPrice && currentPrice < maxPrice)
	{
		int y = priceToY(currentPrice);
		CPen curPen(PS_DOT, 1, RGB(112, 32, 176));
		oldPen = memDC.SelectObject(&curPen);
		memDC.MoveTo(chartLeft, y);
		memDC.LineTo(chartRight, y);
		memDC.SelectObject(oldPen);
		CString priceTxt;
		priceTxt.Format(_T("现 %s"), CCommon::FormatFloat(currentPrice));
		CSize txtSize = memDC.GetTextExtent(priceTxt);
		int paddingX = g_data.RDPI(4);
		int paddingY = g_data.RDPI(2);
		int labelW = txtSize.cx + paddingX * 2;
		int labelH = txtSize.cy + paddingY * 2;
		int labelLeft = chartRight - labelW - g_data.RDPI(2);
		int labelTop = min(max(chartTop, y - labelH / 2), chartBottom - labelH);
		CRect labelRect(labelLeft, labelTop, labelLeft + labelW, labelTop + labelH);
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(labelRect.left + paddingX, labelRect.top + paddingY, priceTxt);
	}

	CString highTxt;
	highTxt = CCommon::FormatFloat(maxPrice);
	CString lowTxt;
	lowTxt = CCommon::FormatFloat(minPrice);
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(chartRight - memDC.GetTextExtent(highTxt).cx, chartTop, highTxt);
	memDC.TextOut(chartRight - memDC.GetTextExtent(lowTxt).cx, chartBottom - memDC.GetTextExtent(lowTxt).cy, lowTxt);

	// 文字信息绘制在筹码峰图下方，拆分为3行
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	// 获利比例：标签 + 带红绿背景的数字
	// 左红（亏损比例）右绿（获利比例），分界点由获利比例决定，红绿各自长度按比例分配
	CString profitLabelTxt = _T("获利比例:");
	double profitRatio = totalPercent > 0 ? (profitPercent / totalPercent * 100.0) : 0.0;
	CString profitNumTxt;
	profitNumTxt.Format(_T("%.1f%%"), profitRatio);
	int profitY = rowY(chartRowEnd) + max(0, (rowH(chartRowEnd) - memDC.GetTextExtent(profitLabelTxt).cy) / 2);
	int profitX = left + g_data.RDPI(5);
	memDC.TextOut(profitX, profitY, profitLabelTxt);
	{
		int labelW = memDC.GetTextExtent(profitLabelTxt).cx;
		int numH = memDC.GetTextExtent(profitNumTxt).cy;
		int barX = profitX + labelW;
		// 总长度固定：从标签后到面板右边界，留少量右边距
		int barW = max(0, right - barX - g_data.RDPI(4));
		int barH = numH + g_data.RDPI(2);
		int barY = profitY - g_data.RDPI(1);

		// 红色长度 = 总宽度 × X/100（获利比例）；绿色长度 = 总宽度 × (100-X)/100（套牢比例）
		// 红色用标准红，绿色与筹码峰 COLOR_GREEN_DOWN 一致
		int redW = static_cast<int>(barW * profitRatio / 100.0 + 0.5);
		int greenW = barW - redW;
		if (redW > 0)
		{
			CRect redRect(barX, barY, barX + redW, barY + barH);
			CBrush redBrush(RGB(179, 64, 65));
			CBrush* pOldBrush = memDC.SelectObject(&redBrush);
			memDC.FillRect(&redRect, &redBrush);
			memDC.SelectObject(pOldBrush);
		}
		if (greenW > 0)
		{
			CRect greenRect(barX + redW, barY, barX + redW + greenW, barY + barH);
			CBrush greenBrush(COLOR_GREEN_DOWN);
			CBrush* pOldBrush = memDC.SelectObject(&greenBrush);
			memDC.FillRect(&greenRect, &greenBrush);
			memDC.SelectObject(pOldBrush);
		}

		// 数字绘制在背景条左侧（白色文字，带少量内边距）
		memDC.SetTextColor(RGB(255, 255, 255));
		memDC.TextOut(barX + g_data.RDPI(4), profitY, profitNumTxt);
		memDC.SetTextColor(COLOR_GRAY_TEXT);
	}
	CString avgCostTxt;
	avgCostTxt.Format(_T("平均成本:%s"), CCommon::FormatFloat(avgCost));
	memDC.TextOut(left + g_data.RDPI(5), rowY(chartRowEnd + 1) + max(0, (rowH(chartRowEnd + 1) - memDC.GetTextExtent(avgCostTxt).cy) / 2), avgCostTxt);

	CString rangeTxt;
	rangeTxt.Format(_T("90%%成本:%s-%s"), CCommon::FormatFloat(chip90Low), CCommon::FormatFloat(chip90High));
	memDC.TextOut(left + g_data.RDPI(5), rowY(chartRowEnd + 2) + max(0, (rowH(chartRowEnd + 2) - memDC.GetTextExtent(rangeTxt).cy) / 2), rangeTxt);
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
	if (!m_isOverviewMode && m_showStockList)
	{
		const int stockListWidth = g_data.RDPI(65);
		const int headerHeight = g_data.RDPI(26);
		const int titleH = g_data.RDPI(16);
		const int rowHeight = g_data.RDPI(35);
		const int listTop = headerHeight + titleH + g_data.RDPI(2);

		if (point.x >= 0 && point.x < stockListWidth && point.y >= listTop)
		{
			int rowIndex = (point.y - listTop) / rowHeight;
			std::vector<std::wstring> stockCodes;
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				for (const auto& code : g_data.m_setting_data.m_stock_codes)
				{
					if (GetStockPriority(code) >= 200)  // 与绘制一致，过滤指数
						stockCodes.push_back(code);
				}
			}
			if (rowIndex >= 0 && rowIndex < (int)stockCodes.size())
			{
				const std::wstring& clickedCode = stockCodes[rowIndex];
				if (clickedCode != m_stock_id)
				{
					m_stock_id = clickedCode;
					m_isFirstRequest = true;
					// 切换股票后重置各K线获取计时器，立即获取新股票数据
					m_last_request_time = 0;
					m_last_min5_fetch_time = 0;
					m_last_min30_fetch_time = 0;
					m_timelineScrollOffset = -1;
					UpdateModeButtons();
					Invalidate();
					RequestData();
				}
				return;
			}
		}
	}

	// 总览模式下的鼠标点击处理
	if (m_isOverviewMode)
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
					m_isOverviewMode = false;
					m_isKLineMode = false;
					m_isMin5KLineMode = false;
					m_isMin30KLineMode = false;
					m_showChipPeak = false;
					m_stock_id = rowInfo.code;
					m_isFirstRequest = true;  // 切换股票后允许首次请求
					// 切换股票后重置各K线获取计时器，立即获取新股票数据
					m_last_request_time = 0;
					m_last_min5_fetch_time = 0;
					m_last_min30_fetch_time = 0;
					UpdateModeButtons();
					UpdatePeriodComboVisibility();
					Invalidate();
					RequestData();
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
	if (!m_isOverviewMode && isDoubleClick)
	{
		CRect rect;
		GetClientRect(&rect);
		bool isIndex = (GetStockPriority(m_stock_id) < 200);
		bool isIndexKLine = isIndex && m_isKLineMode;
		const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
		const int chartWidth = rect.Width() - orderBookWidth;
		const int headerHeight = g_data.RDPI(26);

		const int yAxisWidth = g_data.RDPI(50);
		const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
		const int chartLeft = stockListWidth + yAxisWidth;

		if (point.x >= chartLeft && point.x < chartWidth && point.y >= headerHeight)
		{
			std::vector<STOCK::TimelinePoint> timelinePoint;
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = g_data.GetStockData(m_stock_id);
				if (stockData)
				{
					if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
					{
						auto klineObj = stockData->getKLineData();
						if (klineObj)
						{
							for (const auto& kp : klineObj->data)
							{
								STOCK::TimelinePoint tp;
								if (kp.day.length() >= 10)
									tp.time = kp.day.substr(5, 5);
								else
									tp.time = kp.day;
								tp.price = kp.close;
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_isMin5KLineMode)
					{
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
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_isMin30KLineMode)
					{
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
								tp.volume = kp.volume;
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

			if (!timelinePoint.empty())
			{
				int totalPoints = static_cast<int>(timelinePoint.size());
				int visibleCount = min(m_timelineVisibleCount, totalPoints);
				int maxOffset = max(0, totalPoints - visibleCount);
				if (m_timelineScrollOffset < 0 || m_timelineScrollOffset >= maxOffset)
					m_timelineScrollOffset = maxOffset;
				int startIndex = max(0, min(m_timelineScrollOffset, maxOffset));

				int adjX = point.x - chartLeft;
				int effectiveWidth = chartWidth - chartLeft;
				int relIndex = static_cast<int>(adjX * static_cast<float>(visibleCount) / effectiveWidth);
				relIndex = max(0, min(relIndex, visibleCount - 1));
				int countX = startIndex + relIndex;
				countX = max(0, min(countX, totalPoints - 1));

				const auto& item = timelinePoint[countX];
				m_pendingTradeTime = item.time.c_str();
				m_pendingTradePrice = item.price;
				// PostMessage(FWND_MSG_SHOW_TRADE_DLG);  // 测试买卖点检测时暂时屏蔽交易记录弹窗
				TestSmartSignalOnDoubleClick(countX);
				return;
			}
		}
	}

	// 拖动启动：在图表区域内按下左键开始拖动滚动
	{
		CRect dragRect;
		GetClientRect(&dragRect);
		bool isIdx = (GetStockPriority(m_stock_id) < 200);
		bool isIdxKLine = isIdx && m_isKLineMode;
		const int dragOrderBookWidth = isIdxKLine ? 0 : ORDER_BOOK_WIDTH;
		const int dragChartWidth = dragRect.Width() - dragOrderBookWidth;
		const int dragYAxisWidth = g_data.RDPI(50);
		const int dragStockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
		const int dragChartLeft = dragStockListWidth + dragYAxisWidth;
		const int dragHeaderHeight = g_data.RDPI(26);
		if (!m_isOverviewMode && point.x >= dragChartLeft && point.x < dragChartWidth && point.y >= dragHeaderHeight)
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
	bool wasDragging = m_isTimelineDragging || m_isKLineDragging;
	m_isTimelineDragging = false;
	m_isKLineDragging = false;
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

// ========== KDJ 指标计算与绘制 ==========

std::vector<CFloatingWnd::KDJData> CFloatingWnd::CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int period /* = 9 */)
{
	std::vector<KDJData> result;
	result.reserve(klineData.size());
	if (klineData.empty() || period <= 0)
		return result;

	// 初始 K = D = 50
	double prevK = 50.0;
	double prevD = 50.0;

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		KDJData kd;
		kd.k = 0;
		kd.d = 0;
		kd.j = 0;
		kd.valid = false;

		// 需要至少 period 个数据点才能计算 RSV
		if (i >= period - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period + 1; j <= i; j++)
			{
				if (klineData[j].high > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					hasValid = true;
				}
				if (klineData[j].low > 0)
				{
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}

			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				double rsv = (static_cast<double>(klineData[i].close) - lowest) / (highest - lowest) * 100.0;
				// 限制 RSV 在 [0, 100] 范围
				if (rsv < 0) rsv = 0;
				if (rsv > 100) rsv = 100;

				// K(t) = 2/3 * K(t-1) + 1/3 * RSV(t)
				double curK = 2.0 / 3.0 * prevK + 1.0 / 3.0 * rsv;
				// D(t) = 2/3 * D(t-1) + 1/3 * K(t)
				double curD = 2.0 / 3.0 * prevD + 1.0 / 3.0 * curK;
				// J = 3K - 2D
				double curJ = 3.0 * curK - 2.0 * curD;

				kd.k = curK;
				kd.d = curD;
				kd.j = curJ;
				kd.valid = true;

				prevK = curK;
				prevD = curD;
			}
			else
			{
				// 数据无效，保持上一状态
				kd.k = prevK;
				kd.d = prevD;
				kd.j = 3.0 * prevK - 2.0 * prevD;
				kd.valid = true;
			}
		}
		else
		{
			// 前 period-1 个数据点无法计算 RSV，使用初始值 50
			kd.k = prevK;
			kd.d = prevD;
			kd.j = 3.0 * prevK - 2.0 * prevD;
			kd.valid = true;
		}

		result.push_back(kd);
	}

	return result;
}

std::vector<CFloatingWnd::KDJData> CFloatingWnd::CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period /* = 9 */)
{
	std::vector<KDJData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.empty() || period <= 0)
		return result;

	double prevK = 50.0;
	double prevD = 50.0;

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		KDJData kd;
		kd.k = 0;
		kd.d = 0;
		kd.j = 0;
		kd.valid = false;

		if (i >= period - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}

			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				double rsv = (static_cast<double>(timelinePoint[i].price) - lowest) / (highest - lowest) * 100.0;
				if (rsv < 0) rsv = 0;
				if (rsv > 100) rsv = 100;

				double curK = 2.0 / 3.0 * prevK + 1.0 / 3.0 * rsv;
				double curD = 2.0 / 3.0 * prevD + 1.0 / 3.0 * curK;
				double curJ = 3.0 * curK - 2.0 * curD;

				kd.k = curK;
				kd.d = curD;
				kd.j = curJ;
				kd.valid = true;

				prevK = curK;
				prevD = curD;
			}
			else
			{
				kd.k = prevK;
				kd.d = prevD;
				kd.j = 3.0 * prevK - 2.0 * prevD;
				kd.valid = true;
			}
		}
		else
		{
			kd.k = prevK;
			kd.d = prevD;
			kd.j = 3.0 * prevK - 2.0 * prevD;
			kd.valid = true;
		}

		result.push_back(kd);
	}

	return result;
}

void CFloatingWnd::DrawKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::KLinePoint>& klineData)
{
	if (klineData.empty())
		return;

	auto kdjData = CalculateKDJ(klineData);
	if (kdjData.empty())
		return;

	// KDJ 指标范围通常 [0, 100]，J 可能超出范围 [-20, 120]
	// 计算实际显示范围以容纳所有数据
	double minVal = 0;
	double maxVal = 100;
	for (const auto& kd : kdjData)
	{
		if (!kd.valid) continue;
		minVal = (std::min)(minVal, kd.j);
		maxVal = (std::max)(maxVal, kd.j);
	}
	// 限制在合理范围 [0, 100] 为主，J 上下扩展
	if (minVal > 0) minVal = 0;
	if (maxVal < 100) maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	// Y 坐标计算：value -> y
	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	// 绘制背景
	memDC.FillSolidRect(CRect(x, y, x + width, y + height), COLOR_WHITE);

	// 绘制网格线和刻度：0, 20, 50, 80, 100
	CPen gridPen(PS_DASHDOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&gridPen);
	double gridValues[] = { 0, 20, 50, 80, 100 };
	for (double v : gridValues)
	{
		if (v < minVal || v > maxVal) continue;
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	// 绘制刻度文字
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	for (double v : gridValues)
	{
		if (v < minVal || v > maxVal) continue;
		int gy = valueToY(v);
		CString label;
		label.Format(_T("%d"), static_cast<int>(v));
		CSize sz = memDC.GetTextExtent(label);
		memDC.DrawText(label, CRect(x + g_data.RDPI(2), gy - sz.cy / 2, x + g_data.RDPI(30), gy + sz.cy / 2), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
	}

	// 计算可见 K 线范围（参考 PrepareKLineDrawData 的逻辑，但简单使用全部数据）
	const int minBarWidth = 7;
	const int gap = 1;
	int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
	int maxVisibleKlines = min(displayCount, width / (minBarWidth + gap));
	int scrollRange = max(0, displayCount - maxVisibleKlines);
	int scrollPos = min(m_scrollOffset, scrollRange);
	int finalStartIndex = max(0, static_cast<int>(klineData.size()) - maxVisibleKlines - scrollPos);
	int barWidth = max(minBarWidth, (width - gap * (maxVisibleKlines - 1)) / maxVisibleKlines);

	int endIndex = min(static_cast<int>(klineData.size()), finalStartIndex + maxVisibleKlines);

	auto indexToX = [&](int i) {
		return x + (i - finalStartIndex) * (barWidth + gap) + barWidth / 2;
		};

	// 绘制 K 线（绿/红，参考 DrawKLineBars）
	for (int i = finalStartIndex; i < endIndex; i++)
	{
		const auto& item = klineData[i];
		if (item.high <= 0 || item.low <= 0 || item.close <= 0 || item.open <= 0)
			continue;

		int barX = x + (i - finalStartIndex) * (barWidth + gap);
		bool isUp = (item.close >= item.open);
		COLORREF color = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		// 简化：仅绘制 K 线的影线和实体
		STOCK::Price highest = item.high;
		STOCK::Price lowest = item.low;
		STOCK::Price openP = item.open;
		STOCK::Price closeP = item.close;

		int highY = valueToY(highest);
		int lowY = valueToY(lowest);
		int openY = valueToY(openP);
		int closeY = valueToY(closeP);

		CPen barPen(PS_SOLID, 1, color);
		memDC.SelectObject(&barPen);
		// 影线
		memDC.MoveTo(barX + barWidth / 2, highY);
		memDC.LineTo(barX + barWidth / 2, lowY);

		// 实体
		CBrush barBrush(color);
		CRect bodyRect(barX, (std::min)(openY, closeY), barX + barWidth, (std::max)(openY, closeY) + 1);
		if (bodyRect.Height() < 1) bodyRect.bottom = bodyRect.top + 1;
		memDC.FillRect(bodyRect, &barBrush);
	}

	// 绘制 K、D、J 三条曲线
	auto drawLine = [&](int lineIdx, COLORREF color, double KDJData::* field) {
		CPen linePen(PS_SOLID, 1, color);
		memDC.SelectObject(&linePen);
		bool first = true;
		for (int i = finalStartIndex; i < endIndex; i++)
		{
			if (i >= static_cast<int>(kdjData.size()) || !kdjData[i].valid)
				continue;
			int px = indexToX(i);
			int py = valueToY(kdjData[i].*field);
			if (first)
			{
				memDC.MoveTo(px, py);
				first = false;
			}
			else
			{
				memDC.LineTo(px, py);
			}
		}
		};

	// K 线：快线，红色
	drawLine(0, COLOR_RED_UP, &KDJData::k);
	// D 线：慢线，蓝色
	drawLine(1, RGB(0, 68, 204), &KDJData::d);
	// J 线：紫色（使用 COLOR_GRAY_PURPLE）
	drawLine(2, COLOR_GRAY_PURPLE, &KDJData::j);

	memDC.SelectObject(pOldPen);

	// 绘制标题
	CString title = _T("KDJ");
	memDC.SetTextColor(COLOR_BLACK);
	memDC.DrawText(title, CRect(x + g_data.RDPI(2), y, x + width, y + g_data.RDPI(16)), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	memDC.SetBkMode(oldBkMode);
}

void CFloatingWnd::DrawBollBands(CDC& memDC, const KLineDrawData& drawData)
{
	const auto& klineData = *drawData.klineData;
	const int N = 20;       // 布林带周期
	const int K = 2;        // 标准差倍数

	int endIndex = static_cast<int>(klineData.size());
	int beginIndex = drawData.finalStartIndex;
	if (endIndex <= beginIndex)
		return;

	// 计算布林带：MA20 ± 2σ
	std::vector<double> upperBand(endIndex, 0);
	std::vector<double> middleBand(endIndex, 0);
	std::vector<double> lowerBand(endIndex, 0);

	for (int i = beginIndex; i < endIndex; i++)
	{
		if (i < N - 1)
		{
			// 数据不足，无法计算
			upperBand[i] = middleBand[i] = lowerBand[i] = 0;
			continue;
		}
		double sum = 0;
		for (int j = i - N + 1; j <= i; j++)
		{
			sum += klineData[j].close;
		}
		double ma = sum / N;
		double variance = 0;
		for (int j = i - N + 1; j <= i; j++)
		{
			double diff = klineData[j].close - ma;
			variance += diff * diff;
		}
		double stddev = std::sqrt(variance / N);
		middleBand[i] = ma;
		upperBand[i] = ma + K * stddev;
		lowerBand[i] = ma - K * stddev;
	}

	// 使用 drawData 的 unitY 进行坐标转换
	auto priceToY = [&](double price) {
		return drawData.y + static_cast<int>((drawData.maxPrice - price) * drawData.unitY);
		};

	// 绘制上轨（金色）
	auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
		CPen bandPen(PS_SOLID, 1, color);
		memDC.SelectObject(&bandPen);
		bool first = true;
		for (int i = drawData.finalStartIndex; i < endIndex; i++)
		{
			if (band[i] <= 0) continue;
			int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap) + drawData.barWidth / 2;
			int py = priceToY(band[i]);
			if (first)
			{
				memDC.MoveTo(barX, py);
				first = false;
			}
			else
			{
				memDC.LineTo(barX, py);
			}
		}
		};

	drawBandLine(upperBand, COLOR_GOLDEN);
	drawBandLine(middleBand, COLOR_GRAY_PURPLE);
	drawBandLine(lowerBand, COLOR_GOLDEN);

	// 填充上下轨之间的区域（浅色）
	CPen nullPen(PS_NULL, 1, RGB(0, 0, 0));
	CPen* pOldFillPen = memDC.SelectObject(&nullPen);
	int prevUpperY = -1;
	int prevLowerY = -1;
	int prevX = -1;
	for (int i = drawData.finalStartIndex; i < endIndex; i++)
	{
		if (upperBand[i] <= 0 || lowerBand[i] <= 0) continue;
		int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap) + drawData.barWidth / 2;
		int upperY = priceToY(upperBand[i]);
		int lowerY = priceToY(lowerBand[i]);
		if (prevUpperY >= 0 && prevLowerY >= 0 && prevX >= 0)
		{
			// 绘制梯形填充（用4个点构成的多边形）
			CBrush fillBrush(RGB(240, 240, 220));
			CPoint pts[4] = {
				CPoint(prevX, prevUpperY),
				CPoint(barX, upperY),
				CPoint(barX, lowerY),
				CPoint(prevX, prevLowerY)
			};
			CBrush* pOldBrush = memDC.SelectObject(&fillBrush);
			memDC.Polygon(pts, 4);
			memDC.SelectObject(pOldBrush);
		}
		prevUpperY = upperY;
		prevLowerY = lowerY;
		prevX = barX;
	}
	memDC.SelectObject(pOldFillPen);
}

void CFloatingWnd::DrawMin5HourLines(CDC& memDC, const KLineDrawData& drawData)
{
	// 清空之前的标签
	m_min5HourLabels.clear();

	const auto& klineData = *drawData.klineData;
	if (klineData.empty())
		return;

	// 在5分钟K线模式下，K线数据通常是5分钟间隔
	// 我们在整点位置（9:30, 10:00, 10:30, ...）绘制垂直网格线和时间标签
	// 假设 klineData[i].day 字段在5分钟模式下存放时间字符串 "HH:MM"
	// 如果 day 字段是日期，则跳过此绘制

	CPen hourPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&hourPen);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);
	memDC.SetTextColor(COLOR_GRAY_TEXT);

	int lastHour = -1;
	for (int i = drawData.finalStartIndex; i < static_cast<int>(klineData.size()); i++)
	{
		const auto& item = klineData[i];
		// 解析 day 字段，判断是否包含时间信息
		// 5分钟K线模式下 day 格式可能是 "YYYY-MM-DD HH:MM" 或仅 "HH:MM"
		const std::string& dayStr = item.day;
		if (dayStr.empty()) continue;

		// 尝试提取小时部分
		int hour = -1;
		size_t spacePos = dayStr.find(' ');
		if (spacePos != std::string::npos && dayStr.length() >= spacePos + 3)
		{
			// "YYYY-MM-DD HH:MM" 格式
			hour = atoi(dayStr.substr(spacePos + 1, 2).c_str());
		}
		else if (dayStr.length() >= 5 && dayStr[2] == ':')
		{
			// "HH:MM" 格式
			hour = atoi(dayStr.substr(0, 2).c_str());
		}

		if (hour < 0) continue;

		// 在整点（且非第一根）绘制网格线
		if (hour != lastHour && lastHour >= 0)
		{
			int barX = drawData.x + (i - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
			memDC.MoveTo(barX, drawData.y);
			memDC.LineTo(barX, drawData.y + drawData.h);

			// 时间标签
			CString label;
			label.Format(_T("%d:00"), hour);
			int labelY = drawData.y + drawData.h + g_data.RDPI(2);
			CSize sz = memDC.GetTextExtent(label);
			int labelX = barX - sz.cx / 2;
			labelX = max(g_data.RDPI(2), min(labelX, drawData.x + drawData.w - sz.cx - g_data.RDPI(2)));
			memDC.TextOut(labelX, labelY, label);

			m_min5HourLabels.emplace_back(barX, label);
		}
		lastHour = hour;
	}

	memDC.SetBkMode(oldBkMode);
	memDC.SelectObject(pOldPen);
}

void CFloatingWnd::DrawTimelineTitleBars(CDC& memDC, const TimelineDrawContext& ctx, int priceChartTop, int volumeChartTop, int macdChartTop, int timelineTitleHeight)
{
	// 在走势图、量柱图、MACD 图三个区域顶部绘制标题栏（使用原始 top，位于绘图区上方）
	const auto& timelinePoint = *ctx.timelinePoint;
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	// 走势图标题栏
	CRect priceTitleRect(0, priceChartTop, ctx.chartWidth, priceChartTop + timelineTitleHeight);
	memDC.FillSolidRect(priceTitleRect, RGB(245, 245, 245));

	if (!m_timelinePriceTitleTip.IsEmpty())
	{
		memDC.SetTextColor(COLOR_BLACK);
		memDC.DrawText(m_timelinePriceTitleTip, priceTitleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
	}
	else if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode && ctx.klineData && !ctx.klineData->empty())
	{
		// 日K线模式：显示开/收/高/低
		int xPos = g_data.RDPI(4);
		int centerY = priceChartTop + timelineTitleHeight / 2;

		const auto& klineData = *ctx.klineData;
		int klineIdx = -1;
		if (m_hoveredBarIndex >= 0 && m_isHoveringVolume)
		{
			klineIdx = ctx.startIndex + m_hoveredBarIndex;
		}
		else
		{
			klineIdx = ctx.startIndex + static_cast<int>(timelinePoint.size()) - 1;
		}
		if (klineIdx < 0 || klineIdx >= static_cast<int>(klineData.size()))
			klineIdx = static_cast<int>(klineData.size()) - 1;

		if (klineIdx >= 0 && klineIdx < static_cast<int>(klineData.size()))
		{
			const auto& kp = klineData[klineIdx];
			STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

			auto drawKLineLabel = [&](const CString& label, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
				CString valStr = CCommon::FormatFloat(value);
				memDC.SetTextColor(labelColor);
				CSize ls = memDC.GetTextExtent(label);
				memDC.TextOut(xPos, centerY - ls.cy / 2, label);
				xPos += ls.cx;
				memDC.SetTextColor(valueColor);
				CSize vs = memDC.GetTextExtent(valStr);
				memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
				xPos += vs.cx + g_data.RDPI(4);
				};

			drawKLineLabel(_T("开:"), kp.open, COLOR_BLACK, (kp.open >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("收:"), kp.close, COLOR_BLACK, (kp.close >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("高:"), kp.high, COLOR_BLACK, (kp.high >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("低:"), kp.low, COLOR_BLACK, (kp.low >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
		}
	}
	else if ((m_isMin5KLineMode || m_isMin30KLineMode) && ctx.klineData && !ctx.klineData->empty())
	{
		// 5分钟/30分钟K线模式：显示开/收/高/低
		int xPos = g_data.RDPI(4);
		int centerY = priceChartTop + timelineTitleHeight / 2;

		// 获取悬停点或最后一个K线数据
		const auto& klineData = *ctx.klineData;
		int klineIdx = -1;
		bool isHovering = (m_hoveredBarIndex >= 0 && m_isHoveringVolume);
		if (isHovering)
		{
			klineIdx = ctx.startIndex + m_hoveredBarIndex;
		}
		else
		{
			klineIdx = ctx.startIndex + static_cast<int>(timelinePoint.size()) - 1;
		}
		if (klineIdx < 0 || klineIdx >= static_cast<int>(klineData.size()))
			klineIdx = static_cast<int>(klineData.size()) - 1;

		if (klineIdx >= 0 && klineIdx < static_cast<int>(klineData.size()))
		{
			const auto& kp = klineData[klineIdx];
			STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

			auto drawKLineLabel = [&](const CString& label, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
				CString valStr = CCommon::FormatFloat(value);
				memDC.SetTextColor(labelColor);
				CSize ls = memDC.GetTextExtent(label);
				memDC.TextOut(xPos, centerY - ls.cy / 2, label);
				xPos += ls.cx;
				memDC.SetTextColor(valueColor);
				CSize vs = memDC.GetTextExtent(valStr);
				memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
				xPos += vs.cx + g_data.RDPI(4);
				};

			drawKLineLabel(_T("开:"), kp.open, COLOR_BLACK, (kp.open >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("收:"), kp.close, COLOR_BLACK, (kp.close >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("高:"), kp.high, COLOR_BLACK, (kp.high >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("低:"), kp.low, COLOR_BLACK, (kp.low >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
		}
	}
	else if (!timelinePoint.empty())
	{
		STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

		// 判断是否hover：hover时使用hover点的数据
		bool isHovering = (m_hoveredBarIndex >= 0 && m_isHoveringVolume);
		STOCK::Price dispAvgPrice = isHovering ? m_hoveredData.averagePrice : timelinePoint.back().averagePrice;
		if (dispAvgPrice <= 0)
			dispAvgPrice = isHovering ? m_hoverMa1 : ctx.ma1;

		int xPos = g_data.RDPI(4);
		int centerY = priceChartTop + timelineTitleHeight / 2;

		// 辅助lambda：绘制"标签:数值"，标签和数值分别着色
		auto drawLabelValue = [&](const CString& labelText, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
			CString valStr = CCommon::FormatFloat(value);
			memDC.SetTextColor(labelColor);
			CSize ls = memDC.GetTextExtent(labelText);
			memDC.TextOut(xPos, centerY - ls.cy / 2, labelText);
			xPos += ls.cx;
			memDC.SetTextColor(valueColor);
			CSize vs = memDC.GetTextExtent(valStr);
			memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
			xPos += vs.cx + g_data.RDPI(4);
			};

		// 与昨收比较的颜色
		auto cmpPrevClose = [prevClose](STOCK::Price p) -> COLORREF {
			if (prevClose <= 0) return COLOR_BLACK;
			if (p > prevClose) return COLOR_RED_UP;
			if (p < prevClose) return COLOR_GREEN_DOWN;
			return COLOR_BLACK;
			};
		// 均价
		drawLabelValue(_T("均价:"), dispAvgPrice, COLOR_BLACK, cmpPrevClose(dispAvgPrice));

		// 分时模式：在标题栏正中间显示实时指标信号指示器
		{
			const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
			if (fullData.size() >= 26)
			{
				int signalEndIndex = -1;
				if (isHovering)
				{
					int hoverGlobalIdx = ctx.startIndex + m_hoveredBarIndex;
					if (hoverGlobalIdx >= 25 && hoverGlobalIdx < static_cast<int>(fullData.size()))
						signalEndIndex = hoverGlobalIdx;
				}

				auto rtSig = CSignalAnalyzer::CalcRealtimeSignalsFromTimeline(fullData, signalEndIndex);

				// 信号颜色：按强度分3档，由浅到深
				static const COLORREF BUY_COLORS[] = {
					RGB(40, 240, 40),   // 1档浅绿
					RGB(50, 180, 50),   // 2档中绿
					RGB(20, 130, 40)    // 3档深绿
				};
				static const COLORREF SELL_COLORS[] = {
					RGB(240, 40, 40),   // 1档浅红
					RGB(180, 50, 50),   // 2档中红
					RGB(130, 20, 40)    // 3档深红
				};

				// 构建信号项列表：{文字, 颜色}，顺序：MACD / BOLL / KDJ / RSI / W&R
				std::vector<std::pair<CString, COLORREF>> sigItems;
				if (rtSig.macd != 0) sigItems.push_back({ rtSig.macd == -1 ? _T("M\u2193") : _T("M\u2191"), rtSig.macd == -1 ? BUY_COLORS[rtSig.macdStr - 1] : SELL_COLORS[rtSig.macdStr - 1] });
				if (rtSig.boll != 0) sigItems.push_back({ rtSig.boll == -1 ? _T("B\u2193") : _T("B\u2191"), rtSig.boll == -1 ? BUY_COLORS[rtSig.bollStr - 1] : SELL_COLORS[rtSig.bollStr - 1] });
				if (rtSig.kdj != 0) sigItems.push_back({ rtSig.kdj == -1 ? _T("K\u2193") : _T("K\u2191"), rtSig.kdj == -1 ? BUY_COLORS[rtSig.kdjStr - 1] : SELL_COLORS[rtSig.kdjStr - 1] });
				if (rtSig.rsi != 0) sigItems.push_back({ rtSig.rsi == -1 ? _T("R\u2193") : _T("R\u2191"), rtSig.rsi == -1 ? BUY_COLORS[rtSig.rsiStr - 1] : SELL_COLORS[rtSig.rsiStr - 1] });
				if (rtSig.wr != 0) sigItems.push_back({ rtSig.wr == -1 ? _T("W\u2193") : _T("W\u2191"), rtSig.wr == -1 ? BUY_COLORS[rtSig.wrStr - 1] : SELL_COLORS[rtSig.wrStr - 1] });

				if (!sigItems.empty())
				{
					// 风险校验：有信号时调用风控检测
					CString riskText;
					COLORREF riskColor = COLOR_BLACK;
					{
						auto stockData = g_data.GetStockData(m_stock_id);
						if (stockData)
						{
							auto min5KLineObj = stockData->getMin5KLineData();
							auto min30KLineObj = stockData->getMin30KLineData();
							if (min5KLineObj && min5KLineObj->data.size() >= 60 && min30KLineObj && min30KLineObj->data.size() >= 2)
							{
								std::vector<STOCK::Bar> bars5;
								bars5.reserve(min5KLineObj->data.size());
								for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));
								std::vector<STOCK::Bar> bars30;
								bars30.reserve(min30KLineObj->data.size());
								for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));
								STOCK::TrendState30m trendState = CSignalAnalyzer::Get30mTrendState(bars30);
								CString reason = CSignalAnalyzer::GetForbidReason(bars5, trendState);
								if (reason.IsEmpty())
								{
									riskText = _T("\u221A");  // √
									// 颜色跟随信号方向：有买入信号用绿色，否则用红色
									bool hasBuy = (rtSig.boll == -1 || rtSig.macd == -1 || rtSig.rsi == -1 || rtSig.kdj == -1 || rtSig.wr == -1);
									riskColor = hasBuy ? BUY_COLORS[0] : SELL_COLORS[0];
								}
								else
								{
									riskText = _T("\u00D7") + reason;  // ×风险原因
									riskColor = RGB(180, 80, 0);  // 橙色警告
								}
							}
						}
					}

					// 计算总宽度（含风险标记），右对齐显示
					int totalW = 0;
					for (const auto& item : sigItems)
						totalW += memDC.GetTextExtent(item.first).cx;
					if (!riskText.IsEmpty())
						totalW += memDC.GetTextExtent(riskText).cx;
					int sigX = ctx.chartWidth - totalW - g_data.RDPI(4);

					for (const auto& item : sigItems)
					{
						memDC.SetTextColor(item.second);
						CSize sz = memDC.GetTextExtent(item.first);
						memDC.TextOut(sigX, centerY - sz.cy / 2, item.first);
						sigX += sz.cx;
					}
					if (!riskText.IsEmpty())
					{
						memDC.SetTextColor(riskColor);
						CSize sz = memDC.GetTextExtent(riskText);
						memDC.TextOut(sigX, centerY - sz.cy / 2, riskText);
					}
				}
			}
		}
	}

	if (m_isKLineMode && !timelinePoint.empty())
	{
		bool isHovering = (m_hoveredBarIndex >= 0 && m_isHoveringVolume);
		int displayIdx = isHovering ? m_hoveredBarIndex : static_cast<int>(timelinePoint.size()) - 1;
		displayIdx = max(0, min(displayIdx, static_cast<int>(timelinePoint.size()) - 1));

		const int rightPadding = g_data.RDPI(4);
		const int itemGap = g_data.RDPI(6);
		int centerY = priceChartTop + timelineTitleHeight / 2;

		auto formatPrice = [](STOCK::Price value) -> CString {
			return CCommon::FormatFloat(value);
			};
		auto drawRightLabelValues = [&](const std::vector<std::pair<CString, COLORREF>>& items) {
			if (items.empty())
				return;

			int totalWidth = 0;
			for (const auto& item : items)
				totalWidth += memDC.GetTextExtent(item.first).cx + itemGap;
			totalWidth -= itemGap;

			int xPos = ctx.chartWidth - rightPadding - totalWidth;
			xPos = max(g_data.RDPI(4), xPos);
			for (const auto& item : items)
			{
				memDC.SetTextColor(item.second);
				CSize sz = memDC.GetTextExtent(item.first);
				memDC.TextOut(xPos, centerY - sz.cy / 2, item.first);
				xPos += sz.cx + itemGap;
			}
			};

		if (m_isMin5KLineMode)
		{
			// 5分钟K线模式：在标题栏正中间显示实时指标信号指示器
			auto stockData = g_data.GetStockData(m_stock_id);
			if (stockData)
			{
				auto min5KLineObj = stockData->getMin5KLineData();
				if (min5KLineObj && min5KLineObj->data.size() >= 26)
				{
					std::vector<STOCK::Bar> bars5;
					bars5.reserve(min5KLineObj->data.size());
					for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));

					// 鼠标悬停时计算悬停点的信号，否则计算最新K线的信号
					int signalEndIndex = -1;
					if (isHovering)
					{
						int hoverKlineIdx = ctx.startIndex + m_hoveredBarIndex;
						if (hoverKlineIdx >= 25 && hoverKlineIdx < static_cast<int>(bars5.size()))
							signalEndIndex = hoverKlineIdx;
					}

					auto rtSig = CSignalAnalyzer::CalcRealtimeSignals(bars5, signalEndIndex);

					// 信号颜色：按强度分3档，由浅到深
					static const COLORREF BUY_COLORS[] = {
						RGB(40, 240, 40),   // 1档浅绿
						RGB(50, 180, 50),   // 2档中绿
						RGB(20, 130, 40)    // 3档深绿
					};
					static const COLORREF SELL_COLORS[] = {
						RGB(240, 40, 40),   // 1档浅红
						RGB(180, 50, 50),   // 2档中红
						RGB(130, 20, 40)    // 3档深红
					};

					// 构建信号项列表：{文字, 颜色}，顺序：MACD / BOLL / KDJ / RSI / W&R
					std::vector<std::pair<CString, COLORREF>> sigItems;
					if (rtSig.macd != 0) sigItems.push_back({ rtSig.macd == -1 ? _T("M\u2193") : _T("M\u2191"), rtSig.macd == -1 ? BUY_COLORS[rtSig.macdStr - 1] : SELL_COLORS[rtSig.macdStr - 1] });
					if (rtSig.boll != 0) sigItems.push_back({ rtSig.boll == -1 ? _T("B\u2193") : _T("B\u2191"), rtSig.boll == -1 ? BUY_COLORS[rtSig.bollStr - 1] : SELL_COLORS[rtSig.bollStr - 1] });
					if (rtSig.kdj != 0) sigItems.push_back({ rtSig.kdj == -1 ? _T("K\u2193") : _T("K\u2191"), rtSig.kdj == -1 ? BUY_COLORS[rtSig.kdjStr - 1] : SELL_COLORS[rtSig.kdjStr - 1] });
					if (rtSig.rsi != 0) sigItems.push_back({ rtSig.rsi == -1 ? _T("R\u2193") : _T("R\u2191"), rtSig.rsi == -1 ? BUY_COLORS[rtSig.rsiStr - 1] : SELL_COLORS[rtSig.rsiStr - 1] });
					if (rtSig.wr != 0) sigItems.push_back({ rtSig.wr == -1 ? _T("W\u2193") : _T("W\u2191"), rtSig.wr == -1 ? BUY_COLORS[rtSig.wrStr - 1] : SELL_COLORS[rtSig.wrStr - 1] });

					if (!sigItems.empty())
					{
						// 风险校验：有信号时调用风控检测
						CString riskText;
						COLORREF riskColor = COLOR_BLACK;
						{
							auto min30KLineObj = stockData->getMin30KLineData();
							if (min30KLineObj && min30KLineObj->data.size() >= 2 && bars5.size() >= 60)
							{
								std::vector<STOCK::Bar> bars30;
								bars30.reserve(min30KLineObj->data.size());
								for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));
								STOCK::TrendState30m trendState = CSignalAnalyzer::Get30mTrendState(bars30);
								CString reason = CSignalAnalyzer::GetForbidReason(bars5, trendState);
								if (reason.IsEmpty())
								{
									riskText = _T("\u221A");  // √
									bool hasBuy = (rtSig.boll == -1 || rtSig.macd == -1 || rtSig.rsi == -1 || rtSig.kdj == -1 || rtSig.wr == -1);
									riskColor = hasBuy ? BUY_COLORS[0] : SELL_COLORS[0];
								}
								else
								{
									riskText = _T("\u00D7") + reason;  // ×风险原因
									riskColor = RGB(180, 80, 0);  // 橙色警告
								}
							}
						}

						// 计算总宽度，右对齐显示
						int totalW = 0;
						for (const auto& item : sigItems)
							totalW += memDC.GetTextExtent(item.first).cx;
						if (!riskText.IsEmpty())
							totalW += memDC.GetTextExtent(riskText).cx;
						int xPos = ctx.chartWidth - totalW - g_data.RDPI(4);

						for (const auto& item : sigItems)
						{
							memDC.SetTextColor(item.second);
							CSize sz = memDC.GetTextExtent(item.first);
							memDC.TextOut(xPos, centerY - sz.cy / 2, item.first);
							xPos += sz.cx;
						}
						if (!riskText.IsEmpty())
						{
							memDC.SetTextColor(riskColor);
							CSize sz = memDC.GetTextExtent(riskText);
							memDC.TextOut(xPos, centerY - sz.cy / 2, riskText);
						}
					}
				}
			}
		}
		else
		{
			// 日K线/30分钟K线模式：右侧显示MA5/MA10/MA20或BOLL上中下轨
			if (m_showMA)
			{
				STOCK::Price dispMa5 = isHovering ? m_hoverMa5 : ctx.ma5;
				STOCK::Price dispMa10 = isHovering ? m_hoverMa10 : ctx.ma10;
				STOCK::Price dispMa20 = isHovering ? m_hoverMa20 : ctx.ma20;
				std::vector<std::pair<CString, COLORREF>> items;
				if (dispMa5 > 0) items.push_back({ _T("MA5:") + formatPrice(dispMa5), RGB(0, 0, 230) });
				if (dispMa10 > 0) items.push_back({ _T("MA10:") + formatPrice(dispMa10), RGB(0, 166, 235) });
				if (dispMa20 > 0) items.push_back({ _T("MA20:") + formatPrice(dispMa20), RGB(169, 102, 186) });
				drawRightLabelValues(items);
			}
			else if (m_showBollBands)
			{
				const int N = 20;
				const int K = 2;
				const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
				int globalIdx = ctx.startIndex + displayIdx;
				if (globalIdx >= N - 1 && globalIdx < static_cast<int>(fullData.size()))
				{
					double sum = 0;
					for (int i = globalIdx - N + 1; i <= globalIdx; i++)
						sum += fullData[i].price;
					double mid = sum / N;

					double variance = 0;
					for (int i = globalIdx - N + 1; i <= globalIdx; i++)
					{
						double diff = fullData[i].price - mid;
						variance += diff * diff;
					}
					double stddev = std::sqrt(variance / N);
					double upper = mid + K * stddev;
					double lower = mid - K * stddev;

					std::vector<std::pair<CString, COLORREF>> items;
					items.push_back({ _T("上:") + formatPrice(static_cast<STOCK::Price>(upper)), COLOR_RED_UP });
					items.push_back({ _T("中:") + formatPrice(static_cast<STOCK::Price>(mid)), RGB(0, 0, 230) });
					items.push_back({ _T("下:") + formatPrice(static_cast<STOCK::Price>(lower)), COLOR_GREEN_DOWN });
					drawRightLabelValues(items);
				}
			}
		}
	}

	CRect volumeTitleRect(0, volumeChartTop, ctx.chartWidth, volumeChartTop + timelineTitleHeight);
	memDC.FillSolidRect(volumeTitleRect, RGB(245, 245, 245));
	// 分量和分额：悬停时显示鼠标指向位置，否则显示最新时间点
	CString perVolStr, perAmtStr;
	bool isVolHovering = !m_timelineVolumeTitleTip.IsEmpty();
	int displayVolIdx = isVolHovering ? m_hoveredBarIndex : static_cast<int>(timelinePoint.size()) - 1;
	displayVolIdx = max(0, min(displayVolIdx, static_cast<int>(timelinePoint.size()) - 1));
	if (displayVolIdx >= 0 && displayVolIdx < static_cast<int>(timelinePoint.size()))
	{
		const auto& displayPt = timelinePoint[displayVolIdx];
		perVolStr = CCommon::FormatVolumeInt(displayPt.volume / 100);
		double displayAmount = static_cast<double>(displayPt.volume) * displayPt.price;
		perAmtStr = CCommon::FormatAmount(displayAmount);
	}

	auto calcVolumeMA = [&](int globalIndex, int period) -> double {
		if (!ctx.fullTimeline || globalIndex < period - 1 || globalIndex >= static_cast<int>(ctx.fullTimeline->size()))
			return 0.0;

		double sum = 0.0;
		for (int i = globalIndex - period + 1; i <= globalIndex; i++)
			sum += static_cast<double>((*ctx.fullTimeline)[i].volume);
		return sum / period;
		};

	auto formatVolumeMA = [](double volume) -> CString {
		return CCommon::FormatVolumeInt(volume / 100.0);
		};

	const int globalVolIdx = ctx.startIndex + displayVolIdx;
	double volMa5 = calcVolumeMA(globalVolIdx, 5);
	double prevVolMa5 = calcVolumeMA(globalVolIdx - 1, 5);
	double volMa10 = calcVolumeMA(globalVolIdx, 10);
	double prevVolMa10 = calcVolumeMA(globalVolIdx - 1, 10);

	// 逐段绘制，分量/分额数值部分浅蓝色背景高亮
	int xPos = g_data.RDPI(4);
	int centerY = volumeChartTop + timelineTitleHeight / 2;
	const COLORREF hoverBgColor = RGB(200, 220, 255);

	auto drawLabelValueHL = [&](const CString& labelText, const CString& valStr, COLORREF labelColor, COLORREF valueColor, bool highlight) {
		memDC.SetTextColor(labelColor);
		CSize ls = memDC.GetTextExtent(labelText);
		memDC.TextOut(xPos, centerY - ls.cy / 2, labelText);
		xPos += ls.cx;
		memDC.SetTextColor(valueColor);
		CSize vs = memDC.GetTextExtent(valStr);
		if (highlight)
		{
			CRect hlRect(xPos, volumeChartTop + 1, xPos + vs.cx, volumeChartTop + timelineTitleHeight - 1);
			memDC.FillSolidRect(hlRect, hoverBgColor);
		}
		memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
		xPos += vs.cx + g_data.RDPI(4);
		};

	auto drawVolumeMA = [&](const CString& labelText, double maValue, double prevMaValue, COLORREF maColor) {
		if (maValue <= 0)
			return;

		CString valueText = formatVolumeMA(maValue);
		memDC.SetTextColor(maColor);
		CString text = labelText + valueText;
		CSize textSize = memDC.GetTextExtent(text);
		memDC.TextOut(xPos, centerY - textSize.cy / 2, text);
		xPos += textSize.cx;

		if (prevMaValue > 0 && maValue != prevMaValue)
		{
			CString arrowText = maValue > prevMaValue ? _T("↑") : _T("↓");
			memDC.SetTextColor(maValue > prevMaValue ? COLOR_RED_UP : COLOR_GREEN_DOWN);
			CSize arrowSize = memDC.GetTextExtent(arrowText);
			memDC.TextOut(xPos, centerY - arrowSize.cy / 2, arrowText);
			xPos += arrowSize.cx;
		}
		xPos += g_data.RDPI(4);
		};

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (!perVolStr.IsEmpty())
		drawLabelValueHL(_T("分量:"), perVolStr, COLOR_GRAY_TEXT, COLOR_GRAY_TEXT, isVolHovering);
	if (!perAmtStr.IsEmpty())
		drawLabelValueHL(_T("分额:"), perAmtStr, COLOR_GRAY_TEXT, COLOR_GRAY_TEXT, isVolHovering);
	if (m_isMin5KLineMode)
	{
		drawVolumeMA(_T("MA5:"), volMa5, prevVolMa5, RGB(0, 0, 230));
		drawVolumeMA(_T("MA10:"), volMa10, prevVolMa10, RGB(0, 166, 235));
	}

	// MACD/KDJ/W&R/RSI 图标题栏
	if (macdChartTop >= 0)
	{
		CRect macdTitleRect(0, macdChartTop, ctx.chartWidth, macdChartTop + timelineTitleHeight);
		memDC.FillSolidRect(macdTitleRect, RGB(245, 245, 245));
		if (m_timelineIndicator == TimelineIndicator::KDJ)
		{
			// KDJ标题栏：分段绘制，颜色与线条一致
			int xPos = g_data.RDPI(4);
			int centerY = macdChartTop + timelineTitleHeight / 2;
			auto drawKDJLabel = [&](const CString& label, const CString& value, COLORREF color) {
				memDC.SetTextColor(color);
				CString text = label + value;
				CSize ts = memDC.GetTextExtent(text);
				memDC.TextOut(xPos, centerY - ts.cy / 2, text);
				xPos += ts.cx + g_data.RDPI(4);
				};
			if (!m_timelineKdjTitleTip.IsEmpty())
			{
				// 悬停提示格式: K:xx D:xx J:xx，按空格分段着色
				CString tip = m_timelineKdjTitleTip;
				int pos = 0;
				CString token;
				COLORREF colors[] = { COLOR_RED_UP, RGB(0, 68, 204), RGB(0, 136, 34) };
				int colorIdx = 0;
				while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
				{
					drawKDJLabel(_T(""), token, colors[min(colorIdx, 2)]);
					colorIdx++;
				}
			}
			else
			{
				drawKDJLabel(_T("K:"), _T(""), COLOR_RED_UP);
				drawKDJLabel(_T("D:"), _T(""), RGB(0, 68, 204));
				drawKDJLabel(_T("J:"), _T(""), RGB(0, 136, 34));
			}
		}
		else if (m_timelineIndicator == TimelineIndicator::WR)
		{
			// WR标题栏：分段绘制，颜色与线条一致
			int xPos = g_data.RDPI(4);
			int centerY = macdChartTop + timelineTitleHeight / 2;
			auto drawWRLabel = [&](const CString& label, const CString& value, COLORREF color) {
				memDC.SetTextColor(color);
				CString text = label + value;
				CSize ts = memDC.GetTextExtent(text);
				memDC.TextOut(xPos, centerY - ts.cy / 2, text);
				xPos += ts.cx + g_data.RDPI(4);
				};
			if (!m_timelineWrTitleTip.IsEmpty())
			{
				CString tip = m_timelineWrTitleTip;
				int pos = 0;
				CString token;
				COLORREF colors[] = { RGB(0, 68, 204), RGB(204, 34, 34) };
				int colorIdx = 0;
				while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
				{
					drawWRLabel(_T(""), token, colors[min(colorIdx, 1)]);
					colorIdx++;
				}
			}
			else
			{
				drawWRLabel(_T("WR6:"), _T(""), RGB(0, 68, 204));
				drawWRLabel(_T("WR14:"), _T(""), RGB(204, 34, 34));
			}
		}
		else if (m_timelineIndicator == TimelineIndicator::RSI)
		{
			// RSI标题栏：分段绘制，颜色与线条一致
			int xPos = g_data.RDPI(4);
			int centerY = macdChartTop + timelineTitleHeight / 2;
			auto drawRSILabel = [&](const CString& label, const CString& value, COLORREF color) {
				memDC.SetTextColor(color);
				CString text = label + value;
				CSize ts = memDC.GetTextExtent(text);
				memDC.TextOut(xPos, centerY - ts.cy / 2, text);
				xPos += ts.cx + g_data.RDPI(4);
				};
			if (!m_timelineRsiTitleTip.IsEmpty())
			{
				CString tip = m_timelineRsiTitleTip;
				int pos = 0;
				CString token;
				COLORREF colors[] = { RGB(0, 68, 204), RGB(204, 34, 34) };
				int colorIdx = 0;
				while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
				{
					drawRSILabel(_T(""), token, colors[min(colorIdx, 1)]);
					colorIdx++;
				}
			}
			else
			{
				drawRSILabel(_T("RSI6:"), _T(""), RGB(0, 68, 204));
				drawRSILabel(_T("RSI14:"), _T(""), RGB(204, 34, 34));
			}
		}
		else
		{
			CString macdTitle = _T("MACD");
			if (!m_timelineMacdTitleTip.IsEmpty())
				macdTitle = m_timelineMacdTitleTip;
			memDC.SetTextColor(COLOR_GOLDEN);
			memDC.DrawText(macdTitle, macdTitleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
		}
	} // end if (macdChartTop >= 0)

	memDC.SetBkMode(oldBkMode);
}

void CFloatingWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (!m_isOverviewMode)
	{
		m_isOverviewMode = true;
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showChipPeak = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
	else
	{
		m_isOverviewMode = false;
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showChipPeak = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
		RequestData();
	}
}

void CFloatingWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	m_mousePos = point;

	// 拖动滚动处理
	if (m_isTimelineDragging || m_isKLineDragging)
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
					if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
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
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_isMin5KLineMode)
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
								tp.volume = kp.volume;
								timelinePoint.push_back(tp);
							}
						}
					}
					else if (m_isMin30KLineMode)
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
		else if (m_isKLineDragging)
		{
			dx = point.x - m_klineDragStartPos.x;
			// 每拖动一个 barWidth + gap 像素滚动一根K线
			const int minBarWidth = 7;
			const int gap = 1;
			int deltaBars = dx / (minBarWidth + gap);
			int newOffset = m_klineDragStartOffset - deltaBars;
			if (newOffset < 0) newOffset = 0;
			if (newOffset != m_scrollOffset)
			{
				m_scrollOffset = newOffset;
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
	bool isIndexKLine = isIndex && m_isKLineMode;
	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = rect.Width() - orderBookWidth;
	const int yAxisWidth = g_data.RDPI(50);
	const int stockListWidth = m_showStockList ? g_data.RDPI(65) : 0;
	const int chartLeft = stockListWidth + yAxisWidth;
	const int headerHeight = g_data.RDPI(26);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int indexBarHeight = g_data.RDPI(20);  // 底部大盘指数状态栏高度

	// 统一布局：标题栏 + 走势图(1/2) + 量柱图(1/4) + 副图(1/4) + 时间标签 + 底部指数状态栏
	int chartArea = rect.Height() - headerHeight - xAxisLabelHeight - indexBarHeight;
	int priceChartHeight = chartArea / 2;
	int volumeChartHeight = chartArea / 4;

	m_isHoveringVolume = false;
	int prevHoveredBarIndex = m_hoveredBarIndex;
	m_hoveredBarIndex = -1;
	bool prevHoveringKLine = m_isHoveringKLine;
	bool prevHoveringKLineVolume = m_isHoveringKLineVolume;
	int prevKlineHoveredBarIndex = m_klineHoveredBarIndex;
	m_isHoveringKLine = false;
	m_isHoveringKLineVolume = false;
	m_klineHoveredBarIndex = -1;

	if ((m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode) && false)  // 日K线模式现在走分时悬停逻辑
	{
		std::vector<STOCK::KLinePoint> klineData;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = g_data.GetStockData(m_stock_id);
			if (stockData)
			{
				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					klineData = klineObj->data;
				}
			}
		}

		if (!klineData.empty() && point.x >= chartLeft && point.x < chartWidth)
		{
			// 使用与绘制完全一致的参数计算（x从chartLeft开始，宽度为chartWidth-chartLeft）
			const int paddingY = g_data.RDPI(10);
			KLineDrawData drawData = PrepareKLineDrawData(chartLeft, headerHeight + paddingY, chartWidth - chartLeft, priceChartHeight - paddingY * 2, klineData);

			if (point.y >= headerHeight && point.y < headerHeight + priceChartHeight)
			{
				// 鼠标在K线图上 - 使用与绘制一致的参数定位
				int barIndex = -1;
				int totalBars = klineData.size() - drawData.finalStartIndex;
				if (totalBars > 0 && drawData.barWidth + drawData.gap > 0)
				{
					barIndex = drawData.finalStartIndex + (point.x - drawData.x) / (drawData.barWidth + drawData.gap);
					barIndex = max(drawData.finalStartIndex, min(barIndex, (int)klineData.size() - 1));
				}

				if (barIndex >= 0)
				{
					m_isHoveringKLine = true;
					m_klineHoveredBarIndex = barIndex;

					const auto& item = klineData[barIndex];
					m_klineHoverTip.Format(_T("开:%s  收:%s  高:%s  低:%s"),
						CCommon::FormatFloat(item.open),
						CCommon::FormatFloat(item.close),
						CCommon::FormatFloat(item.high),
						CCommon::FormatFloat(item.low));

					m_klineTrendHoverTip.Format(_T("收:%s  最高:%s  最低:%s"),
						CCommon::FormatFloat(item.close),
						CCommon::FormatFloat(item.high),
						CCommon::FormatFloat(item.low));

					// 同时设置量柱提示，实现同步显示
					STOCK::Volume volumeLots = item.volume / 100;
					CString volumeStr = CCommon::FormatVolumeInt(volumeLots);
					m_klineVolumeHoverTip.Format(_T("成交量:%s"),
						volumeStr);
				}
			}
			else
			{
				// 统一布局：成交量图紧贴走势图
				int volumeY = headerHeight + priceChartHeight;
				if (point.y >= volumeY && point.y < volumeY + volumeChartHeight)
				{
					// 鼠标在量柱图上 - 使用与绘制一致的参数定位
					int barIndex = -1;
					int totalBars = klineData.size() - drawData.finalStartIndex;
					if (totalBars > 0 && drawData.barWidth + drawData.gap > 0)
					{
						barIndex = drawData.finalStartIndex + (point.x - drawData.x) / (drawData.barWidth + drawData.gap);
						barIndex = max(drawData.finalStartIndex, min(barIndex, (int)klineData.size() - 1));
					}

					if (barIndex >= 0)
					{
						m_isHoveringKLineVolume = true;
						m_klineHoveredBarIndex = barIndex;

						const auto& item = klineData[barIndex];
						STOCK::Volume volumeLots = item.volume / 100;
						CString volumeStr = CCommon::FormatVolumeInt(volumeLots);
						m_klineVolumeHoverTip.Format(_T("成交量:%s"),
							volumeStr);

						// 同时设置K线提示，实现同步显示
						m_klineHoverTip.Format(_T("开:%s  收:%s  高:%s  低:%s"),
							CCommon::FormatFloat(item.open),
							CCommon::FormatFloat(item.close),
							CCommon::FormatFloat(item.high),
							CCommon::FormatFloat(item.low));

						m_klineTrendHoverTip.Format(_T("收:%s  最高:%s  最低:%s"),
							CCommon::FormatFloat(item.close),
							CCommon::FormatFloat(item.high),
							CCommon::FormatFloat(item.low));
					}
				}
			}

			// 只在悬停状态变化时重绘图表区域，避免按钮闪烁
			bool hoverChanged = (m_isHoveringKLine != prevHoveringKLine ||
				m_isHoveringKLineVolume != prevHoveringKLineVolume ||
				m_klineHoveredBarIndex != prevKlineHoveredBarIndex);
			if (hoverChanged)
			{
				InvalidateRect(CRect(0, headerHeight, chartWidth, rect.Height()));
			}
		}
	}
	else
	{
		std::vector<STOCK::TimelinePoint> timelinePoint;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = g_data.GetStockData(m_stock_id);
			if (stockData)
			{
				if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
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
							tp.averagePrice = kp.close;
							tp.volume = kp.volume;
							tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
							timelinePoint.push_back(tp);
						}
					}
				}
				else if (m_isMin5KLineMode)
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
							tp.averagePrice = kp.close;
							tp.volume = kp.volume;
							tp.amount = static_cast<STOCK::Amount>(kp.volume) * kp.close;
							timelinePoint.push_back(tp);
						}
					}
				}
				else if (m_isMin30KLineMode)
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
			CalcAllRollingAvgPrices(timelinePoint);
			auto subStart = timelinePoint.begin() + startIndex;
			auto subEnd = timelinePoint.begin() + startIndex + visibleCount;
			std::vector<STOCK::TimelinePoint> subTimeline(subStart, subEnd);

			// 鼠标坐标减去图表左边界，对应到分时图内部坐标
			int adjX = point.x - chartLeft;
			int effectiveWidth = chartWidth - chartLeft;
			// 按索引比例计算鼠标对应的可见数据索引
			int relIndex = static_cast<int>(adjX * static_cast<float>(visibleCount) / effectiveWidth);
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
				if (m_timelineIndicator == TimelineIndicator::KDJ)
				{
					auto kdjData = CalculateTimelineKDJ(subTimeline);
					if (relIndex < static_cast<int>(kdjData.size()) && kdjData[relIndex].valid)
					{
						m_timelineKdjTitleTip.Format(_T("K:%.1f D:%.1f J:%.1f"), kdjData[relIndex].k, kdjData[relIndex].d, kdjData[relIndex].j);
					}
					m_timelineMacdTitleTip.Empty();
					m_timelineWrTitleTip.Empty();
					m_timelineRsiTitleTip.Empty();
				}
				else if (m_timelineIndicator == TimelineIndicator::WR)
				{
					// WR悬停提示
					auto wrData = CalculateTimelineWR(subTimeline);
					if (relIndex < static_cast<int>(wrData.size()) && wrData[relIndex].valid)
					{
						m_timelineWrTitleTip.Format(_T("WR6:%.1f WR14:%.1f"), wrData[relIndex].wr1, wrData[relIndex].wr2);
					}
					m_timelineMacdTitleTip.Empty();
					m_timelineKdjTitleTip.Empty();
					m_timelineRsiTitleTip.Empty();
				}
				else if (m_timelineIndicator == TimelineIndicator::RSI)
				{
					auto rsiData = CalculateTimelineRSI(subTimeline);
					if (relIndex < static_cast<int>(rsiData.size()) && rsiData[relIndex].valid)
					{
						m_timelineRsiTitleTip.Format(_T("RSI6:%.1f RSI14:%.1f"), rsiData[relIndex].rsi1, rsiData[relIndex].rsi2);
					}
					m_timelineMacdTitleTip.Empty();
					m_timelineKdjTitleTip.Empty();
					m_timelineWrTitleTip.Empty();
				}
				else
				{
					auto macdData = CalculateTimelineMACD(subTimeline);
					if (relIndex < static_cast<int>(macdData.size()) && macdData[relIndex].valid)
					{
						m_timelineMacdTitleTip.Format(_T("DIF:%.3f DEA:%.3f MACD:%.3f"), macdData[relIndex].dif, macdData[relIndex].dea, macdData[relIndex].bar);
					}
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
}

void CFloatingWnd::RequestData()
{
	if (!m_is_thread_running)
	{
		m_pendingRequest = false;
		loading_state_txt = g_data.StringRes(IDS_LOADING).GetString();
		// CREATE_SUSPENDED 以便安全获取线程句柄并设置 m_bAutoDelete = FALSE，
		// 从而在 OnDestroy 中可对其等待，避免进程退出时仍卡在 HTTP 请求
		CWinThread* pThread = AfxBeginThread(NetworkThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
		if (pThread != nullptr)
		{
			pThread->m_bAutoDelete = FALSE;
			m_hNetworkThread = pThread->m_hThread;
			m_pNetworkThread = pThread;
			ResumeThread(m_hNetworkThread);
		}
	}
	else
	{
		// 线程正在运行，标记待请求，线程结束后会自动触发
		m_pendingRequest = true;
	}
}

void CFloatingWnd::ToggleKLineMode()
{
	m_isKLineMode = !m_isKLineMode;
	m_isOverviewMode = false;
	m_isMin5KLineMode = false;
	m_isMin30KLineMode = false;
	m_showBollBands = true;
	m_showAmplitudeBands = false;
	m_btnBoll.SetWindowText(_T("ZF"));
	m_scrollOffset = 0;
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	m_timelineVisibleCount = 40;  // 切回分时显示最新走势
	m_showTrendView = false;
	m_showChipPeak = m_isKLineMode;
	m_showMA = false;
	UpdateModeButtons();
	UpdatePeriodComboVisibility();

	m_isHoveringKLine = false;
	m_isHoveringKLineVolume = false;
	m_isHoveringVolume = false;
	m_klineHoveredBarIndex = -1;
	m_hoveredBarIndex = -1;
	m_klineHoverTip.Empty();
	m_hoverTip.Empty();
	m_klineTrendHoverTip.Empty();
	m_timelineVolumeTitleTip.Empty();
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();

	if (m_isKLineMode)
	{
		// 不再重置m_klineDataLoaded，因为已在启动时预加载
		EnsureChipPeakData();
	}
	Invalidate();
	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
}

void CFloatingWnd::UpdateModeButtons()
{
	if (m_btnTimeLine.GetSafeHwnd() && m_btnKLine.GetSafeHwnd())
	{
		if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			SafeSetButtonStyle(m_btnTimeLine, BS_FLAT);
			SafeSetButtonStyle(m_btnKLine, BS_DEFPUSHBUTTON);
		}
		else if (m_isMin5KLineMode)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			SafeSetButtonStyle(m_btnTimeLine, BS_FLAT);
			SafeSetButtonStyle(m_btnKLine, BS_FLAT);
		}
		else if (m_isMin30KLineMode)
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			SafeSetButtonStyle(m_btnTimeLine, BS_FLAT);
			SafeSetButtonStyle(m_btnKLine, BS_FLAT);
		}
		else
		{
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			SafeSetButtonStyle(m_btnTimeLine, BS_DEFPUSHBUTTON);
			SafeSetButtonStyle(m_btnKLine, BS_FLAT);
		}

		SafeSetButtonStyle(m_btnT0, m_showT0Markers ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnMA, m_showMA ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnMin5KLine, m_isMin5KLineMode ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnMin30KLine, m_isMin30KLineMode ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnBoll, (m_showBollBands || m_showAmplitudeBands) ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
		SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);

		SafeSetButtonStyle(m_btnExpand, m_expandedMode ? BS_FLAT : BS_DEFPUSHBUTTON);
		m_btnExpand.SetWindowText(m_expandedMode ? _T("△") : _T("▽"));

		m_btnToggleStockList.SetWindowText(m_showStockList ? _T("|>") : _T("<|"));
		SafeSetButtonStyle(m_btnToggleStockList, m_showStockList ? BS_FLAT : BS_DEFPUSHBUTTON);
		SafeShowWindow(m_btnToggleStockList, !m_isOverviewMode);

		// 缩放按钮在所有模式下显示（除总览模式外）
		SafeShowWindow(m_btnZoomOut, !m_isOverviewMode);
		SafeShowWindow(m_btnZoomIn, !m_isOverviewMode);
		// MACD/KDJ/WR指标按钮在所有模式下显示（除总览模式和放大模式外）
		bool showIndicatorBtns = !m_isOverviewMode && !m_expandedMode;
		SafeShowWindow(m_btnIndicatorMACD, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorKDJ, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorWR, showIndicatorBtns);
		SafeShowWindow(m_btnIndicatorRSI, showIndicatorBtns);
	}
}

void CFloatingWnd::UpdatePeriodComboVisibility()
{
	SafeShowWindow(m_btnT0, !m_isOverviewMode);
	SafeShowWindow(m_btnMA, !m_isOverviewMode);

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

	SafeShowWindow(m_btnBoll, !m_isOverviewMode);
	SafeShowWindow(m_btnChipPeak, !m_isOverviewMode);
	SafeShowWindow(m_btnOrderBook, !m_isOverviewMode);
}

BOOL CFloatingWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 分时图模式/5分钟K线模式/日K线模式：滚轮缩放可见数据点数
	if (!m_isOverviewMode)
	{
		const int minVisible = 40;   // 最大放大：显示40个数据点
		int maxVisible;              // 最小缩放上限：根据模式不同
		if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
			maxVisible = 750;        // 日K线：最多显示约3年
		else if (m_isMin5KLineMode || m_isMin30KLineMode)
			maxVisible = 480;        // 5分/30分K线
		else
			maxVisible = 240;        // 分时：1天240分钟
		// 获取实际数据点数
		int totalPoints = 0;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = g_data.GetStockData(m_stock_id);
			if (stockData)
			{
				if (m_isKLineMode && !m_isMin5KLineMode && !m_isMin30KLineMode)
				{
					auto klineObj = stockData->getKLineData();
					if (klineObj)
						totalPoints = static_cast<int>(klineObj->data.size());
				}
				else if (m_isMin5KLineMode)
				{
					auto min5KLineObj = stockData->getMin5KLineData();
					if (min5KLineObj)
						totalPoints = static_cast<int>(min5KLineObj->data.size());
				}
				else if (m_isMin30KLineMode)
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
			// 向上滚：放大（减少可见点数）
			newCount = max(minVisible, m_timelineVisibleCount - 20);
		}
		else
		{
			// 向下滚：缩小（增加可见点数）
			newCount = min(effectiveMax, m_timelineVisibleCount + 20);
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

	if (!m_isOverviewMode)
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

void CFloatingWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar != &m_hScrollBar)
	{
		CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
		return;
	}

	SCROLLINFO si = { sizeof(SCROLLINFO) };
	si.fMask = SIF_ALL;
	m_hScrollBar.GetScrollInfo(&si);

	int newPos = si.nPos;
	switch (nSBCode)
	{
	case SB_LEFT:
		newPos = si.nMin;
		break;
	case SB_RIGHT:
		newPos = si.nMax;
		break;
	case SB_LINELEFT:
		newPos -= 1;
		break;
	case SB_LINERIGHT:
		newPos += 1;
		break;
	case SB_PAGELEFT:
		newPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		newPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		newPos = si.nTrackPos;
		break;
	case SB_THUMBPOSITION:
		newPos = nPos;
		break;
	}

	int displayCount = m_klinePeriodDays;
	std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
	auto stockData = g_data.GetStockData(m_stock_id);
	if (stockData)
	{
		auto klineObj = stockData->getKLineData();
		if (klineObj)
		{
			displayCount = min(m_klinePeriodDays, static_cast<int>(klineObj->data.size()));
		}
	}
	const int minBarWidth = 7;
	const int gap = 1;
	CRect clientRect;
	GetClientRect(clientRect);
	int maxVisibleKlines = min(displayCount, clientRect.Width() / (minBarWidth + gap));
	int scrollRange = max(0, displayCount - maxVisibleKlines);

	newPos = max(si.nMin, min(newPos, scrollRange));
	si.fMask = SIF_POS;
	si.nPos = newPos;
	m_hScrollBar.SetScrollInfo(&si, TRUE);

	m_scrollOffset = newPos;
	Invalidate();
}

void CFloatingWnd::OnBnClickedTimeLineBtn()
{
	if (m_isOverviewMode)
	{
		m_isOverviewMode = false;
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showTrendView = false;
		m_showChipPeak = false;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;  // 显示最新走势
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
	else if (m_isKLineMode || m_isMin5KLineMode || m_isMin30KLineMode)
	{
		// 回到分时模式
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showTrendView = false;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showChipPeak = false;
		m_scrollOffset = 0;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	}
}

void CFloatingWnd::OnBnClickedKLineBtn()
{
	if (m_isOverviewMode)
	{
		m_isOverviewMode = false;
		m_isKLineMode = true;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_scrollOffset = 0;
		m_showTrendView = false;  // 日K默认显示K线图
		m_showChipPeak = true;
		m_showMA = false;
		m_timelineVisibleCount = 40;  // 日K线初始缩放到最大，显示最新40根
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	}
	else if (!m_isKLineMode || m_isMin5KLineMode || m_isMin30KLineMode)
	{
		// 当前在分时模式或5分钟/30分钟K线模式，切到日K模式
		m_isKLineMode = true;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showTrendView = false;  // 日K默认显示K线图
		m_showChipPeak = true;
		m_scrollOffset = 0;
		m_timelineVisibleCount = 40;  // 日K线初始缩放到最大，显示最新40根
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		EnsureChipPeakData();
		Invalidate();
		PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	}
	else if (m_showTrendView)
	{
		m_showTrendView = false;
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedT0Btn()
{
	m_showT0Markers = !m_showT0Markers;
	SafeSetButtonStyle(m_btnT0, m_showT0Markers ? BS_DEFPUSHBUTTON : BS_FLAT);
	Invalidate();
}

void CFloatingWnd::OnBnClickedChipPeakBtn()
{
	m_showChipPeak = !m_showChipPeak;
	SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);

	EnsureChipPeakData();
	Invalidate();
}

void CFloatingWnd::OnBnClickedOrderBookBtn()
{
	m_showChipPeak = !m_showChipPeak;
	SafeSetButtonStyle(m_btnChipPeak, m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnOrderBook, !m_showChipPeak ? BS_DEFPUSHBUTTON : BS_FLAT);

	EnsureChipPeakData();
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
		m_btnIndicatorMACD.Invalidate();
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
		wnd.SetWindowPos(nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOREDRAW);
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
			auto requestThread = [](LPVOID pParam) -> UINT {
				auto param = static_cast<std::pair<HWND, std::wstring>*>(pParam);
				HWND hWnd = param->first;
				std::wstring stockId = param->second;
				delete param;

				g_data.RequestStockBasicData(stockId);
				g_data.RequestChipDistributionData(stockId);
				if (::IsWindow(hWnd))
					::PostMessage(hWnd, FWND_MSG_UPDATE_STATUS, 0, 0);
				return 0;
				};
			AfxBeginThread(requestThread, new std::pair<HWND, std::wstring>(GetSafeHwnd(), m_stock_id));
		}
	}
}

void CFloatingWnd::OnBnClickedMABtn()
{
	m_showMA = !m_showMA;
	if (m_showMA)
	{
		m_showBollBands = false;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
	}
	SafeSetButtonStyle(m_btnMA, m_showMA ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnBoll, (m_showBollBands || m_showAmplitudeBands) ? BS_DEFPUSHBUTTON : BS_FLAT);
	Invalidate();
}

void CFloatingWnd::OnBnClickedMin5KLineBtn()
{
	if (!m_isMin5KLineMode)
	{
		// 切换到5分钟K线模式
		m_isKLineMode = true;
		m_isMin5KLineMode = true;
		m_isMin30KLineMode = false;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showChipPeak = false;
		m_scrollOffset = 0;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;  // 初始化缩放，显示最新40个数据点
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
	}
	else
	{
		// 退出5分钟K线模式，回到分时模式
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showTrendView = false;
		m_showChipPeak = false;
		m_scrollOffset = 0;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
	}
	UpdateModeButtons();
	UpdatePeriodComboVisibility();
	EnsureChipPeakData();
	Invalidate();
	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
}

void CFloatingWnd::OnBnClickedMin30KLineBtn()
{
	if (!m_isMin30KLineMode)
	{
		// 切换到30分钟K线模式
		m_isKLineMode = true;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = true;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showChipPeak = false;  // 默认展示盘口，与5分钟视图保持一致
		m_scrollOffset = 0;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;  // 初始化缩放，显示最新40个数据点
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
	}
	else
	{
		// 退出30分钟K线模式，回到分时模式
		m_isKLineMode = false;
		m_isMin5KLineMode = false;
		m_isMin30KLineMode = false;
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_btnBoll.SetWindowText(_T("ZF"));
		m_showTrendView = false;
		m_showChipPeak = false;
		m_scrollOffset = 0;
		m_timelineScrollOffset = -1;  // 自动滚动到末尾
		m_timelineVisibleCount = 40;
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		m_timelineVolumeTitleTip.Empty();
		m_timelineMacdTitleTip.Empty();
		m_timelineKdjTitleTip.Empty();
	}
	UpdateModeButtons();
	UpdatePeriodComboVisibility();
	EnsureChipPeakData();
	Invalidate();
	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
}

void CFloatingWnd::OnBnClickedBollBtn()
{
	// 在布林带(ZF)和振幅(BL)之间切换
	// ZF模式：绘制布林带，按钮显示"ZF"
	// BL模式：绘制振幅上下线，按钮显示"BL"
	if (!m_showAmplitudeBands && m_showBollBands)
	{
		// 当前是布林带(ZF) → 切换到振幅(BL)
		m_showBollBands = false;
		m_showAmplitudeBands = true;
		m_showMA = false;
		m_btnBoll.SetWindowText(_T("BL"));
	}
	else
	{
		// 当前是振幅(BL)或无 → 切换到布林带(ZF)
		m_showBollBands = true;
		m_showAmplitudeBands = false;
		m_showMA = false;
		m_btnBoll.SetWindowText(_T("ZF"));
	}
	SafeSetButtonStyle(m_btnBoll, (m_showBollBands || m_showAmplitudeBands) ? BS_DEFPUSHBUTTON : BS_FLAT);
	SafeSetButtonStyle(m_btnMA, m_showMA ? BS_DEFPUSHBUTTON : BS_FLAT);
	Invalidate();
}

void CFloatingWnd::OnBnClickedZoomOutBtn()
{
	// 缩小：显示全部240分钟数据
	m_timelineVisibleCount = 240;
	m_timelineScrollOffset = 0;
	Invalidate();
}

void CFloatingWnd::OnBnClickedZoomInBtn()
{
	// 放大：显示最新40个数据点
	m_timelineVisibleCount = 40;
	m_timelineScrollOffset = -1;  // 自动滚动到末尾
	Invalidate();
}

void CFloatingWnd::OnBnClickedIndicatorMACDBtn()
{
	m_timelineIndicator = TimelineIndicator::MACD;
	m_timelineMacdTitleTip.Empty();
	m_timelineKdjTitleTip.Empty();
	m_timelineWrTitleTip.Empty();
	m_timelineRsiTitleTip.Empty();
	Invalidate();
}

void CFloatingWnd::OnBnClickedIndicatorKDJBtn()
{
	m_timelineIndicator = TimelineIndicator::KDJ;
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

void CFloatingWnd::DrawStockListPanel(CDC& memDC, int x, int y, int w, int h, const std::wstring& currentStockId)
{
	// 绘制背景
	memDC.FillSolidRect(x, y, w, h, RGB(245, 245, 245));

	// 绘制标题栏（与走势图标题栏高度一致）
	const int titleH = g_data.RDPI(16);
	memDC.FillSolidRect(x, y, w, titleH, RGB(230, 230, 230));
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	memDC.TextOut(x + g_data.RDPI(4), y + g_data.RDPI(2), _T("股票列表"));

	// 绘制分隔线
	CPen linePen(PS_SOLID, 1, RGB(200, 200, 200));
	CPen* pOldPen = memDC.SelectObject(&linePen);
	memDC.MoveTo(x, y + titleH);
	memDC.LineTo(x + w, y + titleH);

	// 获取所有股票列表（加锁访问，过滤掉大盘指数）
	std::vector<std::wstring> stockCodes;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		for (const auto& code : g_data.m_setting_data.m_stock_codes)
		{
			if (GetStockPriority(code) >= 200)  // 只保留非指数股票
				stockCodes.push_back(code);
		}
	}

	if (stockCodes.empty())
	{
		// 没有股票时显示提示
		memDC.SetTextColor(RGB(150, 150, 150));
		memDC.TextOut(x + g_data.RDPI(4), y + titleH + g_data.RDPI(10), _T("暂无股票"));
		memDC.SelectObject(pOldPen);
		return;
	}

	// 每行高度固定35像素
	const int rowHeight = g_data.RDPI(35);
	const int nameHeight = g_data.RDPI(14);
	const int codeHeight = g_data.RDPI(7);

	// 绘制股票列表
	int currentY = y + titleH + g_data.RDPI(2);
	for (const auto& code : stockCodes)
	{
		if (currentY + rowHeight > y + h)
			break;  // 超出区域

		// 获取股票名称（GetStockData 内部无需额外加锁）
		std::wstring stockName = code;  // 默认使用代码作为名称
		auto stockData = g_data.GetStockData(code);
		if (stockData && !stockData->info.displayName.empty())
		{
			stockName = stockData->info.displayName;
		}

		// 高亮当前股票
		bool isCurrent = (code == currentStockId);
		if (isCurrent)
		{
			memDC.FillSolidRect(x + 1, currentY, w - 2, rowHeight, RGB(200, 220, 255));
		}

		// 文字垂直居中：内容总高度 = nameHeight + codeHeight，在rowHeight内居中
		int contentH = nameHeight + codeHeight;
		int textOffsetY = (rowHeight - contentH) / 2;

		// 绘制股票名称（上方，超出宽度截断）
		memDC.SetTextColor(isCurrent ? RGB(0, 0, 180) : COLOR_BLACK);
		CFont nameFont;
		nameFont.CreatePointFont(90, _T("Microsoft YaHei"), &memDC);
		CFont* pOldFont = memDC.SelectObject(&nameFont);
		CRect nameRect(x + g_data.RDPI(4), currentY + textOffsetY, x + w - g_data.RDPI(4), currentY + textOffsetY + nameHeight);
		memDC.DrawText(stockName.c_str(), stockName.length(), &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		memDC.SelectObject(pOldFont);
		nameFont.DeleteObject();

		// 绘制股票代码（下方，字体较小）
		memDC.SetTextColor(isCurrent ? RGB(80, 80, 180) : RGB(100, 100, 100));
		CFont codeFont;
		codeFont.CreatePointFont(70, _T("Microsoft YaHei"), &memDC);
		pOldFont = memDC.SelectObject(&codeFont);
		memDC.TextOut(x + g_data.RDPI(4), currentY + textOffsetY + nameHeight, code.c_str());
		memDC.SelectObject(pOldFont);
		codeFont.DeleteObject();

		// 绘制分隔线
		currentY += rowHeight;
		memDC.MoveTo(x, currentY);
		memDC.LineTo(x + w, currentY);
	}

	memDC.SelectObject(pOldPen);
}

UINT CFloatingWnd::NetworkThreadProc(LPVOID pParam)
{
	CFloatingWnd* pFW = (CFloatingWnd*)pParam;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CFlagLocker flag_locker(pFW->m_is_thread_running);

	if (pFW->m_stock_id.empty())
	{
		return 0;
	}

	// 收盘后/盘前/周末不请求数据（午休期间允许请求）
	// 但启动后首次请求不受此限制，以确保能获取当天盘中数据
	if (!pFW->m_isFirstRequest && !CDataManager::IsTradingDaySession())
	{
		return 0;
	}
	pFW->m_isFirstRequest = false;

	time_t now = time(nullptr);

	// 视图相关数据（分时/日K线）：仅在非5min/30min视图下按10秒间隔获取
	if (!pFW->m_isMin5KLineMode && !pFW->m_isMin30KLineMode)
	{
		if (now - (time_t)pFW->m_last_request_time > 10)
		{
			pFW->m_last_request_time = now;
			if (pFW->m_is_thread_stopping) return 0;   // 关闭中，跳过 HTTP
			if (pFW->m_isKLineMode)
				g_data.RequestKLineData(pFW->m_stock_id);
			else
				g_data.RequestTimelineData(pFW->m_stock_id);
		}
	}

	// 5分钟K线：固定60秒间隔获取，与当前视图无关
	// 这样切换到5分钟视图时数据已是最新，无需临时触发请求
	if (now - (time_t)pFW->m_last_min5_fetch_time > 60)
	{
		pFW->m_last_min5_fetch_time = now;
		if (pFW->m_is_thread_stopping) return 0;   // 关闭中，跳过 HTTP
		g_data.RequestMin5KLineData(pFW->m_stock_id, 250);
	}

	// 30分钟K线：固定600秒间隔获取，与当前视图无关
	if (now - (time_t)pFW->m_last_min30_fetch_time > 600)
	{
		pFW->m_last_min30_fetch_time = now;
		if (pFW->m_is_thread_stopping) return 0;   // 关闭中，跳过 HTTP
		g_data.RequestMin30KLineData(pFW->m_stock_id, 250);
	}

	// 线程结束前检查是否有待处理的请求（切换股票时线程正在运行导致的）
	// 关闭窗口期间不再投递新请求，避免窗口销毁过程中再起新线程
	if (!pFW->m_is_thread_stopping && pFW->m_pendingRequest && ::IsWindow(pFW->GetSafeHwnd()))
	{
		pFW->PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	}

	return 0;
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
		RequestData();
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
		RequestData();
	}
	return 0;
}

void CFloatingWnd::TestSmartSignalOnDoubleClick(int timelineIndex)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	std::vector<STOCK::Bar> allBars5, allBars30;
	std::vector<std::string> allBars5Time;
	CString dataStatus;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (!stockData)
		{
			MessageBox(_T("未获取到当前股票数据，无法测试买卖点检测。"), _T("买卖点检测测试"), MB_ICONINFORMATION);
			return;
		}

		auto min5KLineObj = stockData->getMin5KLineData();
		auto min30KLineObj = stockData->getMin30KLineData();
		if (min5KLineObj)
		{
			allBars5.reserve(min5KLineObj->data.size());
			allBars5Time.reserve(min5KLineObj->data.size());
			for (const auto& kp : min5KLineObj->data)
			{
				allBars5.push_back(STOCK::Bar::FromKLinePoint(kp));
				allBars5Time.push_back(kp.day);
			}
		}
		if (min30KLineObj)
		{
			allBars30.reserve(min30KLineObj->data.size());
			for (const auto& kp : min30KLineObj->data)
				allBars30.push_back(STOCK::Bar::FromKLinePoint(kp));
		}
	}

	if (allBars5.empty() || allBars30.empty())
	{
		dataStatus.Format(_T("股票代码：%s\r\n5分钟K线数量：%zu\r\n30分钟K线数量：%zu\r\n双击价格：%.3f\r\n双击时间：%s\r\n"),
			CString(m_stock_id.c_str()), allBars5.size(), allBars30.size(), static_cast<double>(m_pendingTradePrice), m_pendingTradeTime.GetString());
		CString msg = dataStatus + _T("\r\n测试结果：缺少5分钟或30分钟K线数据，无法调用买卖点检测函数。\r\n原因：SignalAnalyzer 需要 bars5 与 bars30 同时存在。 ");
		MessageBox(msg, _T("买卖点检测测试"), MB_ICONINFORMATION);
		return;
	}

	auto extractDatePart = [](const std::string& timeText) -> std::string {
		auto spacePos = timeText.find(' ');
		return spacePos != std::string::npos ? timeText.substr(0, spacePos) : std::string();
		};
	auto extractTimeMinute = [](const std::string& timeText) -> int {
		std::string t = timeText;
		auto spacePos = t.find(' ');
		if (spacePos != std::string::npos)
			t = t.substr(spacePos + 1);
		if (t.length() < 5)
			return -1;
		int hour = -1;
		int minute = -1;
		if (sscanf_s(t.substr(0, 5).c_str(), "%d:%d", &hour, &minute) != 2)
			return -1;
		if (hour < 0 || minute < 0 || minute >= 60)
			return -1;
		return hour * 60 + minute;
		};

	CStringA pendingTradeTimeA(m_pendingTradeTime);
	int clickedMinute = extractTimeMinute(std::string(pendingTradeTimeA.GetString()));
	std::string latestDate;
	for (auto it = allBars5Time.rbegin(); it != allBars5Time.rend(); ++it)
	{
		latestDate = extractDatePart(*it);
		if (!latestDate.empty())
			break;
	}
	int barIndex = -1;
	if (m_isMin5KLineMode)
	{
		barIndex = max(0, min(timelineIndex, static_cast<int>(allBars5.size()) - 1));
	}
	else if (clickedMinute >= 0)
	{
		for (int i = 0; i < static_cast<int>(allBars5Time.size()); i++)
		{
			if (!latestDate.empty() && extractDatePart(allBars5Time[i]) != latestDate)
				continue;
			int barMinute = extractTimeMinute(allBars5Time[i]);
			if (barMinute < 0)
				continue;
			if (barMinute <= clickedMinute)
				barIndex = i;
			else
				break;
		}
	}
	if (barIndex < 0)
		barIndex = static_cast<int>(allBars5.size()) - 1;
	barIndex = max(0, min(barIndex, static_cast<int>(allBars5.size()) - 1));

	std::vector<STOCK::Bar> bars5(allBars5.begin(), allBars5.begin() + barIndex + 1);
	std::vector<STOCK::Bar> bars30 = allBars30;

	CString mappedBarTime;
	if (barIndex >= 0 && barIndex < static_cast<int>(allBars5Time.size()))
		mappedBarTime = CString(allBars5Time[barIndex].c_str());
	dataStatus.Format(_T("股票代码：%s\r\n5分钟K线数量：%zu / 全部%zu\r\n30分钟K线数量：%zu\r\n双击分时索引：%d\r\n双击分钟：%d\r\n匹配交易日：%s\r\n映射5分钟K线索引：%d\r\n映射5分钟K线时间：%s\r\n双击价格：%.3f\r\n双击时间：%s\r\n"),
		CString(m_stock_id.c_str()), bars5.size(), allBars5.size(), bars30.size(), timelineIndex, clickedMinute, CString(latestDate.c_str()).GetString(), barIndex, mappedBarTime.GetString(), static_cast<double>(m_pendingTradePrice), m_pendingTradeTime.GetString());

	STOCK::TrendState30m trendState = CSignalAnalyzer::Get30mTrendState(bars30);
	STOCK::Signal5m sig5m = CSignalAnalyzer::Get5mSignal(bars5, trendState);
	bool forbid = CSignalAnalyzer::IsForbidTrade(bars5, trendState);
	CSignalAnalyzer::T0Signal smartSignal = CSignalAnalyzer::DetectSmartSignal(bars5, bars30);
	auto batchSignals = CSignalAnalyzer::BatchDetectSignals(allBars5, bars30);
	const CSignalAnalyzer::SmartSignalPoint* clickedBatchSignal = nullptr;
	for (const auto& item : batchSignals)
	{
		if (item.barIndex == barIndex)
		{
			clickedBatchSignal = &item;
			break;
		}
	}

	int buyCount = 0;
	int sellCount = 0;
	int forbidCount = 0;
	for (const auto& item : batchSignals)
	{
		if (item.isForbid)
			forbidCount++;
		else if (item.isBuy)
			buyCount++;
		else
			sellCount++;
	}

	CString riskDetail;
	if (bars5.size() < 60)
	{
		riskDetail = _T("风控未运行：5分钟K线不足60根。\r\n");
	}
	else
	{
		std::vector<double> bollBandSeq(bars5.size(), 0.0);
		for (size_t i = 19; i < bars5.size(); i++)
		{
			std::vector<double> closes;
			closes.reserve(20);
			for (size_t j = i - 19; j <= i; j++)
				closes.push_back(bars5[j].close);
			bollBandSeq[i] = 4.0 * CSignalAnalyzer::CalcStdDev(closes);
		}

		size_t lastIndex = bars5.size() - 1;
		bool bandExpand = false;
		bool bandHighPercentile = false;
		if (lastIndex > 0 && bollBandSeq[lastIndex] > 0 && bollBandSeq[lastIndex - 1] > 0)
		{
			bandExpand = bollBandSeq[lastIndex] > bollBandSeq[lastIndex - 1] * 1.4;
			size_t start = lastIndex > 60 ? lastIndex - 60 : 0;
			int validCount = 0;
			int lowerOrEqualCount = 0;
			for (size_t i = start; i < lastIndex; i++)
			{
				if (bollBandSeq[i] <= 0)
					continue;
				validCount++;
				if (bollBandSeq[lastIndex] >= bollBandSeq[i])
					lowerOrEqualCount++;
			}
			bandHighPercentile = validCount >= 20 && lowerOrEqualCount * 100 >= validCount * 90;
		}

		STOCK::BollResult boll = CSignalAnalyzer::CalcBoll(bars5, 20);
		double bandRatio = boll.mid > 0 ? boll.bandwidth / boll.mid : 0.0;
		size_t avgStart = lastIndex + 1 > 10 ? lastIndex + 1 - 10 : 0;
		double avgBollBandwidth = 0.0;
		size_t avgBollCount = 0;
		for (size_t i = avgStart; i <= lastIndex; i++)
		{
			if (bollBandSeq[i] <= 0)
				continue;
			avgBollBandwidth += bollBandSeq[i];
			avgBollCount++;
		}
		if (avgBollCount > 0)
			avgBollBandwidth /= avgBollCount;
		double absNarrowThreshold = boll.mid > 0 && boll.mid < 1.5 ? 0.05 : 0.03;
		double relativeNarrowThreshold = boll.mid > 0 ? boll.mid * 0.005 : 0.0;
		double narrowThreshold = max(relativeNarrowThreshold, absNarrowThreshold);
		bool narrowBand = avgBollBandwidth > 0 && avgBollBandwidth < narrowThreshold;

		std::vector<STOCK::Bar> prevBars(bars5.begin(), bars5.end() - 1);
		std::vector<STOCK::Bar> prevPrevBars(bars5.begin(), bars5.end() - 2);
		STOCK::KDJResult kdj = CSignalAnalyzer::CalcKDJ(bars5, 9);
		STOCK::KDJResult prevKdj = CSignalAnalyzer::CalcKDJ(prevBars, 9);
		STOCK::KDJResult prevPrevKdj = CSignalAnalyzer::CalcKDJ(prevPrevBars, 9);
		bool kdjTopPassive = (kdj.k > 85 && prevKdj.k > 85 && prevPrevKdj.k > 85
			&& kdj.k < prevKdj.k && prevKdj.k < prevPrevKdj.k);
		bool kdjBottomPassive = (kdj.k < 15 && prevKdj.k < 15 && prevPrevKdj.k < 15
			&& kdj.k > prevKdj.k && prevKdj.k > prevPrevKdj.k);

		double wr6 = CSignalAnalyzer::CalcWR(bars5, 6);
		double wrPrev = CSignalAnalyzer::CalcWR(prevBars, 6);
		double wrPrevPrev = CSignalAnalyzer::CalcWR(prevPrevBars, 6);
		bool wrTopPassive = (wr6 < 18 && wrPrev < 18 && wrPrevPrev < 18
			&& wr6 > wrPrev && wrPrev > wrPrevPrev);
		bool wrBottomPassive = (wr6 > 82 && wrPrev > 82 && wrPrevPrev > 82
			&& wr6 < wrPrev && wrPrev < wrPrevPrev);

		bool bollRiskActive = trendState == STOCK::TrendState30m::STATE_SHAKE && (bandExpand || bandHighPercentile || narrowBand);
		bool kdjBottomActive = trendState == STOCK::TrendState30m::STATE_STRONG || trendState == STOCK::TrendState30m::STATE_SHAKE;
		bool wrBottomActive = trendState != STOCK::TrendState30m::STATE_WEAK;

		CString line;
		line.Format(_T("BOLL带宽快速扩张：%s（当前=%.4f，上一根=%.4f，阈值=1.4，仅震荡趋势限制买入）\r\n"), BoolToText(bandExpand).GetString(), bollBandSeq[lastIndex], bollBandSeq[lastIndex - 1]);
		riskDetail += line;
		line.Format(_T("BOLL带宽历史高分位：%s（阈值=90，仅震荡趋势限制买入）\r\n"), BoolToText(bandHighPercentile).GetString());
		riskDetail += line;
		line.Format(_T("BOLL极窄：%s（当前带宽=%.4f，近10根平均带宽=%.4f，中轨=%.4f，当前带宽/中轨=%.4f%%，阈值=%.4f，低价阈值=%s，仅震荡趋势限制买入）\r\n"),
			BoolToText(narrowBand).GetString(), boll.bandwidth, avgBollBandwidth, boll.mid, bandRatio * 100.0, narrowThreshold,
			boll.mid > 0 && boll.mid < 1.5 ? _T("0.05") : _T("0.03"));
		riskDetail += line;
		line.Format(_T("KDJ高位钝化：%s（K=%.2f，前K=%.2f，前前K=%.2f，当前策略不作为拦截项）\r\n"), BoolToText(kdjTopPassive).GetString(), kdj.k, prevKdj.k, prevPrevKdj.k);
		riskDetail += line;
		line.Format(_T("KDJ低位钝化：%s（K=%.2f，前K=%.2f，前前K=%.2f，当前趋势%s）\r\n"), BoolToText(kdjBottomPassive).GetString(), kdj.k, prevKdj.k, prevPrevKdj.k, kdjBottomActive ? _T("会拦截") : _T("不拦截"));
		riskDetail += line;
		line.Format(_T("WR高位钝化：%s（WR6=%.2f，前=%.2f，前前=%.2f，当前策略不作为拦截项）\r\n"), BoolToText(wrTopPassive).GetString(), wr6, wrPrev, wrPrevPrev);
		riskDetail += line;
		line.Format(_T("WR低位钝化：%s（WR6=%.2f，前=%.2f，前前=%.2f，当前趋势%s）\r\n"), BoolToText(wrBottomPassive).GetString(), wr6, wrPrev, wrPrevPrev, wrBottomActive ? _T("会拦截") : _T("不拦截"));
		riskDetail += line;
		line.Format(_T("实际参与拦截：BOLL=%s，KDJ低位=%s，WR低位=%s（风控仅限制买入，不屏蔽卖出）\r\n"),
			BoolToText(bollRiskActive).GetString(),
			BoolToText(kdjBottomActive && kdjBottomPassive).GetString(),
			BoolToText(wrBottomActive && wrBottomPassive).GetString());
		riskDetail += line;
	}

	CString clickedBatchText;
	if (clickedBatchSignal)
	{
		clickedBatchText.Format(_T("%s（趋势=%s，原因=%s）"),
			clickedBatchSignal->isForbid ? _T("风控禁止") : (clickedBatchSignal->isBuy ? _T("买入") : _T("卖出")),
			TrendStateToText(clickedBatchSignal->trendState).GetString(),
			clickedBatchSignal->reason.GetString());
	}
	else
	{
		clickedBatchText = _T("该K线无批量绘制信号");
	}

	CString latestSignalText;
	if (smartSignal.isForbid)
		latestSignalText = _T("风控禁止");
	else if (!smartSignal.valid)
		latestSignalText = _T("无有效综合信号");
	else
		latestSignalText = smartSignal.isBuy ? _T("买入") : _T("卖出");

	CString msg;
	msg.Format(_T("%s\r\n")
		_T("一、单项信号测试结果\r\n")
		_T("图上该K线绘制信号：%s\r\n")
		_T("30分钟趋势：%s\r\n")
		_T("5分钟买卖信号：%s\r\n")
		_T("\r\n二、风险运行结果\r\n")
		_T("是否触发风控禁止：%s\r\n")
		_T("风控明细：\r\n%s")
		_T("\r\n三、综合判定结果\r\n")
		_T("有效：%s\r\n")
		_T("方向：%s\r\n")
		_T("价格：%.3f\r\n")
		_T("强度：%d\r\n")
		_T("原因：%s\r\n")
		_T("\r\n四、批量信号测试结果\r\n")
		_T("总信号数：%zu\r\n")
		_T("买入信号：%d\r\n")
		_T("卖出信号：%d\r\n")
		_T("风控禁止：%d\r\n"),
		dataStatus.GetString(),
		clickedBatchText.GetString(),
		TrendStateToText(trendState).GetString(),
		Signal5mToText(sig5m).GetString(),
		BoolToText(forbid).GetString(),
		riskDetail.GetString(),
		BoolToText(smartSignal.valid).GetString(),
		latestSignalText.GetString(),
		static_cast<double>(smartSignal.price),
		smartSignal.strength,
		smartSignal.reason.IsEmpty() ? _T("无") : smartSignal.reason.GetString(),
		batchSignals.size(),
		buyCount,
		sellCount,
		forbidCount);

	MessageBox(msg, _T("买卖点检测测试"), MB_ICONINFORMATION);
}

LRESULT CFloatingWnd::OnShowTradeDialog(WPARAM wParam, LPARAM lParam)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	/* 测试买卖点检测时暂时屏蔽交易记录弹窗
	CTradeRecordDialog dlg(this);
	dlg.SetTradeInfo(m_pendingTradeTime, m_pendingTradePrice, CString(m_stock_id.c_str()));
	dlg.DoModal();
	*/
	return 0;
}

void CFloatingWnd::OnDestroy()
{
	CWnd::OnDestroy();

	// 通知 NetworkThreadProc 尽快退出（在每个 HTTP 请求之间检查）
	m_is_thread_stopping = true;

	// 等待后台线程退出（HTTP 请求可能尚未完成，给最多 2 秒）
	// 不阻塞过久，避免影响主程序退出体验
	if (m_hNetworkThread != nullptr)
	{
		WaitForSingleObject(m_hNetworkThread, 2000);
		CloseHandle(m_hNetworkThread);
		m_hNetworkThread = nullptr;
	}
	// m_bAutoDelete = FALSE，需手动释放 CWinThread 对象
	if (m_pNetworkThread != nullptr)
	{
		delete m_pNetworkThread;
		m_pNetworkThread = nullptr;
	}

	if (m_CTransparentWnd.GetSafeHwnd())
	{
		m_CTransparentWnd.DestroyWindow();
	}
}