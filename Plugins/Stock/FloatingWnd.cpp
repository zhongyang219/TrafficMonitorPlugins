#include "pch.h"
#include "FloatingWnd.h"
#include <afxinet.h>
#include <memory>
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>
#include <algorithm>
#include "OptionsDlg.h"

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

#define ORDER_BOOK_WIDTH          g_data.RDPI(150)     // 右侧信息面板宽度

enum {
	IDC_OVERVIEW_BTN = 1000,
	IDC_TIMELINE_BTN = 1001,
	IDC_KLINE_BTN = 1002,
	IDC_KLINE_PERIOD = 1003,
	IDC_TREND_BTN = 1004,
	IDC_CLOSE_BTN = 1005,
	IDM_CLOSE_WINDOW = 1006,
	IDC_MA_BTN = 1007
};

BEGIN_MESSAGE_MAP(CFloatingWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_MESSAGE((WM_USER + 100), OnUpdateStatus)
	ON_MESSAGE((WM_USER + 101), OnRequestData)
	ON_MESSAGE((WM_USER + 102), OnShowEditDialog)
	ON_MESSAGE((WM_USER + 103), OnShowAddDialog)
	ON_MESSAGE(IDM_CLOSE_WINDOW, OnCloseWindow)
	ON_BN_CLICKED(IDC_OVERVIEW_BTN, &CFloatingWnd::OnBnClickedOverviewBtn)
	ON_BN_CLICKED(IDC_TIMELINE_BTN, &CFloatingWnd::OnBnClickedTimeLineBtn)
	ON_BN_CLICKED(IDC_KLINE_BTN, &CFloatingWnd::OnBnClickedKLineBtn)
	ON_CBN_SELCHANGE(IDC_KLINE_PERIOD, &CFloatingWnd::OnCbnSelChangeKLinePeriod)
	ON_BN_CLICKED(IDC_TREND_BTN, &CFloatingWnd::OnBnClickedTrendBtn)
	ON_BN_CLICKED(IDC_CLOSE_BTN, &CFloatingWnd::OnBnClickedCloseBtn)
	ON_BN_CLICKED(IDC_MA_BTN, &CFloatingWnd::OnBnClickedMABtn)
END_MESSAGE_MAP()

int CFloatingWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	const int btnWidth = g_data.RDPI(40);
	const int btnHeight = g_data.RDPI(22);
	const int btnGap = g_data.RDPI(4);

	// 左侧按钮：总览、分时、日K
	CRect overviewRect(g_data.RDPI(5), g_data.RDPI(2), g_data.RDPI(5) + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnOverview.Create(_T("总览"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, overviewRect, this, IDC_OVERVIEW_BTN);

	CRect timelineRect(overviewRect.right + btnGap, g_data.RDPI(2), overviewRect.right + btnGap + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnTimeLine.Create(_T("分时"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, timelineRect, this, IDC_TIMELINE_BTN);

	CRect klineRect(timelineRect.right + btnGap, g_data.RDPI(2), timelineRect.right + btnGap + btnWidth, g_data.RDPI(2) + btnHeight);
	m_btnKLine.Create(_T("日K"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, klineRect, this, IDC_KLINE_BTN);

	// 右侧按钮：关闭、日期选择、K线/走势、MA（从右往左排列）
	const int closeBtnWidth = g_data.RDPI(20);
	const int closeBtnHeight = g_data.RDPI(18);
	const int cx = lpCreateStruct->cx;
	CRect closeBtnRect(cx - closeBtnWidth, g_data.RDPI(2), cx, g_data.RDPI(2) + closeBtnHeight);
	m_btnClose.Create(_T("X"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, closeBtnRect, this, IDC_CLOSE_BTN);

	const int comboWidth = g_data.RDPI(50);
	const int comboHeight = g_data.RDPI(18);
	const int comboGap = g_data.RDPI(4);
	CRect comboRect;
	comboRect.right = closeBtnRect.left - comboGap;
	comboRect.left = comboRect.right - comboWidth;
	comboRect.top = g_data.RDPI(2);
	comboRect.bottom = comboRect.top + comboHeight;
	m_comboKLinePeriod.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, comboRect, this, IDC_KLINE_PERIOD);

	m_comboKLinePeriod.AddString(_T("1年"));
	m_comboKLinePeriod.AddString(_T("2年"));
	m_comboKLinePeriod.AddString(_T("3年"));
	m_comboKLinePeriod.SetCurSel(0);

	const int trendBtnWidth = g_data.RDPI(40);
	CRect trendBtnRect(comboRect.left - trendBtnWidth - comboGap, g_data.RDPI(2), comboRect.left - comboGap, g_data.RDPI(2) + btnHeight);
	m_btnTrend.Create(_T("K线"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, trendBtnRect, this, IDC_TREND_BTN);

	const int maBtnWidth = g_data.RDPI(40);
	CRect maBtnRect(trendBtnRect.left - maBtnWidth - comboGap, g_data.RDPI(2), trendBtnRect.left - comboGap, g_data.RDPI(2) + btnHeight);
	m_btnMA.Create(_T("MA"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, maBtnRect, this, IDC_MA_BTN);

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
		if (req_time - m_last_request_time > 10)
		{
			m_last_request_time = req_time;
			// 开始网络请求
			RequestData();
		}
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
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
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
		L"", WS_POPUP | WS_VISIBLE | WS_BORDER,
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

CPoint CFloatingWnd::Stock2Point(int x, int y, int w, int h, float unitY, const STOCK::TimelinePoint& item, const STOCK::Price prevClosePrice)
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

	CString prefixTxt;
	prefixTxt.Format(_T("%s:"), realtimeData.displayName.c_str());

	CString currentTxt = CCommon::FormatFloat(realtimeData.currentPrice);
	CString diffTxt;
	if (diff >= 0)
		diffTxt.Format(_T(" +%.2f%%(%s)"), diffPercent, CCommon::FormatFloat(diff));
	else
		diffTxt.Format(_T(" %.2f%%(%s)"), diffPercent, CCommon::FormatFloat(std::abs(diff)));

	// 成本信息
	double costPrice = g_data.GetCostPrice(m_stock_id);
	CString costLabelTxt;
	CString costValueTxt;
	COLORREF costValueColor = COLOR_GRAY_TEXT;
	if (costPrice > 0 && realtimeData.currentPrice > 0)
	{
		double costDiff = realtimeData.currentPrice - costPrice;
		double costDiffPercent = (costDiff / costPrice) * 100;
		costValueColor = costDiff >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CString costDiffSign = costDiff >= 0 ? _T("+") : _T("");
		costLabelTxt = _T(" 成本:");
		costValueTxt.Format(_T("%s %s%.2f%%(%s)"), CCommon::FormatFloat(costPrice), costDiffSign, costDiffPercent, CCommon::FormatFloat(costDiff));
	}
	else if (costPrice > 0)
	{
		costLabelTxt = _T(" 成本:");
		costValueTxt = CCommon::FormatFloat(costPrice);
	}
	else
	{
		costLabelTxt = _T(" 成本:");
		costValueTxt = _T("--");
	}

	// 计算总宽度以居中
	CSize prefixSize = memDC.GetTextExtent(prefixTxt);
	CSize currentSize = memDC.GetTextExtent(currentTxt);
	CSize diffSize = memDC.GetTextExtent(diffTxt);
	CSize costLabelSize = memDC.GetTextExtent(costLabelTxt);
	CSize costValueSize = memDC.GetTextExtent(costValueTxt);
	int totalWidth = prefixSize.cx + currentSize.cx + diffSize.cx + costLabelSize.cx + costValueSize.cx;

	int startX = (windowWidth - totalWidth) / 2;
	int centerY = headerHeight / 2;

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(startX, centerY - prefixSize.cy / 2, prefixTxt);

	int curX = startX + prefixSize.cx;
	memDC.SetTextColor(diffColor);
	memDC.TextOut(curX, centerY - currentSize.cy / 2, currentTxt);

	curX += currentSize.cx;
	memDC.TextOut(curX, centerY - diffSize.cy / 2, diffTxt);

	curX += diffSize.cx;
	memDC.SetTextColor(COLOR_BLACK);
	memDC.TextOut(curX, centerY - costLabelSize.cy / 2, costLabelTxt);

	curX += costLabelSize.cx;
	memDC.SetTextColor(costValueColor);
	memDC.TextOut(curX, centerY - costValueSize.cy / 2, costValueTxt);
}

void CFloatingWnd::DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawHeader(memDC, ctx.realtimeData, ctx.windowWidth, g_data.RDPI(26));
}

// 绘制最高价、最低价、平均价、振幅合并行（原MA均线位置）
void CFloatingWnd::DrawTimelineMAIndicators(CDC& memDC, const TimelineDrawContext& ctx)
{
	int maY = g_data.RDPI(26) + g_data.RDPI(2);
	if (ctx.realtimeData.currentPrice <= 0)
		return;

	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto klinePtr = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;

	// 最高价
	CString highTxt;
	COLORREF highColor = (ctx.realtimeData.highPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	highTxt.Format(_T("最高:%s "), CCommon::FormatFloat(ctx.realtimeData.highPrice));

	// 最低价
	CString lowTxt;
	COLORREF lowColor = (ctx.realtimeData.lowPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	lowTxt.Format(_T("最低:%s "), CCommon::FormatFloat(ctx.realtimeData.lowPrice));

	// 平均价
	CString avgTxt;
	double avgPrice = ctx.realtimeData.GetAveragePrice();
	COLORREF avgColor = COLOR_BLACK;
	if (avgPrice > 0)
	{
		avgColor = avgPrice >= ctx.realtimeData.prevClosePrice ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		avgTxt.Format(_T("平均:%s "), CCommon::FormatFloat(avgPrice));
	}
	else
	{
		avgTxt = _T("平均:-- ");
	}

	// 振幅01
	CString ampTxt;
	float fluctuation = ctx.realtimeData.highPrice - ctx.realtimeData.lowPrice;
	float fluctuationPercent = ctx.realtimeData.prevClosePrice != 0 ? (fluctuation / ctx.realtimeData.prevClosePrice) * 100 : 0;
	ampTxt.Format(_T("振幅:%.2f%%(%s)"), fluctuationPercent, CCommon::FormatFloat(fluctuation));

	// 居中排列
	int totalWidth = memDC.GetTextExtent(highTxt).cx + memDC.GetTextExtent(lowTxt).cx
		+ memDC.GetTextExtent(avgTxt).cx + memDC.GetTextExtent(ampTxt).cx;
	int startX = (ctx.chartWidth - totalWidth) / 2;
	startX = max(g_data.RDPI(5), startX);

	memDC.SetTextColor(highColor);
	memDC.TextOut(startX, maY, highTxt);
	startX += memDC.GetTextExtent(highTxt).cx;

	memDC.SetTextColor(lowColor);
	memDC.TextOut(startX, maY, lowTxt);
	startX += memDC.GetTextExtent(lowTxt).cx;

	memDC.SetTextColor(avgColor);
	memDC.TextOut(startX, maY, avgTxt);
	startX += memDC.GetTextExtent(avgTxt).cx;

	memDC.SetTextColor(COLOR_BLACK);
	memDC.TextOut(startX, maY, ampTxt);
}

// 绘制开盘/尾盘时间区间高亮背景
void CFloatingWnd::DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx)
{
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

	const TimeMarker timeMarkers[] = {
		{ _T("9:30"), 0 },
		{ _T("10:30"), 60 },
		{ _T("11:30"), 120 },
		{ _T("14:00"), 180 },
		{ _T("15:00"), 240 }
	};

	// X轴时间竖线
	for (const auto& marker : timeMarkers)
	{
		int xPos = ctx.chartWidth * marker.minutesFromStart / 240;
		memDC.MoveTo(xPos, ctx.priceChartTop);
		memDC.LineTo(xPos, ctx.priceChartTop + ctx.priceChartHeight);
	}

	// X轴横线
	memDC.MoveTo(0, ctx.priceChartTop + ctx.priceChartHeight);
	memDC.LineTo(ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight);

	// 中间虚线（前收盘价位置）
	CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
	memDC.SelectObject(&pMiddleLine);
	memDC.MoveTo(0, ctx.priceChartTop + ctx.priceChartHeight / 2);
	memDC.LineTo(ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight / 2);

	memDC.SelectObject(pOldPen);
}

// 绘制涨跌停价和前收盘价标签
void CFloatingWnd::DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx)
{
	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;

	memDC.SetTextColor(COLOR_RED_UP);
	CString upperLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice + priceLimit);
	CRect upperRect{ 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight };
	upperRect.right = upperRect.left + memDC.GetTextExtent(upperLimitTxt).cx;
	memDC.DrawText(upperLimitTxt, upperRect, DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

	memDC.SetTextColor(COLOR_GREEN_DOWN);
	CString lowerLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice - priceLimit);
	CRect lowerRect{ 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight };
	lowerRect.right = lowerRect.left + memDC.GetTextExtent(lowerLimitTxt).cx;
	memDC.DrawText(lowerLimitTxt, lowerRect, DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

	memDC.SetTextColor(COLOR_GRAY_PURPLE);
	CString middleTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice);
	CRect middleRect{ 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight };
	middleRect.right = middleRect.left + memDC.GetTextExtent(middleTxt).cx;
	memDC.DrawText(middleTxt, middleRect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 绘制成本线和盈亏线
void CFloatingWnd::DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	double costPrice = g_data.GetCostPrice(m_stock_id);
	if (costPrice <= 0 || ctx.realtimeData.prevClosePrice == 0)
		return;

	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;

	// 计算价格范围（涨跌停限制）
	STOCK::Price maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
	STOCK::Price minPrice = ctx.realtimeData.prevClosePrice - priceLimit;

	// 在顶部和底部各预留10个像素的空间
	const int pricePaddingY = g_data.RDPI(10);
	double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
	maxPrice += paddingPrice;
	minPrice -= paddingPrice;

	float unitY = ctx.priceChartHeight / (maxPrice - minPrice);

	// 计算成本线Y坐标
	float costLineY = ctx.priceChartTop + ctx.priceChartHeight - (int)((costPrice - minPrice) * unitY);

	costLineY = max((float)ctx.priceChartTop + 5, min(costLineY, (float)ctx.priceChartTop + ctx.priceChartHeight - 5));

	// 绘制成本线
	CPen costLinePen(PS_DOT, 1, COLOR_BLUE_COST);
	CPen* pOldPen = memDC.SelectObject(&costLinePen);
	memDC.MoveTo(0, (int)costLineY);
	memDC.LineTo(ctx.chartWidth, (int)costLineY);

	memDC.SetTextColor(COLOR_BLUE_COST);
	CString costPriceTxt = CCommon::FormatFloat(costPrice);
	CSize costTxtSize = memDC.GetTextExtent(costPriceTxt);
	memDC.TextOut(g_data.RDPI(2), (int)costLineY - costTxtSize.cy, costPriceTxt);

	CString costLabel = _T("cost");
	CSize costLabelSize = memDC.GetTextExtent(costLabel);
	memDC.TextOut(ctx.chartWidth - costLabelSize.cx - g_data.RDPI(2), (int)costLineY - costTxtSize.cy, costLabel);

	// 计算盈亏比例
	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto klinePtr = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;
	double avgAmplitude = klinePtr ? klinePtr->CalculateAverageAmplitude(5) : 0;
	double calculatedRatio = avgAmplitude > 0 ? avgAmplitude / 200.0 : 0.0;

	std::wstring first2 = m_stock_id.substr(2, 2);
	const std::vector<std::wstring> etf_prefixes = { L"50", L"51", L"56", L"15", L"16", L"18" };
	bool isETF = false;
	for (const auto& prefix : etf_prefixes)
	{
		if (first2 == prefix) { isETF = true; break; }
	}

	double minRatio = isETF ? 0.005 : 0.025;
	double plRatio = calculatedRatio > minRatio ? calculatedRatio : minRatio;
	double basePrice = costPrice > ctx.realtimeData.prevClosePrice ? costPrice : ctx.realtimeData.prevClosePrice;
	double profitPrice = basePrice * (1 + plRatio);
	double lossPrice = basePrice * (1 - plRatio);

	float profitLineY = ctx.priceChartTop + ctx.priceChartHeight - (int)((profitPrice - minPrice) * unitY);
	float lossLineY = ctx.priceChartTop + ctx.priceChartHeight - (int)((lossPrice - minPrice) * unitY);

	profitLineY = max((float)ctx.priceChartTop + 5, min(profitLineY, (float)ctx.priceChartTop + ctx.priceChartHeight - 5));
	lossLineY = max((float)ctx.priceChartTop + 5, min(lossLineY, (float)ctx.priceChartTop + ctx.priceChartHeight - 5));

	// 绘制盈利线
	CPen profitPen(PS_DASH, 1, COLOR_RED_UP);
	memDC.SelectObject(&profitPen);
	memDC.MoveTo(0, (int)profitLineY);
	memDC.LineTo(ctx.chartWidth, (int)profitLineY);
	memDC.SetTextColor(COLOR_RED_UP);
	CString profitPriceTxt = CCommon::FormatFloat(profitPrice);
	CSize profitTxtSize = memDC.GetTextExtent(profitPriceTxt);
	int profitTxtY = (int)profitLineY - profitTxtSize.cy;
	memDC.TextOut(g_data.RDPI(2), profitTxtY, profitPriceTxt);
	CString profitRatioTxt;
	profitRatioTxt.Format(_T("+%.2f%%"), plRatio * 100);
	CSize profitRatioSize = memDC.GetTextExtent(profitRatioTxt);
	memDC.TextOut(ctx.chartWidth - profitRatioSize.cx - g_data.RDPI(2), profitTxtY, profitRatioTxt);

	// 绘制亏损线
	CPen lossPen(PS_DASH, 1, COLOR_GREEN_DOWN);
	memDC.SelectObject(&lossPen);
	memDC.MoveTo(0, (int)lossLineY);
	memDC.LineTo(ctx.chartWidth, (int)lossLineY);
	memDC.SetTextColor(COLOR_GREEN_DOWN);
	CString lossPriceTxt = CCommon::FormatFloat(lossPrice);
	CSize lossTxtSize = memDC.GetTextExtent(lossPriceTxt);
	int lossTxtY = (int)lossLineY - lossTxtSize.cy;
	memDC.TextOut(g_data.RDPI(2), lossTxtY, lossPriceTxt);
	CString lossRatioTxt;
	lossRatioTxt.Format(_T("%.2f%%"), -plRatio * 100);
	CSize lossRatioSize = memDC.GetTextExtent(lossRatioTxt);
	memDC.TextOut(ctx.chartWidth - lossRatioSize.cx - g_data.RDPI(2), lossTxtY, lossRatioTxt);

	memDC.SelectObject(pOldPen);
}

// 保持向后兼容：DrawTimelineGridAndLines 调用新的拆分函数
void CFloatingWnd::DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawTimelineMAIndicators(memDC, ctx);
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

	const int before12ClockOffset = 570;
	const int after12ClockOffset = 660;
	const int totalPoints = static_cast<int>(timelinePoint.size());

	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
	float halfH = ctx.priceChartHeight / 2.0f;

	// 计算价格范围（涨跌停限制）
	STOCK::Price maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
	STOCK::Price minPrice = ctx.realtimeData.prevClosePrice - priceLimit;

	// 在顶部和底部各预留10个像素的空间
	const int pricePaddingY = g_data.RDPI(10);
	double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
	maxPrice += paddingPrice;
	minPrice -= paddingPrice;

	float unitY = ctx.priceChartHeight / (maxPrice - minPrice);

	CPen pKLine(PS_SOLID, 1, COLOR_DARK_GRAY_BORDER);
	memDC.SelectObject(&pKLine);

	// 价格曲线
	std::vector<CPoint> dataPoints;
	dataPoints.reserve(timelinePoint.size());
	for (int i = 0; i < totalPoints; i++)
	{
		const auto& item = timelinePoint[i];
		std::vector<std::string> time_arr = CCommon::split(item.time, ":");
		int pointX = 0;
		if (time_arr.size() >= 2)
		{
			int hour = _ttoi(CString(time_arr[0].c_str()));
			int minute = _ttoi(CString(time_arr[1].c_str()));
			int countX = hour * 60 + minute;
			if (hour < 12) countX -= before12ClockOffset;
			else if (hour >= 13) countX -= after12ClockOffset;
			pointX = static_cast<int>(ctx.chartWidth / 240.0f * countX);
		}
		else
			pointX = i * (ctx.chartWidth / totalPoints);

		float yVal = (item.price - minPrice) * unitY;
		dataPoints.push_back(CPoint(pointX, (int)yVal));
	}

	if (!dataPoints.empty())
	{
		int startY = ctx.priceChartTop + ctx.priceChartHeight - (int)((ctx.realtimeData.openPrice - minPrice) * unitY);
		memDC.MoveTo(0, startY);
		for (const auto& pt : dataPoints)
			memDC.LineTo(pt.x, ctx.priceChartTop + ctx.priceChartHeight - pt.y);

		// 均价线
		CPen avgLinePen(PS_SOLID, 1, COLOR_GOLDEN);
		CPen* pOldAvgPen = memDC.SelectObject(&avgLinePen);

		std::vector<CPoint> avgPoints;
		avgPoints.reserve(timelinePoint.size());
		for (int i = 0; i < totalPoints; i++)
		{
			const auto& item = timelinePoint[i];
			std::vector<std::string> time_arr = CCommon::split(item.time, ":");
			int pointX = 0;
			if (time_arr.size() >= 2)
			{
				int hour = _ttoi(CString(time_arr[0].c_str()));
				int minute = _ttoi(CString(time_arr[1].c_str()));
				int countX = hour * 60 + minute;
				if (hour < 12) countX -= before12ClockOffset;
				else if (hour >= 13) countX -= after12ClockOffset;
				pointX = static_cast<int>(ctx.chartWidth / 240.0f * countX);
			}
			else
				pointX = i * (ctx.chartWidth / totalPoints);

			float yVal = (item.averagePrice - minPrice) * unitY;
			avgPoints.push_back(CPoint(pointX, (int)yVal));
		}

		if (!avgPoints.empty())
		{
			int avgStartY = ctx.priceChartTop + ctx.priceChartHeight - (int)((ctx.realtimeData.openPrice - minPrice) * unitY);
			memDC.MoveTo(0, avgStartY);
			for (const auto& pt : avgPoints)
				memDC.LineTo(pt.x, ctx.priceChartTop + ctx.priceChartHeight - pt.y);
		}
		memDC.SelectObject(pOldAvgPen);
	}

	// 时间标签
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	struct TimeMarker {
		const TCHAR* label;
		int minutesFromStart;
	};
	const TimeMarker timeMarkers[] = {
		{ _T("9:30"), 0 },
		{ _T("10:30"), 60 },
		{ _T("11:30"), 120 },
		{ _T("14:00"), 180 },
		{ _T("15:00"), 240 }
	};
	for (int i = 0; i < sizeof(timeMarkers) / sizeof(timeMarkers[0]); i++)
	{
		int xPos = ctx.chartWidth * timeMarkers[i].minutesFromStart / 240;
		CString timeLabel(timeMarkers[i].label);
		CSize labelSize = memDC.GetTextExtent(timeLabel);
		int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
		memDC.TextOut(labelX, ctx.priceChartTop + ctx.priceChartHeight + g_data.RDPI(2), timeLabel);
	}
}

void CFloatingWnd::DrawTimelineHoverOverlay(CDC& memDC, const TimelineDrawContext& ctx)
{
	if (!m_isHoveringVolume || m_hoveredBarIndex < 0)
		return;

	const auto& timelinePoint = *ctx.timelinePoint;
	if (m_hoveredBarIndex >= static_cast<int>(timelinePoint.size()))
		return;

	const int before12ClockOffset = 570;
	const int after12ClockOffset = 660;

	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;

	// 计算价格范围（涨跌停限制）
	STOCK::Price maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
	STOCK::Price minPrice = ctx.realtimeData.prevClosePrice - priceLimit;

	// 在顶部和底部各预留10个像素的空间
	const int pricePaddingY = g_data.RDPI(10);
	double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
	maxPrice += paddingPrice;
	minPrice -= paddingPrice;

	float unitY = ctx.priceChartHeight / (maxPrice - minPrice);

	// 重新计算悬停点位置
	const auto& item = timelinePoint[m_hoveredBarIndex];
	std::vector<std::string> time_arr = CCommon::split(item.time, ":");
	int hoverX = 0;
	if (time_arr.size() >= 2)
	{
		int hour = _ttoi(CString(time_arr[0].c_str()));
		int minute = _ttoi(CString(time_arr[1].c_str()));
		int countX = hour * 60 + minute;
		if (hour < 12) countX -= before12ClockOffset;
		else if (hour >= 13) countX -= after12ClockOffset;
		hoverX = static_cast<int>(ctx.chartWidth / 240.0f * countX);
	}

	int dotY = ctx.priceChartTop + ctx.priceChartHeight - (int)((item.price - minPrice) * unitY);

	COLORREF dotColor = (item.price >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;

	CPen dotPen(PS_SOLID, 4, dotColor);
	CPen* pOldPen = memDC.SelectObject(&dotPen);
	memDC.Ellipse(hoverX - 3, dotY - 3, hoverX + 3, dotY + 3);

	// 均价圆点
	float avgPrice = item.averagePrice;
	int avgDotY = ctx.priceChartTop + ctx.priceChartHeight - (int)((avgPrice - minPrice) * unitY);
	COLORREF avgDotColor = (avgPrice >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	CPen avgDotPen(PS_SOLID, 4, avgDotColor);
	memDC.SelectObject(&avgDotPen);
	memDC.Ellipse(hoverX - 3, avgDotY - 3, hoverX + 3, avgDotY + 3);

	// 十字竖线（延伸到量柱图底部）
	CPen crossPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
	memDC.SelectObject(&crossPen);
	memDC.MoveTo(hoverX, ctx.priceChartTop);
	memDC.LineTo(hoverX, ctx.volumeChartTop + ctx.volumeChartHeight);

	// 悬停提示文字 - 显示在圆点右侧
	double diff = item.price - ctx.realtimeData.prevClosePrice;
	double diffPercent = ctx.realtimeData.prevClosePrice != 0 ? (diff / ctx.realtimeData.prevClosePrice) * 100 : 0;

	CString priceTxt = CCommon::FormatFloat(item.price);
	CString diffAmountTxt = CCommon::FormatFloat(std::abs(diff));
	CString diffTxt;
	if (diff >= 0)
		diffTxt.Format(_T("+%.2f%%(+%s)"), diffPercent, diffAmountTxt);
	else
		diffTxt.Format(_T("%.2f%%(-%s)"), diffPercent, diffAmountTxt);

	CSize priceSize = memDC.GetTextExtent(priceTxt);
	CSize diffSize = memDC.GetTextExtent(diffTxt);
	CString space = _T(" ");
	CSize spaceSize = memDC.GetTextExtent(space);
	int totalWidth = priceSize.cx + spaceSize.cx + diffSize.cx;

	// 文字位置：圆点右侧，向上偏移避免遮挡趋势线
	int textX = hoverX + g_data.RDPI(8);
	int textY = dotY - priceSize.cy / 2;

	// 如果右侧空间不足，改为显示在圆点左侧
	if (textX + totalWidth > ctx.chartWidth - g_data.RDPI(2))
	{
		textX = hoverX - totalWidth - g_data.RDPI(8);
	}

	// 限制在走势图区域内
	textY = max(ctx.priceChartTop + g_data.RDPI(2), min(textY, ctx.priceChartTop + ctx.priceChartHeight - priceSize.cy - g_data.RDPI(2)));

	memDC.SetTextColor(dotColor);
	memDC.TextOut(textX, textY, priceTxt);
	memDC.TextOut(textX + priceSize.cx + spaceSize.cx, textY, diffTxt);

	// 均价及涨跌幅、涨跌额显示在均价线圆点右侧
	CString avgTxt = CCommon::FormatFloat(avgPrice);
	double avgDiff = avgPrice - ctx.realtimeData.prevClosePrice;
	double avgDiffPercent = ctx.realtimeData.prevClosePrice != 0 ? (avgDiff / ctx.realtimeData.prevClosePrice) * 100 : 0;
	CString avgDiffTxt;
	if (avgDiff >= 0)
		avgDiffTxt.Format(_T("+%.2f%%(+%s)"), avgDiffPercent, CCommon::FormatFloat(std::abs(avgDiff)));
	else
		avgDiffTxt.Format(_T("%.2f%%(-%s)"), avgDiffPercent, CCommon::FormatFloat(std::abs(avgDiff)));

	CSize avgSize = memDC.GetTextExtent(avgTxt);
	CSize avgDiffSize = memDC.GetTextExtent(avgDiffTxt);
	int avgTotalWidth = avgSize.cx + spaceSize.cx + avgDiffSize.cx;
	int avgTextX = hoverX + g_data.RDPI(8);
	int avgTextY = avgDotY - avgSize.cy / 2;
	if (avgTextX + avgTotalWidth > ctx.chartWidth - g_data.RDPI(2))
		avgTextX = hoverX - avgTotalWidth - g_data.RDPI(8);
	avgTextY = max(ctx.priceChartTop + g_data.RDPI(2), min(avgTextY, ctx.priceChartTop + ctx.priceChartHeight - avgSize.cy - g_data.RDPI(2)));
	memDC.SetTextColor(COLOR_GOLDEN);
	memDC.TextOut(avgTextX, avgTextY, avgTxt);
	memDC.TextOut(avgTextX + avgSize.cx + spaceSize.cx, avgTextY, avgDiffTxt);

	// 时间标签显示在竖线对应的X轴下方
	CString timeStr(item.time.c_str());
	if (timeStr.GetLength() >= 5)
		timeStr = timeStr.Left(5);
	CSize timeSize = memDC.GetTextExtent(timeStr);
	int timeLabelX = hoverX - timeSize.cx / 2;
	timeLabelX = max(g_data.RDPI(2), min(timeLabelX, ctx.chartWidth - timeSize.cx - g_data.RDPI(2)));
	int timeLabelY = ctx.priceChartTop + ctx.priceChartHeight + g_data.RDPI(2);
	memDC.SetTextColor(COLOR_BLACK);
	memDC.TextOut(timeLabelX, timeLabelY, timeStr);

	memDC.SelectObject(pOldPen);
}

void CFloatingWnd::DrawTimelineVolumeSection(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawVolumeChart(memDC, 0, ctx.volumeChartTop, ctx.chartWidth, ctx.volumeChartHeight, *ctx.timelinePoint, &ctx.realtimeData);

	// 时间竖线和平均值参考线
	CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
	CPen* pOldVolPen = memDC.SelectObject(&pGrid);

	int volumeY = ctx.volumeChartTop;
	memDC.MoveTo(0, volumeY);
	memDC.LineTo(ctx.chartWidth, volumeY);

	const auto& timelinePoint = *ctx.timelinePoint;
	if (!timelinePoint.empty())
	{
		STOCK::Volume totalVolume = 0;
		for (const auto& item : timelinePoint)
			totalVolume += item.volume;
		float avgVolume = static_cast<float>(totalVolume) / static_cast<float>(timelinePoint.size());

		STOCK::Volume maxVolume = 0;
		for (const auto& item : timelinePoint)
		{
			if (item.volume > maxVolume)
				maxVolume = item.volume;
		}

		if (maxVolume > 0)
		{
			float ratios[] = { 0.5f, 1.0f, 2.5f, 5.0f };
			memDC.SetTextColor(COLOR_GRAY_TEXT);
			CPen dashPen(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);

			for (float ratio : ratios)
			{
				float targetVolume = avgVolume * ratio;
				float yRatio = min(1.0f, targetVolume / static_cast<float>(maxVolume));
				int yPos = volumeY + ctx.volumeChartHeight - static_cast<int>(yRatio * ctx.volumeChartHeight);

				memDC.SelectObject(ratio == 1.0f ? static_cast<CPen*>(&dashPen) : &pGrid);
				memDC.MoveTo(0, yPos);
				memDC.LineTo(ctx.chartWidth, yPos);

				CString labelTxt;
				if (ratio == 0.5f) labelTxt = _T("0.5");
				else if (ratio == 1.0f) labelTxt = _T("avg");
				else if (ratio == 2.5f) labelTxt = _T("2.5");
				else if (ratio == 5.0f) labelTxt = _T("5.0");

				CSize labelSize = memDC.GetTextExtent(labelTxt);
				memDC.TextOut(ctx.chartWidth - labelSize.cx - g_data.RDPI(5), yPos - labelSize.cy / 2, labelTxt);
			}
		}
	}

	struct TimeMarker {
		const TCHAR* label;
		int minutesFromStart;
	};
	const TimeMarker timeMarkers[] = {
		{ _T("9:30"), 0 },
		{ _T("10:30"), 60 },
		{ _T("11:30"), 120 },
		{ _T("14:00"), 180 },
		{ _T("15:00"), 240 }
	};
	for (int i = 0; i < sizeof(timeMarkers) / sizeof(timeMarkers[0]); i++)
	{
		int xPos = ctx.chartWidth * timeMarkers[i].minutesFromStart / 240;
		memDC.MoveTo(xPos, volumeY);
		memDC.LineTo(xPos, volumeY + ctx.volumeChartHeight);
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
	// 分时图模式下大盘与个股一致显示盘口和量柱图，仅日K图模式下大盘不显示
	bool isIndexKLine = isIndex && m_isKLineMode;

	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = w - orderBookWidth;

	const int headerHeight = g_data.RDPI(26);
	const int maLineHeight = g_data.RDPI(18);
	const int maTitleHeight = g_data.RDPI(20);
	const int volumeTitleHeight = g_data.RDPI(20);
	const int gapBetweenCharts = g_data.RDPI(18);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int positionInfoHeight = isIndexKLine ? 0 : g_data.RDPI(22);
	const int scrollBarHeight = g_data.RDPI(18);

	int priceChartHeight, volumeChartHeight;
	if (isIndexKLine)
	{
		// 大盘日K图：价格图表 + X轴标签 + 滚动条
		priceChartHeight = h - headerHeight - xAxisLabelHeight - scrollBarHeight - g_data.RDPI(4);
		volumeChartHeight = 0;
	}
	else if (m_showTrendView)
	{
		priceChartHeight = h * 2 / 3 - headerHeight - gapBetweenCharts / 2 - xAxisLabelHeight;
		volumeChartHeight = h - priceChartHeight - headerHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight;
	}
	else if (!m_isKLineMode)
	{
		// 分时图模式：标题栏 + MA均线行(20) + 走势图 + X轴标签行(20) + 量柱标题行(20) + 量柱图 + 底部持仓
		priceChartHeight = h * 2 / 3 - headerHeight - maTitleHeight;
		volumeChartHeight = h - priceChartHeight - headerHeight - maTitleHeight - xAxisLabelHeight - volumeTitleHeight - positionInfoHeight;
	}
	else
	{
		priceChartHeight = h * 2 / 3 - headerHeight - maLineHeight - gapBetweenCharts / 2 - xAxisLabelHeight;
		volumeChartHeight = h - priceChartHeight - headerHeight - maLineHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight;
	}

	const int priceChartTop = !m_isKLineMode ? headerHeight + maTitleHeight : headerHeight;

	STOCK::StockInfo realtimeData;
	std::vector<STOCK::TimelinePoint> timelinePoint;
	std::vector<STOCK::KLinePoint> klineData;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(m_stock_id);
		if (stockData)
		{
			realtimeData = stockData->info;
			if (m_isKLineMode)
			{
				auto klineObj = stockData->getKLineData();
				if (klineObj)
				{
					klineData = klineObj->data;
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

	if (m_isKLineMode && !klineData.empty())
	{
		if (!isIndexKLine)
		{
			int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
			int maxVisibleKlines = min(displayCount, chartWidth / 3);
			if (displayCount > maxVisibleKlines)
			{
				volumeChartHeight = h - priceChartHeight - headerHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight - scrollBarHeight;
			}
		}
	}
	else if (!isIndexKLine)
	{
		if (!m_isKLineMode)
		{
			volumeChartHeight = h - priceChartHeight - headerHeight - maTitleHeight - xAxisLabelHeight - volumeTitleHeight - positionInfoHeight;
		}
		else
		{
			volumeChartHeight = h - priceChartHeight - headerHeight - maLineHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight;
		}
	}

	if (!m_isOverviewMode)
	{
		if (!m_isKLineMode && m_hScrollBar.GetSafeHwnd())
			m_hScrollBar.ShowWindow(SW_HIDE);

		if (m_isKLineMode && !klineData.empty())
		{
			CPen pKLine(PS_SOLID, 1, COLOR_DARK_GRAY_BORDER);
			memDC.SelectObject(&pKLine);

			DrawHeader(memDC, realtimeData, w, headerHeight);

			CPen pGrid(PS_SOLID, 1, COLOR_GRAY_GRID);
			CPen* pOldPen = memDC.SelectObject(&pGrid);

			// K线模式不需要绘制MA和成本线等，这些在DrawKLineChart中处理
			memDC.SelectObject(pOldPen);
		}
		else if (!timelinePoint.empty())
		{
			TimelineDrawContext ctx;
			ctx.chartWidth = chartWidth;
			ctx.windowWidth = w;
			ctx.chartHeight = h;
			ctx.priceChartTop = priceChartTop;
			ctx.priceChartHeight = priceChartHeight;
			ctx.volumeChartTop = priceChartTop + priceChartHeight + xAxisLabelHeight + volumeTitleHeight;
			ctx.volumeChartHeight = volumeChartHeight;
			ctx.positionY = ctx.volumeChartTop + volumeChartHeight + g_data.RDPI(4);
			ctx.realtimeData = realtimeData;
			ctx.timelinePoint = &timelinePoint;
			ctx.klineData = &klineData;

			DrawTimelineHeader(memDC, ctx);
			DrawTimelineGridAndLines(memDC, ctx);
			DrawTimelinePriceCurve(memDC, ctx);
			DrawTimelineVolumeSection(memDC, ctx);
			DrawTimelineHoverOverlay(memDC, ctx);
			DrawTimelinePositionInfo(memDC, ctx);
			DrawOrderBook(memDC, chartWidth, w, h, realtimeData, klineData);
		}
		else if (!m_isKLineMode)
		{
			CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&pMiddleLine);
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			memDC.TextOut((chartWidth - memDC.GetTextExtent(loading_state_txt).cx) / 2, headerHeight + g_data.RDPI(10), loading_state_txt);
		}
	} // end if (!m_isOverviewMode)

	if (m_isKLineMode && !klineData.empty())
	{
		if (m_showTrendView)
		{
			DrawKLineTrendChart(memDC, 0, priceChartTop, chartWidth, priceChartHeight, klineData, realtimeData);
		}
		else
		{
			DrawKLineChart(memDC, 0, priceChartTop, chartWidth, priceChartHeight, klineData, realtimeData);
		}

		if (!isIndexKLine)
		{
			DrawKLineVolumeChart(memDC, 0, headerHeight + priceChartHeight + gapBetweenCharts + g_data.RDPI(18), chartWidth, volumeChartHeight, klineData);

			int positionY = headerHeight + priceChartHeight + gapBetweenCharts + g_data.RDPI(18) + volumeChartHeight + g_data.RDPI(4);
			DrawKLinePositionInfo(memDC, 0, positionY, chartWidth, realtimeData);
			DrawKLineInfoPanel(memDC, chartWidth, w, positionY, realtimeData, klineData);

			if (m_hScrollBar.GetSafeHwnd())
			{
				const int minBarWidth = 7;
				const int gap = 1;
				int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
				int maxVisibleKlines = min(displayCount, chartWidth / (minBarWidth + gap));
				int scrollRange = max(0, displayCount - maxVisibleKlines);
				if (scrollRange > 0)
				{
					int scrollBarY = positionY + positionInfoHeight + g_data.RDPI(2);
					m_hScrollBar.MoveWindow(0, scrollBarY, chartWidth, g_data.RDPI(14), TRUE);
					SCROLLINFO si = { sizeof(SCROLLINFO) };
					si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
					si.nMin = 0;
					si.nMax = scrollRange + maxVisibleKlines;
					si.nPage = maxVisibleKlines;
					si.nPos = min(m_scrollOffset, scrollRange);
					m_hScrollBar.SetScrollInfo(&si, FALSE);
					m_hScrollBar.ShowWindow(SW_SHOW);
				}
				else
				{
					m_hScrollBar.ShowWindow(SW_HIDE);
				}
			}
		}
		else if (m_hScrollBar.GetSafeHwnd())
		{
			// 大盘日K图也需要滚动条
			const int minBarWidth = 7;
			const int gap = 1;
			int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
			int maxVisibleKlines = min(displayCount, chartWidth / (minBarWidth + gap));
			int scrollRange = max(0, displayCount - maxVisibleKlines);
			if (scrollRange > 0)
			{
				int scrollBarY = headerHeight + priceChartHeight + xAxisLabelHeight + g_data.RDPI(2);
				m_hScrollBar.MoveWindow(0, scrollBarY, chartWidth, g_data.RDPI(14), TRUE);
				SCROLLINFO si = { sizeof(SCROLLINFO) };
				si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
				si.nMin = 0;
				si.nMax = scrollRange + maxVisibleKlines;
				si.nPage = maxVisibleKlines;
				si.nPos = min(m_scrollOffset, scrollRange);
				m_hScrollBar.SetScrollInfo(&si, FALSE);
				m_hScrollBar.ShowWindow(SW_SHOW);
			}
			else
			{
				m_hScrollBar.ShowWindow(SW_HIDE);
			}
		}

		// 在所有K线图绘制完成后，绘制悬停日期标签（避免被后续绘制覆盖）
		if (m_isHoveringKLine || m_isHoveringKLineVolume)
		{
			int hoveredIdx = m_klineHoveredBarIndex;
			if (hoveredIdx >= 0 && hoveredIdx < (int)klineData.size())
			{
				const auto& item = klineData[hoveredIdx];
				CString dateStr(item.day.c_str());
				dateStr.Replace(_T("-"), _T("/"));
				CSize dateSize = memDC.GetTextExtent(dateStr);

				// 计算日期标签的X位置（与K线柱对齐）
				int dateX = 0;
				if (!m_showTrendView)
				{
					const int paddingY = g_data.RDPI(10);
					KLineDrawData drawData = PrepareKLineDrawData(0, priceChartTop + paddingY, chartWidth, priceChartHeight - paddingY * 2, klineData);
					int barX = drawData.x + (hoveredIdx - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
					dateX = barX + drawData.barWidth / 2 - dateSize.cx / 2;
				}
				else
				{
					// 走势图模式
					const int paddingY = g_data.RDPI(10);
					KLineDrawData drawData = PrepareKLineDrawData(0, priceChartTop + paddingY, chartWidth, priceChartHeight - paddingY * 2, klineData);
					int barX = drawData.x + (hoveredIdx - drawData.finalStartIndex) * (drawData.barWidth + drawData.gap);
					dateX = barX + drawData.barWidth / 2 - dateSize.cx / 2;
				}

				dateX = max(g_data.RDPI(2), min(dateX, chartWidth - dateSize.cx - g_data.RDPI(2)));
				int dateY = priceChartTop + priceChartHeight + g_data.RDPI(2);
				memDC.SetTextColor(COLOR_BLACK);
				memDC.TextOut(dateX, dateY, dateStr);
			}
		}
	}

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

void CFloatingWnd::DrawVolumeChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const STOCK::StockInfo* stockInfo /* = nullptr */)
{
	if (timelinePoint.empty())
		return;

	STOCK::Volume maxVolume = 0;
	for (const auto& item : timelinePoint)
	{
		if (item.volume > maxVolume)
			maxVolume = item.volume;
	}

	if (maxVolume == 0)
		return;

	const float totalTradingMinutes = 240.0f;
	const int before12ClockOffset = 570;
	const int after12ClockOffset = 660;
	const int barWidth = 2;

	for (int i = 0; i < timelinePoint.size(); i++)
	{
		const auto& item = timelinePoint[i];

		int barX = x;
		std::vector<std::string> time_arr = CCommon::split(item.time, ":");
		if (time_arr.size() >= 2)
		{
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
			barX = x + static_cast<int>(width / 240.0f * countX);
		}
		else
		{
			barX = x + i * (width / static_cast<int>(timelinePoint.size()));
		}

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

	STOCK::Volume totalVolume = 0;
	for (const auto& item : timelinePoint)
	{
		totalVolume += item.volume;
	}

	CString volumeTxt;
	STOCK::Volume totalVolumeInLots = totalVolume / 100;
	volumeTxt.Format(_T("总量: %s"), CCommon::FormatVolumeInt(totalVolumeInLots));

	CString turnoverTxt;
	if (stockInfo)
	{
		turnoverTxt.Format(_T(" 总额: %s"), CCommon::FormatAmount(stockInfo->turnover));
	}

	// 鼠标悬停时追加当前柱子的成交量和成交额
	CString hoverTxt;
	if (m_isHoveringVolume && m_hoveredBarIndex >= 0 && m_hoveredBarIndex < timelinePoint.size())
	{
		const auto& hoverItem = timelinePoint[m_hoveredBarIndex];
		STOCK::Volume hoverVolLots = hoverItem.volume / 100;
		double hoverAmount = static_cast<double>(hoverItem.volume) * hoverItem.price;
		hoverTxt.Format(_T(" (%s %s)"), CCommon::FormatVolumeInt(hoverVolLots), CCommon::FormatAmount(hoverAmount));
	}

	CString fullTxt = volumeTxt + turnoverTxt + hoverTxt;

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	const int vTitleHeight = g_data.RDPI(20);
	CRect volumeRect(x, y - vTitleHeight, x + width, y);
	memDC.DrawText(fullTxt, volumeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	if (m_isHoveringVolume && m_hoveredBarIndex >= 0 && m_hoveredBarIndex < timelinePoint.size())
	{
		const auto& item = timelinePoint[m_hoveredBarIndex];
		int barX = x;
		std::vector<std::string> time_arr = CCommon::split(item.time, ":");
		if (time_arr.size() >= 2)
		{
			int hour = _ttoi(CString(time_arr[0].c_str()));
			int minute = _ttoi(CString(time_arr[1].c_str()));
			int countX = hour * 60 + minute;

			if (hour < 12)
			{
				countX -= 570;
			}
			else if (hour >= 13)
			{
				countX -= 660;
			}
			barX = x + static_cast<int>(width / 240.0f * countX);
		}
		else
		{
			barX = x + m_hoveredBarIndex * (width / static_cast<int>(timelinePoint.size()));
		}

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

		// 绘制十字竖线
		CPen crossPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
		pOldPen = memDC.SelectObject(&crossPen);
		memDC.MoveTo(barX + barWidth / 2, y);
		memDC.LineTo(barX + barWidth / 2, y + height);
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
	for (int i = 0; i <= 4; i++)
	{
		int gridY = drawData.y + static_cast<int>(i * drawData.h / 4.0f);
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
	CString maxPriceTxt = CCommon::FormatFloat(drawData.maxPrice);
	memDC.TextOut(drawData.x + g_data.RDPI(2), drawData.y + g_data.RDPI(2), maxPriceTxt);

	CString minPriceTxt = CCommon::FormatFloat(drawData.minPrice);
	memDC.TextOut(drawData.x + g_data.RDPI(2), drawData.y + drawData.h - memDC.GetTextExtent(minPriceTxt).cy - g_data.RDPI(2), minPriceTxt);
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

	// 重新计算最高最低价，过滤无效价格，使用固定周期范围（不随滚动条变化）
	drawData.maxPrice = 0;
	drawData.minPrice = (std::numeric_limits<STOCK::Price>::max)();
	for (int i = drawData.startIndex; i < klineData.size(); i++)
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

	// 上下各预留20像素内部边距，通过扩大价格范围实现
	if (drawData.maxPrice > drawData.minPrice)
	{
		const int pricePaddingY = g_data.RDPI(20);
		double paddingPrice = (drawData.maxPrice - drawData.minPrice) * pricePaddingY / drawData.h;
		drawData.maxPrice += paddingPrice;
		drawData.minPrice -= paddingPrice;
		drawData.unitY = drawData.h / (drawData.maxPrice - drawData.minPrice);
	}

	if (drawData.maxPrice <= drawData.minPrice) { memDC.RestoreDC(-1); return; }

	// 绘制各部分
	DrawKLineGrid(memDC, drawData);
	DrawYearAverageLines(memDC, drawData);
	DrawPriceLabels(memDC, drawData);
	DrawMAIndicators(memDC, drawData);
	std::vector<LabelInfo> labelInfos = DrawKLineMonthLines(memDC, drawData);
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

	// 重新计算最高最低价，过滤无效价格，使用固定周期范围（不随滚动条变化）
	drawData.maxPrice = 0;
	drawData.minPrice = (std::numeric_limits<STOCK::Price>::max)();
	for (int i = drawData.startIndex; i < klineData.size(); i++)
	{
		if (klineData[i].high > 0)
			drawData.maxPrice = max(drawData.maxPrice, klineData[i].high);
		if (klineData[i].low > 0)
			drawData.minPrice = min(drawData.minPrice, klineData[i].low);
	}

	PeriodPoint periodHighs[3] = {};
	PeriodPoint periodLows[3] = {};
	CalculatePeriodHighsLows(drawData, periodHighs, periodLows, true);

	// 上下各预留20像素内部边距，通过扩大价格范围实现
	if (drawData.maxPrice > drawData.minPrice)
	{
		const int pricePaddingY = g_data.RDPI(20);
		double paddingPrice = (drawData.maxPrice - drawData.minPrice) * pricePaddingY / drawData.h;
		drawData.maxPrice += paddingPrice;
		drawData.minPrice -= paddingPrice;
		drawData.unitY = drawData.h / (drawData.maxPrice - drawData.minPrice);
	}

	if (drawData.maxPrice <= drawData.minPrice) { memDC.RestoreDC(-1); return; }

	DrawKLineGrid(memDC, drawData);
	DrawYearAverageLines(memDC, drawData);
	DrawPriceLabels(memDC, drawData);
	DrawMAIndicators(memDC, drawData);
	DrawCurrentPriceLine(memDC, drawData);

	std::vector<LabelInfo> labelInfos = DrawKLineMonthLines(memDC, drawData);

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
	const int rowHeight = height / 18;
	const int topOffset = rowHeight;  // 顶部空出一行给标题栏

	memDC.SetBkMode(TRANSPARENT);

	STOCK::Volume innerVol = stockInfo.innerVolume / 100;
	STOCK::Volume outerVol = stockInfo.outerVolume / 100;

	CString innerStr = CCommon::FormatVolumeInt(innerVol);
	CString outerStr = CCommon::FormatVolumeInt(outerVol);

	int textX = left + g_data.RDPI(5) + 3;

	memDC.SetTextColor(COLOR_GREEN_DOWN);
	CString innerTxt;
	innerTxt.Format(_T("内盘: %s"), innerStr);
	if (innerVol > outerVol) innerTxt += _T("⬆");
	memDC.TextOut(textX, topOffset + g_data.RDPI(2), innerTxt);

	memDC.SetTextColor(COLOR_RED_UP);
	CString outerTxt;
	outerTxt.Format(_T("外盘: %s"), outerStr);
	if (outerVol > innerVol) outerTxt += _T("⬆");
	memDC.TextOut(textX, topOffset + rowHeight + g_data.RDPI(2), outerTxt);

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	CString turnoverTxt;
	turnoverTxt.Format(_T("换手率: %.2f%%"), stockInfo.turnoverRate);
	if (stockInfo.turnoverRate >= 5)
		memDC.SetTextColor(COLOR_RED_UP);
	else
		memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(textX, topOffset + rowHeight * 2 + g_data.RDPI(2), turnoverTxt);

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

	CString wbTxt;
	if (wbRatio >= 0)
	{
		wbTxt.Format(_T("委比: +%.2f%%"), wbRatio);
	}
	else
	{
		wbTxt.Format(_T("委比: %.2f%%"), wbRatio);
	}

	int wbX = left + g_data.RDPI(5) + 3;

	if (wbRatio >= 0)
		memDC.SetTextColor(COLOR_RED_UP);
	else
		memDC.SetTextColor(COLOR_GREEN_DOWN);
	CPen pDashLine(PS_DASH, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&pDashLine);
	memDC.MoveTo(left, topOffset + rowHeight * 3);
	memDC.LineTo(right, topOffset + rowHeight * 3);

	memDC.TextOut(wbX, topOffset + rowHeight * 3 + g_data.RDPI(2), wbTxt);

	for (int i = 0; i < MAX_LEVEL; i++)
	{
		if (i < MAX_LEVEL - 3)
			continue;
		int idx = MAX_LEVEL - 1 - i;
		STOCK::Price price = stockInfo.askLevels[idx].price;
		STOCK::Volume volume = stockInfo.askLevels[idx].volume / 100;

		memDC.SetTextColor(COLOR_RED_UP);
		CString askTxt;

		CString volumeStr = CCommon::FormatVolumeInt(volume);

		CString priceStr = CCommon::FormatFloat(price);
		askTxt.Format(_T("卖[%d]: %s %s"), idx + 1, priceStr, volumeStr);
		int y = topOffset + (i - MAX_LEVEL + 3 + 4) * rowHeight;
		memDC.TextOut(left + g_data.RDPI(5) + 3, y + g_data.RDPI(2), askTxt);
	}

	for (int i = 0; i < MAX_LEVEL; i++)
	{
		if (i >= 3)
			continue;
		STOCK::Price price = stockInfo.bidLevels[i].price;
		STOCK::Volume volume = stockInfo.bidLevels[i].volume / 100;

		memDC.SetTextColor(COLOR_GREEN_DOWN);
		CString bidTxt;

		CString volumeStr = CCommon::FormatVolumeInt(volume);

		CString priceStr = CCommon::FormatFloat(price);
		bidTxt.Format(_T("买[%d]: %s %s"), i + 1, priceStr, volumeStr);
		int y = topOffset + (7 + i) * rowHeight;
		memDC.TextOut(left + g_data.RDPI(5) + 3, y + g_data.RDPI(2), bidTxt);
	}

	if (!klineData.empty())
	{
		CPen pDashAmp(PS_DASH, 1, COLOR_GRAY_GRID);
		memDC.SelectObject(&pDashAmp);
		memDC.MoveTo(left, topOffset + rowHeight * 10);
		memDC.LineTo(right, topOffset + rowHeight * 10);

		int periodDays[] = { 5, 13, 34, 55 };
		auto stockDataPtr2 = g_data.GetStockData(m_stock_id);
		auto* klinePtr2 = stockDataPtr2 ? stockDataPtr2->getKLineData() : nullptr;
		for (int p = 0; p < 4; p++)
		{
			int days = periodDays[p];
			double avgAmplitude = klinePtr2 ? klinePtr2->CalculateAverageAmplitude(days) : 0;

			CString amplitudeTxt;
			if (avgAmplitude > 0)
			{
				amplitudeTxt.Format(_T("振幅%02d: %.2f%%"), days, avgAmplitude);
			}
			else
			{
				amplitudeTxt.Format(_T("振幅%02d: --"), days);
			}
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(textX, topOffset + rowHeight * (10 + p) + g_data.RDPI(2), amplitudeTxt);
		}
	}

	// MA5-MA55 均线指标放到最后显示
	{
		CPen pDashMA(PS_DASH, 1, COLOR_GRAY_GRID);
		memDC.SelectObject(&pDashMA);
		memDC.MoveTo(left, topOffset + rowHeight * 14);
		memDC.LineTo(right, topOffset + rowHeight * 14);
	}

	auto stockDataPtr = g_data.GetStockData(m_stock_id);
	auto* klinePtr = stockDataPtr ? stockDataPtr->getKLineData() : nullptr;
	const int maPeriods[] = { 5, 13, 34, 55 };
	for (int m = 0; m < 4; m++)
	{
		int days = maPeriods[m];
		double ma = klinePtr ? klinePtr->CalculateMA(days) : 0;
		CString maTxt;
		COLORREF maColor = COLOR_GRAY_TEXT;
		if (ma > 0)
		{
			maTxt.Format(_T("MA%d: %s"), days, CCommon::FormatFloat(ma));
			if (ma > stockInfo.currentPrice)
				maColor = COLOR_RED_UP;
			else if (ma < stockInfo.currentPrice)
				maColor = COLOR_GREEN_DOWN;
		}
		else
		{
			maTxt.Format(_T("MA%d: --"), days);
		}
		memDC.SetTextColor(maColor);
		memDC.TextOut(textX, topOffset + rowHeight * (14 + m) + g_data.RDPI(2), maTxt);
	}
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
					m_stock_id = rowInfo.code;
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
}

void CFloatingWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDblClk(nFlags, point);
}

void CFloatingWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (!m_isOverviewMode)
	{
		m_isOverviewMode = true;
		m_isKLineMode = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
	else
	{
		m_isOverviewMode = false;
		m_isKLineMode = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
		RequestData();
	}
}

void CFloatingWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	m_mousePos = point;

	CRect rect;
	GetClientRect(&rect);
	bool isIndex = (GetStockPriority(m_stock_id) < 200);
	bool isIndexKLine = isIndex && m_isKLineMode;
	const int orderBookWidth = isIndexKLine ? 0 : ORDER_BOOK_WIDTH;
	const int chartWidth = rect.Width() - orderBookWidth;
	const int headerHeight = g_data.RDPI(26);
	const int maLineHeight = g_data.RDPI(18);
	const int maTitleHeight = g_data.RDPI(20);
	const int volumeTitleHeight = g_data.RDPI(20);
	const int gapBetweenCharts = g_data.RDPI(18);
	const int xAxisLabelHeight = g_data.RDPI(20);
	const int positionInfoHeight = isIndexKLine ? 0 : g_data.RDPI(22);
	const int scrollBarHeight = g_data.RDPI(18);

	int priceChartHeight, volumeChartHeight;
	if (isIndexKLine)
	{
		priceChartHeight = rect.Height() - headerHeight - xAxisLabelHeight - g_data.RDPI(4);
		volumeChartHeight = 0;
	}
	else if (m_showTrendView)
	{
		priceChartHeight = rect.Height() * 2 / 3 - headerHeight - gapBetweenCharts / 2 - xAxisLabelHeight;
		volumeChartHeight = rect.Height() - priceChartHeight - headerHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight;
	}
	else if (!m_isKLineMode)
	{
		priceChartHeight = rect.Height() * 2 / 3 - headerHeight - maTitleHeight;
		volumeChartHeight = rect.Height() - priceChartHeight - headerHeight - maTitleHeight - xAxisLabelHeight - volumeTitleHeight - positionInfoHeight;
	}
	else
	{
		priceChartHeight = rect.Height() * 2 / 3 - headerHeight - maLineHeight - gapBetweenCharts / 2 - xAxisLabelHeight;
		volumeChartHeight = rect.Height() - priceChartHeight - headerHeight - maLineHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight;
	}

	m_isHoveringVolume = false;
	int prevHoveredBarIndex = m_hoveredBarIndex;
	m_hoveredBarIndex = -1;
	bool prevHoveringKLine = m_isHoveringKLine;
	bool prevHoveringKLineVolume = m_isHoveringKLineVolume;
	int prevKlineHoveredBarIndex = m_klineHoveredBarIndex;
	m_isHoveringKLine = false;
	m_isHoveringKLineVolume = false;
	m_klineHoveredBarIndex = -1;

	if (m_isKLineMode)
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

		if (!klineData.empty() && point.x >= 0 && point.x < chartWidth)
		{
			// 使用与绘制完全一致的参数计算，避免鼠标位置偏移
			const int paddingY = g_data.RDPI(10);
			KLineDrawData drawData = PrepareKLineDrawData(0, headerHeight + paddingY, chartWidth, priceChartHeight - paddingY * 2, klineData);

			// K线模式下调整volumeChartHeight（与OnPaint一致）
			if (!isIndexKLine)
			{
				int displayCount = min(m_klinePeriodDays, static_cast<int>(klineData.size()));
				int maxVisibleKlines = min(displayCount, chartWidth / 3);
				if (displayCount > maxVisibleKlines)
				{
					volumeChartHeight = rect.Height() - priceChartHeight - headerHeight - gapBetweenCharts - xAxisLabelHeight - positionInfoHeight - scrollBarHeight;
				}
			}

			if (point.y >= headerHeight && point.y < headerHeight + priceChartHeight)
			{
				// 鼠标在K线图上 - 使用与绘制一致的参数定位
				int barIndex = -1;
				int totalBars = klineData.size() - drawData.finalStartIndex;
				if (totalBars > 0 && drawData.barWidth + drawData.gap > 0)
				{
					barIndex = drawData.finalStartIndex + point.x / (drawData.barWidth + drawData.gap);
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
				int volumeY = headerHeight + priceChartHeight + gapBetweenCharts + g_data.RDPI(18);
				if (point.y >= volumeY && point.y < volumeY + volumeChartHeight)
				{
					// 鼠标在量柱图上 - 使用与绘制一致的参数定位
					int barIndex = -1;
					int totalBars = klineData.size() - drawData.finalStartIndex;
					if (totalBars > 0 && drawData.barWidth + drawData.gap > 0)
					{
						barIndex = drawData.finalStartIndex + point.x / (drawData.barWidth + drawData.gap);
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
				auto timelineData = stockData->getTimelineData();
				if (timelineData)
				{
					timelinePoint = timelineData->data;
				}
			}
		}

		if (!timelinePoint.empty() && point.x >= 0 && point.x < chartWidth)
		{
			const float totalTradingMinutes = 240.0f;
			const int before12ClockOffset = 570;
			const int after12ClockOffset = 660;

			int countX = static_cast<int>(point.x * totalTradingMinutes / chartWidth);
			int targetTime = before12ClockOffset + countX;

			if (countX >= 120)
			{
				targetTime = after12ClockOffset + countX;
			}

			int barIndex = -1;
			int minDiff = INT_MAX;
			for (int i = 0; i < timelinePoint.size(); i++)
			{
				const auto& item = timelinePoint[i];
				std::vector<std::string> time_arr = CCommon::split(item.time, ":");
				if (time_arr.size() >= 2)
				{
					int hour = _ttoi(CString(time_arr[0].c_str()));
					int minute = _ttoi(CString(time_arr[1].c_str()));
					int itemTime = hour * 60 + minute;
					int diff = abs(itemTime - targetTime);
					if (diff < minDiff)
					{
						minDiff = diff;
						barIndex = i;
					}
				}
			}

			if (barIndex >= 0)
			{
				m_isHoveringVolume = true;
				m_hoveredBarIndex = barIndex;
				m_hoveredData = timelinePoint[barIndex];

				CString timeStr(m_hoveredData.time.c_str());
				STOCK::Volume volumeLots = m_hoveredData.volume / 100;
				CString volumeStr = CCommon::FormatVolumeInt(volumeLots);

				double amount = static_cast<double>(m_hoveredData.volume) * m_hoveredData.price;
				CString amountStr = CCommon::FormatAmount(amount);

				m_hoverTip.Format(_T("%s %s %s"), timeStr, volumeStr, amountStr);
			}
			else
			{
				m_isHoveringVolume = false;
				m_hoveredBarIndex = -1;
				m_hoverTip.Empty();
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
		loading_state_txt = g_data.StringRes(IDS_LOADING).GetString();
		AfxBeginThread(NetworkThreadProc, this);
	}
}

void CFloatingWnd::ToggleKLineMode()
{
	m_isKLineMode = !m_isKLineMode;
	m_isOverviewMode = false;
	m_scrollOffset = 0;
	m_showTrendView = true;
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

	if (m_isKLineMode)
	{
		// 不再重置m_klineDataLoaded，因为已在启动时预加载
	}
	Invalidate();
	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
}

void CFloatingWnd::UpdateModeButtons()
{
	if (m_btnTimeLine.GetSafeHwnd() && m_btnKLine.GetSafeHwnd())
	{
		if (m_isOverviewMode)
		{
			m_btnOverview.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
			m_btnTimeLine.SetButtonStyle(BS_FLAT, TRUE);
			m_btnKLine.SetButtonStyle(BS_FLAT, TRUE);
		}
		else if (m_isKLineMode)
		{
			m_btnOverview.SetButtonStyle(BS_FLAT, TRUE);
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			m_btnTimeLine.SetButtonStyle(BS_FLAT, TRUE);
			m_btnKLine.SetButtonStyle(BS_FLAT, TRUE);
		}
		else
		{
			m_btnOverview.SetButtonStyle(BS_FLAT, TRUE);
			m_btnTimeLine.SetWindowText(_T("分时"));
			m_btnKLine.SetWindowText(_T("日K"));
			m_btnTimeLine.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
			m_btnKLine.SetButtonStyle(BS_FLAT, TRUE);
		}

		if (m_btnTrend.GetSafeHwnd())
		{
			if (m_isKLineMode && m_showTrendView)
			{
				m_btnTrend.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
				m_btnKLine.SetButtonStyle(BS_FLAT, TRUE);
			}
			else
			{
				m_btnTrend.SetButtonStyle(BS_FLAT, TRUE);
			}
		}

		if (m_btnMA.GetSafeHwnd())
		{
			if (m_isKLineMode && m_showMA)
			{
				m_btnMA.SetButtonStyle(BS_DEFPUSHBUTTON, TRUE);
			}
			else
			{
				m_btnMA.SetButtonStyle(BS_FLAT, TRUE);
			}
		}
	}
}

void CFloatingWnd::UpdatePeriodComboVisibility()
{
	if (m_comboKLinePeriod.GetSafeHwnd())
	{
		m_comboKLinePeriod.ShowWindow((m_isKLineMode && !m_isOverviewMode) ? SW_SHOW : SW_HIDE);
	}

	if (m_btnTrend.GetSafeHwnd())
	{
		m_btnTrend.ShowWindow((m_isKLineMode && !m_isOverviewMode) ? SW_SHOW : SW_HIDE);
	}

	if (m_btnMA.GetSafeHwnd())
	{
		m_btnMA.ShowWindow((m_isKLineMode && !m_isOverviewMode) ? SW_SHOW : SW_HIDE);
	}
}

void CFloatingWnd::OnCbnSelChangeKLinePeriod()
{
	int sel = m_comboKLinePeriod.GetCurSel();
	switch (sel)
	{
	case 0:
		m_klinePeriodDays = 250;
		break;
	case 1:
		m_klinePeriodDays = 500;
		break;
	case 2:
		m_klinePeriodDays = 750;
		break;
	default:
		m_klinePeriodDays = 250;
		break;
	}

	m_scrollOffset = 0;
	Invalidate();
	PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
}

BOOL CFloatingWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
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

void CFloatingWnd::OnBnClickedOverviewBtn()
{
	if (!m_isOverviewMode)
	{
		m_isOverviewMode = true;
		m_isKLineMode = false;
		m_vScrollOffset = 0;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
	else
	{
		m_isOverviewMode = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedTimeLineBtn()
{
	if (m_isOverviewMode)
	{
		m_isOverviewMode = false;
		m_isKLineMode = false;
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
	}
	else if (m_isKLineMode)
	{
		ToggleKLineMode();
	}
}

void CFloatingWnd::OnBnClickedKLineBtn()
{
	if (m_isOverviewMode)
	{
		m_isOverviewMode = false;
		m_isKLineMode = true;
		m_scrollOffset = 0;
		m_showTrendView = true;
		m_showMA = false;
		m_isHoveringKLine = false;
		m_isHoveringKLineVolume = false;
		m_isHoveringVolume = false;
		m_klineHoveredBarIndex = -1;
		m_hoveredBarIndex = -1;
		m_klineHoverTip.Empty();
		m_hoverTip.Empty();
		m_klineTrendHoverTip.Empty();
		UpdateModeButtons();
		UpdatePeriodComboVisibility();
		Invalidate();
		PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
	}
	else if (!m_isKLineMode)
	{
		ToggleKLineMode();
	}
	else if (m_showTrendView)
	{
		m_showTrendView = false;
		UpdateModeButtons();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedTrendBtn()
{
	if (m_isKLineMode)
	{
		m_showTrendView = !m_showTrendView;
		if (m_btnTrend.GetSafeHwnd())
		{
			m_btnTrend.SetWindowText(m_showTrendView ? _T("走势") : _T("K线"));
		}
		UpdateModeButtons();
		Invalidate();
	}
}

void CFloatingWnd::OnBnClickedMABtn()
{
	if (m_isKLineMode)
	{
		m_showMA = !m_showMA;
		if (m_btnMA.GetSafeHwnd())
		{
			m_btnMA.SetButtonStyle(m_showMA ? BS_DEFPUSHBUTTON : BS_FLAT, TRUE);
		}
		Invalidate();
	}
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

	g_data.RequestTimelineData(pFW->m_stock_id);

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

void CFloatingWnd::OnDestroy()
{
	CWnd::OnDestroy();

	if (m_CTransparentWnd.GetSafeHwnd())
	{
		m_CTransparentWnd.DestroyWindow();
	}
}