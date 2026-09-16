#include "pch.h"
#include "EstVolumeDialog.h"
#include "Common.h"
#include "DataManager.h"
#include <ctime>
#include <cmath>

IMPLEMENT_DYNAMIC(CEstVolumeDialog, CDialogEx)

BEGIN_MESSAGE_MAP(CEstVolumeDialog, CDialogEx)
	ON_BN_CLICKED(IDC_EST_TYPE_BUY, &CEstVolumeDialog::OnBnClickedEstTypeBuy)
	ON_BN_CLICKED(IDC_EST_TYPE_SELL, &CEstVolumeDialog::OnBnClickedEstTypeSell)
	ON_EN_CHANGE(IDC_EST_ORDER_BUY_EDIT, &CEstVolumeDialog::OnEnChangeEstOrderBuyEdit)
	ON_EN_CHANGE(IDC_EST_CUM_BUY_EDIT, &CEstVolumeDialog::OnEnChangeEstCumBuyEdit)
	ON_BN_CLICKED(IDOK, &CEstVolumeDialog::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CEstVolumeDialog::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_RESET_BTN, &CEstVolumeDialog::OnBnClickedResetBtn)
END_MESSAGE_MAP()

CEstVolumeDialog::CEstVolumeDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EST_VOLUME_DIALOG, pParent)
{
}

CEstVolumeDialog::~CEstVolumeDialog()
{
}

void CEstVolumeDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EST_PRICE_EDIT, m_editPrice);
	DDX_Control(pDX, IDC_EST_TYPE_BUY, m_btnBuy);
	DDX_Control(pDX, IDC_EST_TYPE_SELL, m_btnSell);
	DDX_Control(pDX, IDC_EST_TIME_EDIT, m_editTime);
	DDX_Control(pDX, IDC_EST_TOTAL_EDIT, m_editTotal);
	DDX_Control(pDX, IDC_EST_ORDER_BUY_EDIT, m_editOrderBuy);
	DDX_Control(pDX, IDC_EST_ORDER_CUR_EDIT, m_editOrderCur);
	DDX_Control(pDX, IDC_EST_CUM_BUY_EDIT, m_editCumBuy);
	DDX_Control(pDX, IDC_EST_CUM_CUR_EDIT, m_editCumCur);
	DDX_Control(pDX, IDC_EST_EST_BUY_EDIT, m_editEstBuy);
	DDX_Control(pDX, IDC_EST_EST_CUR_EDIT, m_editEstCur);
	DDX_Control(pDX, IDOK, m_btnOk);
	DDX_Control(pDX, IDCANCEL, m_btnCancel);
}

