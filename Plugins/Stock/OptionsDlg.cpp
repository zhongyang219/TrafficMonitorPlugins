// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "Common.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(const std::wstring& code, CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_OPTIONS_DIALOG, pParent)
	, m_stock_code(code.c_str())
	, m_alert_low_price(0.0)
	, m_alert_high_price(0.0)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::EnableUpdateBtn(bool enable)
{
	::EnableWindow(GetDlgItem(IDC_UPDATE_BUTTON)->GetSafeHwnd(), enable);
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CODE_EDIT, m_code_edit);
	DDX_Control(pDX, IDC_MARKET_TYPE_COMBO, m_market_type_combo);
	DDX_Text(pDX, IDC_ALERT_LOW_PRICE_EDIT, m_alert_low_price);
	DDX_Text(pDX, IDC_ALERT_HIGH_PRICE_EDIT, m_alert_high_price);
	DDX_Text(pDX, IDC_COST_PRICE_EDIT, m_cost_price);
	DDX_Text(pDX, IDC_HOLDING_COUNT_EDIT, m_holding_count);
	DDX_Text(pDX, IDC_BUY_DATE_EDIT, m_buy_date);
	DDX_Check(pDX, IDC_STATUSBAR_CHECK, m_show_in_statusbar);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
	ON_EN_CHANGE(IDC_CODE_EDIT, &COptionsDlg::OnChangeCodeEdit)
	ON_BN_CLICKED(IDOK, &COptionsDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &COptionsDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_TODAY_BUTTON, &COptionsDlg::OnBnClickedToday)
END_MESSAGE_MAP()

// COptionsDlg 消息处理程序