BOOL CEstVolumeDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 窗口定位：贴紧父窗口（FloatingWnd）右边缘无缝隙，高度与其一致，底部对齐
	{
		CRect parentRect, parentClient, dlgRect;
		CWnd* pWnd = GetParent();
		if (pWnd && pWnd->GetSafeHwnd())
		{
			pWnd->GetWindowRect(&parentRect);
			pWnd->GetClientRect(&parentClient);
			pWnd->ClientToScreen(&parentClient);
			GetWindowRect(&dlgRect);

			// 高度与FloatingWnd客户区一致；去掉边框空隙使对话框内容区紧贴
			CRect windowRect = dlgRect;
			CRect clientArea;
			GetClientRect(&clientArea);
			int borderGap = windowRect.Width() - clientArea.Width();  // 对话框左右边框宽
			int x = parentRect.right - borderGap / 2;  // 左移半个边框，内容区左缘贴住父窗口右缘
			int y = parentClient.top;
			int cy = parentClient.Height();
			int cx = windowRect.Width() + borderGap / 2;
			SetWindowPos(nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	// 客户区高度与模板不符时（跟随FloatingWnd高度），把确定/取消按钮重排到客户区底部
	{
		CRect clientArea;
		GetClientRect(&clientArea);
		CRect okRect, cancelRect;
		m_btnOk.GetWindowRect(&okRect);
		m_btnCancel.GetWindowRect(&cancelRect);
		ScreenToClient(&okRect);
		ScreenToClient(&cancelRect);
		int btnBottomMargin = 6;
		int okBottom = clientArea.Height() - btnBottomMargin;
		m_btnOk.MoveWindow(okRect.left, okBottom - okRect.Height(), okRect.Width(), okRect.Height());
		m_btnCancel.MoveWindow(cancelRect.left, okBottom - cancelRect.Height(), cancelRect.Width(), cancelRect.Height());
	}

	// 买入价格：十档行带入当前档价格只读，首行允许输入
	if (m_isOrderRow)
	{
		m_editPrice.SetWindowText(CCommon::FormatFloat(m_price));
		m_editPrice.SetReadOnly(TRUE);
	}
	// 首行：价格框可编辑，默认带入现价（由调用方通过SetPrice传入0表示无）

	// 方向：双击卖盘行=卖出，买盘行=买入（与 m_tradeType 口径一致）
	m_btnBuy.SetCheck(m_tradeType == 0 ? 1 : 0);
	m_btnSell.SetCheck(m_tradeType == 1 ? 1 : 0);

	// 时间：库中有记录取库中记录时间，否则取双击时刻；可编辑（HH:MM），作为保存的输入条件
	// m_editTime.SetReadOnly 不设置，允许用户修改
	if (m_hasSaved)
	{
		tm localTm = {};
		localtime_s(&localTm, &m_savedEst.createTime);
		m_timeText.Format(_T("%02d:%02d"), localTm.tm_hour, localTm.tm_min);
	}
	m_editTime.SetWindowText(m_timeText);

	// 实时列（只读）：挂单量"当前值 / min"、累计成交量"当前值 / 增加值"、第三行剩余排队挂单量
	m_editOrderCur.SetReadOnly(TRUE);
	m_editCumCur.SetReadOnly(TRUE);
	m_editEstCur.SetReadOnly(TRUE);

	CString volText;

	// 买入时列：有当天同价+方向的历史记录时回显（总数量/挂单量/累计成交量），否则带入当前盘口值
	if (m_hasSaved)
	{
		volText.Format(_T("%lld"), static_cast<long long>(m_savedEst.totalAmount));
		m_editTotal.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_savedEst.orderVolume));
		m_editOrderBuy.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_savedEst.cumVolume));
		m_editCumBuy.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_savedEst.estVolume));
		m_editEstBuy.SetWindowText(volText);
	}
	else if (m_isOrderRow)
	{
		// 十档行无历史记录：价格已带入，挂单量/累计成交量填当前值，总数量留空
		volText.Format(_T("%lld"), static_cast<long long>(m_orderVolume));
		m_editOrderBuy.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_cumVolume));
		m_editCumBuy.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_orderVolume + m_cumVolume));
		m_editEstBuy.SetWindowText(volText);
	}
	// 首行且无历史记录：全部留空由用户输入

	UpdateEstVolume();

	// 实时列填充（放在买入时列之后：累计增加值读左侧输入框；CalcRemainQueue 先算，
	// 产出 m_minOrderVolume 供挂单量行与剩余排队行共用）
	long long remain = CalcRemainQueue();
	{
		volText.Format(_T("%lld / %lld"), static_cast<long long>(m_curOrderVolume), m_minOrderVolume);
		m_editOrderCur.SetWindowText(volText);
	}
	{
		CString buyCumText;
		m_editCumBuy.GetWindowText(buyCumText);
		// 增加值 = 当前实时累计 - 买入时累计（左侧输入框当前值）
		volText.Format(_T("%lld / %lld"), static_cast<long long>(m_curCumVolume),
			static_cast<long long>(m_curCumVolume) - _ttol(buyCumText));
		m_editCumCur.SetWindowText(volText);
	}
	{
		volText.Format(_T("%lld"), remain);
		m_editEstCur.SetWindowText(volText);
	}

	return TRUE;
}

// 买入时预计成交累积量 = 挂单量(购入时) + 累计成交量(购入时)，挂单量或累计成交量输入变化时联动
void CEstVolumeDialog::UpdateEstVolume()
{
	CString orderText, cumText, volText;
	m_editOrderBuy.GetWindowText(orderText);
	m_editCumBuy.GetWindowText(cumText);
	long long orderVol = _ttol(orderText);
	long long cumVol = _ttol(cumText);
	volText.Format(_T("%lld"), orderVol + cumVol);
	m_editEstBuy.SetWindowText(volText);
}