BOOL COptionsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 定位对话框：挨着参考窗口右侧，底部对齐系统工作区底部
	if (!m_refWndRect.IsRectNull())
	{
		CRect dlgRect;
		GetWindowRect(&dlgRect);
		int dlgW = dlgRect.Width();
		int dlgH = dlgRect.Height();

		// 获取系统工作区（排除任务栏）
		CRect workArea;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

		int x = m_refWndRect.right;
		int y = workArea.bottom - dlgH;

		// 如果右侧放不下，放到左侧
		if (x + dlgW > workArea.right)
			x = m_refWndRect.left - dlgW;

		SetWindowPos(nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	//设置标题
	if (m_stock_code.IsEmpty())
		SetWindowText(g_data.StringRes(IDS_ADD_STOCK));
	else
	{
		CString title = g_data.StringRes(IDS_EDIT_STOCK);
		auto stockData = g_data.GetStockData(std::wstring(m_stock_code));
		if (stockData && !stockData->info.displayName.empty())
			title += _T("(") + CString(stockData->info.displayName.c_str()) + _T(")");
		SetWindowText(title);
	}

	Log1("COptionsDlg::OnInitDialog: m_stock_code=%s\n", m_stock_code.GetString());

	if (!m_stock_code.IsEmpty())
	{
		double low_price = g_data.GetAlertLowPrice(std::wstring(m_stock_code));
		double high_price = g_data.GetAlertHighPrice(std::wstring(m_stock_code));
		Log1("COptionsDlg::OnInitDialog: low_price=%f, high_price=%f\n", low_price, high_price);
		m_alert_low_price = low_price;
		m_alert_high_price = high_price;

		double cost_price = g_data.GetCostPrice(std::wstring(m_stock_code));
		double holding_count = g_data.GetHoldingCount(std::wstring(m_stock_code));
		std::wstring buy_date = g_data.GetBuyDate(std::wstring(m_stock_code));
		Log1("COptionsDlg::OnInitDialog: cost_price=%f, holding_count=%f\n", cost_price, holding_count);
		m_cost_price = cost_price;
		m_holding_count = holding_count;
		m_buy_date = buy_date.c_str();
		m_show_in_statusbar = g_data.GetShowInStatusBar(std::wstring(m_stock_code)) ? TRUE : FALSE;

		UpdateData(FALSE);
	}

	CString display_code = m_stock_code;
	RemoveTypeFromCode(display_code);
	SetDlgItemText(IDC_CODE_EDIT, display_code);

	m_market_type_combo.AddString(L"上证");
	m_market_type_combo.AddString(L"深证");
	m_market_type_combo.AddString(L"北交所");
	m_market_type_combo.AddString(L"港股");
	m_market_type_combo.AddString(L"美股");

	if (!m_stock_code.IsEmpty())
	{
		CString type = GetCodeType(m_stock_code);
		CString display_type = TypeToDisplayName(type);
		int index = m_market_type_combo.FindStringExact(-1, display_type);
		if (index != CB_ERR)
			m_market_type_combo.SetCurSel(index);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void COptionsDlg::OnChangeCodeEdit()
{
	CString code;
	GetDlgItemText(IDC_CODE_EDIT, code);
	CString type = GetMarketTypeByCode(code);
	CString display_type = TypeToDisplayName(type);
	if (!display_type.IsEmpty())
	{
		int index = m_market_type_combo.FindStringExact(-1, display_type);
		if (index != CB_ERR)
			m_market_type_combo.SetCurSel(index);
	}
}

CString COptionsDlg::TypeToDisplayName(const CString& type)
{
	if (type == kSH)
		return L"上证";
	else if (type == kSZ)
		return L"深证";
	else if (type == kBJ)
		return L"北交所";
	else if (type == kHK)
		return L"港股";
	else if (type == kMG)
		return L"美股";
	return L"";
}

CString COptionsDlg::DisplayNameToType(const CString& displayName)
{
	if (displayName == L"上证")
		return kSH;
	else if (displayName == L"深证")
		return kSZ;
	else if (displayName == L"北交所")
		return kBJ;
	else if (displayName == L"港股")
		return kHK;
	else if (displayName == L"美股")
		return kMG;
	return CString();
}

void COptionsDlg::RemoveTypeFromCode(CString& code)
{
	for (const auto& type : StockTypeSet)
	{
		if (code.Left(type.GetLength()) == type)
		{
			code = code.Right(code.GetLength() - type.GetLength());
			return;
		}
	}
}

CString COptionsDlg::GetCodeType(const CString& code)
{
	for (const auto& type : StockTypeSet)
	{
		if (code.Left(type.GetLength()) == type)
		{
			return type;
		}
	}
	return CString();
}

CString COptionsDlg::GetMarketTypeByCode(const CString& code)
{
	CString clean_code = code;
	RemoveTypeFromCode(clean_code);

	if (clean_code.GetLength() < 6)
	{
		return CString();
	}

	CString prefix = clean_code.Left(3);
	CString prefix2 = clean_code.Left(2);

	if (prefix == L"600" || prefix == L"601" || prefix == L"603" || prefix == L"605")
	{
		return kSH;
	}
	else if (prefix == L"688")
	{
		return kSH;
	}
	else if (prefix2 == L"51" || prefix2 == L"58")
	{
		return kSH;
	}
	else if (prefix == L"000" || prefix == L"001" || prefix == L"002" || prefix == L"003" || prefix == L"004")
	{
		return kSZ;
	}
	else if (prefix == L"300" || prefix == L"301")
	{
		return kSZ;
	}
	else if (prefix == L"159")
	{
		return kSZ;
	}
	else if (prefix == L"399")
	{
		return kSZ;
	}
	else if (clean_code.GetLength() >= 6)
	{
		CString first_char = clean_code.Left(1);
		if (first_char == L"8")
		{
			return kBJ;
		}
		else if (prefix2 == L"43" || prefix2 == L"83" || prefix2 == L"87")
		{
			return kBJ;
		}
	}

	return CString();
}

void COptionsDlg::OnBnClickedOk()
{
	CString code;
	GetDlgItemText(IDC_CODE_EDIT, code);
	if (code.IsEmpty())
	{
		CDialog::OnCancel();
		return;
	}

	CString type;
	int sel_index = m_market_type_combo.GetCurSel();
	if (sel_index != CB_ERR)
	{
		CString display_type;
		m_market_type_combo.GetLBText(sel_index, display_type);
		type = DisplayNameToType(display_type);
	}

	if (type.IsEmpty())
	{
		type = GetMarketTypeByCode(code);
		if (type.IsEmpty())
		{
			if (m_stock_code.IsEmpty())
			{
				MessageBox(L"无法识别股票代码，请输入有效的A股股票代码", g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONWARNING | MB_OK);
				return;
			}
			else
			{
				type = GetCodeType(m_stock_code);
			}
		}
	}

	RemoveTypeFromCode(code);
	m_stock_code = type + code;

	// 获取并校验关注价格
	UpdateData(TRUE);
	if (m_alert_low_price < 0)
	{
		MessageBox(g_data.StringRes(IDS_ALERT_LOW_PRICE_INVALID), g_data.StringRes(IDS_PRICE_ALERT_TITLE), MB_ICONWARNING | MB_OK);
		return;
	}
	if (m_alert_high_price < 0)
	{
		MessageBox(g_data.StringRes(IDS_ALERT_HIGH_PRICE_INVALID), g_data.StringRes(IDS_PRICE_ALERT_TITLE), MB_ICONWARNING | MB_OK);
		return;
	}
	if (m_alert_low_price > 0 && m_alert_high_price > 0 && m_alert_low_price >= m_alert_high_price)
	{
		MessageBox(g_data.StringRes(IDS_ALERT_PRICE_RANGE_INVALID), g_data.StringRes(IDS_PRICE_ALERT_TITLE), MB_ICONWARNING | MB_OK);
		return;
	}

	if (m_cost_price < 0)
	{
		MessageBox(g_data.StringRes(IDS_COST_PRICE_INVALID), g_data.StringRes(IDS_PRICE_ALERT_TITLE), MB_ICONWARNING | MB_OK);
		return;
	}

	if (m_holding_count < 0)
	{
		MessageBox(g_data.StringRes(IDS_HOLDING_COUNT_INVALID), g_data.StringRes(IDS_PRICE_ALERT_TITLE), MB_ICONWARNING | MB_OK);
		return;
	}

	// 保存关注价格
	g_data.SetAlertPrice(std::wstring(m_stock_code), m_alert_low_price, m_alert_high_price);

	// 成本为0时清除持股，持股为0时清除成本
	if (m_cost_price <= 0) m_holding_count = 0;
	if (m_holding_count <= 0) m_cost_price = 0;
	if (m_cost_price <= 0 && m_holding_count <= 0) m_buy_date.Empty();

	// 保存持仓配置
	g_data.SetPosition(std::wstring(m_stock_code), m_cost_price, m_holding_count, std::wstring(m_buy_date));

	// 保存状态栏展示配置
	g_data.SetShowInStatusBar(std::wstring(m_stock_code), m_show_in_statusbar != FALSE);

	g_data.SaveConfig();

	CDialog::OnOK();
}

void COptionsDlg::OnBnClickedCancel()
{
	CDialog::OnCancel();
}

void COptionsDlg::OnBnClickedToday()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t dateBuf[20];
	swprintf_s(dateBuf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
	m_buy_date = dateBuf;
	SetDlgItemText(IDC_BUY_DATE_EDIT, m_buy_date);
}