// 剩余排队挂单量计算：
//   基准挂单量 = 买入时挂单量 与 [买入时间, 现在] 快照最低挂单量(手) 与 实时挂单量 的最小值
//   剩余排队 = 基准挂单量 + 买入时累计成交量 - 当前累计成交量
// （一次 MIN SQL 直接取最低，无需遍历；快照无数据时基准=买入时挂单量）
long long CEstVolumeDialog::CalcRemainQueue()
{
	// 开单基准与累计（手）：库命中时取库记录，否则十档行取当前带入值，首行取用户输入框当前值
	long long baseOrder = 0;   // 基准挂单量（手）
	long long buyCum = 0;      // 买入时累计成交量（手）
	if (m_hasSaved)
	{
		baseOrder = m_savedEst.orderVolume;
		buyCum = m_savedEst.cumVolume;
	}
	else if (m_isOrderRow)
	{
		baseOrder = m_orderVolume;
		buyCum = m_cumVolume;
	}
	else
	{
		CString orderText, cumText;
		m_editOrderBuy.GetWindowText(orderText);
		m_editCumBuy.GetWindowText(cumText);
		baseOrder = _ttol(orderText);
		buyCum = _ttol(cumText);
	}
	if (baseOrder <= 0)
		return 0;

	// 开单时间 HH:MM → 当日时间戳，作为快照查询起点
	tm localTm = {};
	time_t nowTime;
	time(&nowTime);
	localtime_s(&localTm, &nowTime);
	int hh = 9, mm = 30;
	if (_stscanf_s(m_timeText, _T("%d:%d"), &hh, &mm) == 2)
	{
		hh = max(0, min(23, hh));
		mm = max(0, min(59, mm));
	}
	time_t dayStart = nowTime - static_cast<long long>(localTm.tm_hour) * 3600
		- localTm.tm_min * 60 - localTm.tm_sec;
	time_t startTime = dayStart + hh * 3600 + mm * 60;

	// 快照历史最低挂单量（手）：有更低值吸收撤单；该价位在快照中出现过则MIN有效
	long long snapMin = baseOrder;
	g_data.GetDbManager().LoadMinOrderBookVolume(m_stockCode, startTime, m_price,
		m_tradeType == 1, snapMin);

	// 实时挂单量也参与最低值比较（本分钟未落库部分）
	long long baseMin = min(baseOrder, min(snapMin, m_curOrderVolume));
	m_minOrderVolume = baseMin;   // 供右列挂单量行展示 "当前挂单量 / min"

	return baseMin + buyCum - m_curCumVolume;
}

void CEstVolumeDialog::OnBnClickedEstTypeBuy()
{
	m_tradeType = 0;
}

void CEstVolumeDialog::OnBnClickedEstTypeSell()
{
	m_tradeType = 1;
}

void CEstVolumeDialog::OnEnChangeEstOrderBuyEdit()
{
	UpdateEstVolume();
}

void CEstVolumeDialog::OnEnChangeEstCumBuyEdit()
{
	UpdateEstVolume();
}

void CEstVolumeDialog::OnBnClickedOk()
{
	// 校验必填项
	CString priceText, totalText, orderText, cumText, timeText;
	m_editPrice.GetWindowText(priceText);
	m_editTotal.GetWindowText(totalText);
	m_editOrderBuy.GetWindowText(orderText);
	m_editCumBuy.GetWindowText(cumText);
	m_editTime.GetWindowText(timeText);

	double price = _ttof(priceText);
	long long totalVol = _ttol(totalText);
	long long orderVol = _ttol(orderText);
	long long cumVol = _ttol(cumText);
	long long estVol = orderVol + cumVol;

	// 时间 HH:MM 校验（可编辑，作为记录的输入条件）
	int hh = -1, mm = -1;
	{
		CString trimmed = timeText;
		trimmed.Trim();
		if (_stscanf_s(trimmed, _T("%d:%d"), &hh, &mm) != 2 ||
			hh < 0 || hh > 23 || mm < 0 || mm > 59)
		{
			MessageBox(_T("时间无效，请输入 HH:MM 格式（如 09:35）"), _T("提示"), MB_OK | MB_ICONWARNING);
			return;
		}
	}

	if (price <= 0)
	{
		MessageBox(_T("买入价格无效，请输入大于0的价格"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}
	if (totalVol <= 0)
	{
		MessageBox(_T("总数量无效，请输入大于0的数量"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}
	if (orderVol < 0 || cumVol < 0)
	{
		MessageBox(_T("挂单量/累计成交量无效，不能为负数"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// create_time = 交易日 + 用户输入的 HH:MM（时间与数量一样是输入条件，可修改）
	time_t createTime = 0;
	{
		time_t nowTime;
		time(&nowTime);
		tm localTm = {};
		localtime_s(&localTm, &nowTime);
		localTm.tm_hour = hh;
		localTm.tm_min = mm;
		localTm.tm_sec = 0;
		createTime = std::mktime(&localTm);
	}

	// 只保存"买入时"列信息，实时列不入库
	// 同价格+同方向+同分钟(HH:MM)当天已有记录则更新，否则新增
	CStockDbManager::OrderBookEstimate est;
	est.stockCode = m_stockCode;
	est.tradeDate = CCommon::get_stock_trade_date();
	est.createTime = createTime;
	est.tradeType = m_tradeType;
	est.buyPrice = price;
	est.totalAmount = totalVol;
	est.orderVolume = orderVol;
	est.cumVolume = cumVol;
	est.estVolume = estVol;

	// 查重键带上用户输入的时间（ HH:MM 同分钟判定）
	CT2A timeKeyA(CTime(createTime).Format(_T("%H:%M")));
	CStockDbManager::OrderBookEstimate existEst;
	if (g_data.GetDbManager().LoadLatestOrderBookEstimate(m_stockCode, est.tradeDate, price, m_tradeType, existEst,
		static_cast<const char*>(timeKeyA)))
	{
		// 同价+同方向+同分钟已存在：按id更新，create_time 用用户输入的时间
		est.id = existEst.id;
		if (!g_data.GetDbManager().UpdateOrderBookEstimate(est))
		{
			MessageBox(_T("更新数据库记录失败"), _T("错误"), MB_OK | MB_ICONERROR);
			return;
		}
	}
	else
	{
		if (!g_data.GetDbManager().SaveOrderBookEstimate(est))
		{
			MessageBox(_T("保存到数据库失败"), _T("错误"), MB_OK | MB_ICONERROR);
			return;
		}
	}

	MessageBox(_T("已保存"), _T("提示"), MB_OK | MB_ICONINFORMATION);

	CDialogEx::OnOK();
}

void CEstVolumeDialog::OnBnClickedCancel()
{
	CDialogEx::OnCancel();
}

// 重置按钮：将左侧买入时列重置为当前时刻的盘口信息（总数量除外）：
// 时间=当前时刻、挂单量/累计成交量=双击时的当前盘口值（预计成交累积量联动）；
// 右侧实时列保持不变。仅十档行有盘口数据源；首行只重置时间。
void CEstVolumeDialog::OnBnClickedResetBtn()
{
	// 时间重置为当前时刻
	time_t nowTime;
	time(&nowTime);
	tm localTm = {};
	localtime_s(&localTm, &nowTime);
	CString nowText;
	nowText.Format(_T("%02d:%02d"), localTm.tm_hour, localTm.tm_min);
	m_timeText = nowText;
	m_editTime.SetWindowText(nowText);

	if (m_isOrderRow)
	{
		CString volText;
		volText.Format(_T("%lld"), static_cast<long long>(m_resetOrderVolume));
		m_editOrderBuy.SetWindowText(volText);
		volText.Format(_T("%lld"), static_cast<long long>(m_resetCumVolume));
		m_editCumBuy.SetWindowText(volText);
		UpdateEstVolume();   // 预计成交累积量 = 挂单量 + 累计成交量 联动重算
	}
	// 总数量保持不变；右侧实时列不动
}
