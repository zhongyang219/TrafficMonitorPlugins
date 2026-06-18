﻿// ManagerDialog.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "afxdialogex.h"
#include "ManagerDialog.h"
#include "Common.h"
#include "OptionsDlg.h"
#include <Windows.h>

// CManagerDialog 对话框

IMPLEMENT_DYNAMIC(CManagerDialog, CDialog)

CManagerDialog::CManagerDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_MANAGER_DIALOG, pParent)
{
}

CManagerDialog::~CManagerDialog()
{
}

void CManagerDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MGR_LIST, m_stock_listctrl);
}

BEGIN_MESSAGE_MAP(CManagerDialog, CDialog)
	ON_NOTIFY(NM_CLICK, IDC_MGR_LIST, &CManagerDialog::OnListItemClick)
	ON_BN_CLICKED(IDC_MGR_DEL_BTN, &CManagerDialog::OnDelBtnClick)
	ON_BN_CLICKED(IDC_MGR_ADD_BTN, &CManagerDialog::OnAddBtnClick)
	ON_BN_CLICKED(IDC_MGR_MOVE_UP_BTN, &CManagerDialog::OnMoveUpBtnClick)
	ON_BN_CLICKED(IDC_MGR_MOVE_DOWN_BTN, &CManagerDialog::OnMoveDownBtnClick)
	ON_BN_CLICKED(IDC_FULL_DAY_CHECK, &CManagerDialog::OnClickedFullDayCheck)
	ON_BN_CLICKED(IDOK, &CManagerDialog::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CManagerDialog::OnBnClickedCancel)
	ON_NOTIFY(NM_DBLCLK, IDC_MGR_LIST, &CManagerDialog::OnLbnDblclkMgrList)
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_SHOW_STOCK_NAME_CHECK, &CManagerDialog::OnBnClickedShowStockNameCheck)
	ON_BN_CLICKED(IDC_COLOR_WITH_PRICE_CHECK, &CManagerDialog::OnBnClickedColorWithPriceCheck)
	ON_BN_CLICKED(IDC_SHOW_FLUCTUATION_CHECK, &CManagerDialog::OnBnClickedShowFluctuationCheck)
END_MESSAGE_MAP()

// CManagerDialog 消息处理程序

BOOL CManagerDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	HICON hIcon = g_data.GetIcon(IDI_STOCK);
	SetIcon(hIcon, FALSE);

	// 获取初始时窗口的大小
	CRect rect;
	GetWindowRect(rect);
	m_min_size.cx = rect.Width();
	m_min_size.cy = rect.Height();

	// 设置列表控件为报表风格
	DWORD dwStyle = m_stock_listctrl.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
	m_stock_listctrl.SetExtendedStyle(dwStyle);

	// 添加两列：代码和名称
	m_stock_listctrl.InsertColumn(0, L"代码", LVCFMT_LEFT, g_data.DPI(70));
	m_stock_listctrl.InsertColumn(1, L"名称", LVCFMT_LEFT, g_data.DPI(70));

	// 从 m_data 加载所有股票代码到列表
	for (int i = 0; i < m_data.m_stock_codes.size(); i++)
	{
		const auto& stock_code = m_data.m_stock_codes[i];
		int nItem = m_stock_listctrl.InsertItem(i, stock_code.c_str());

		// 获取并显示股票名称
		std::wstring stock_name = GetStockName(stock_code);
		m_stock_listctrl.SetItemText(nItem, 1, stock_name.c_str());
	}

	CheckDlgButton(IDC_FULL_DAY_CHECK, m_data.m_full_day);
	CheckDlgButton(IDC_SHOW_STOCK_NAME_CHECK, m_data.m_show_stock_name);
	CheckDlgButton(IDC_COLOR_WITH_PRICE_CHECK, m_data.m_color_with_price);
	CheckDlgButton(IDC_SHOW_FLUCTUATION_CHECK, m_data.m_show_fluctuation);

	CString value;
	value.Format(_T("%d"), static_cast<int>(g_data.m_setting_data.m_kline_width));
	SetDlgItemText(IDC_KLINE_WIDTH_EDIT, value);
	value.Format(_T("%d"), static_cast<int>(g_data.m_setting_data.m_kline_height));
	SetDlgItemText(IDC_KLINE_HEIGHT_EDIT, value);

	return TRUE; // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

std::wstring CManagerDialog::GetStockName(const std::wstring& code)
{
	// 从 DataManager 获取股票名称
	auto stockData = g_data.GetStockData(code);
	if (stockData && !stockData->info.displayName.empty())
	{
		return stockData->info.displayName;
	}
	return L"";
}

void CManagerDialog::OnListItemClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// CListCtrl 的点击事件，这里可以处理选中逻辑
	int curSelPos = m_stock_listctrl.GetNextItem(-1, LVNI_SELECTED);
	if (curSelPos >= 0)
	{
		CString code = m_stock_listctrl.GetItemText(curSelPos, 0);
		Log1("OnListItemClick: %s\n", code.GetString());
	}
	*pResult = 0;
}

void CManagerDialog::OnDelBtnClick()
{
	int curSelPos = m_stock_listctrl.GetNextItem(-1, LVNI_SELECTED);
	Log1("OnDelBtnClick: %d\n", curSelPos);
	if (curSelPos < 0 || curSelPos >= m_data.m_stock_codes.size())
	{
		return;
	}
	m_stock_listctrl.DeleteItem(curSelPos);
	m_data.m_stock_codes.erase(m_data.m_stock_codes.begin() + curSelPos);
}

void CManagerDialog::OnAddBtnClick()
{
	if (m_data.m_stock_codes.size() >= Stock_ITEM_MAX)
	{
		MessageBox(g_data.StringRes(IDS_STOCK_NUM_LIMIT_WARNING), g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONWARNING | MB_OK);
		return;
	}
	COptionsDlg dlg(std::wstring(), this);
	auto rtn = dlg.DoModal();
	if (rtn == IDOK)
	{
		std::wstring stock_code = dlg.m_stock_code.GetString();
		if (!stock_code.empty())
		{
			if (count(m_data.m_stock_codes.begin(), m_data.m_stock_codes.end(), stock_code))
			{
				Log1("OnAddBtnClick: ignore %s\n", stock_code.c_str());
				return;
			}
			Log1("OnAddBtnClick: %s\n", stock_code.c_str());
			m_data.m_stock_codes.push_back(stock_code.c_str());

			// 添加到列表并显示名称
			int nItem = m_stock_listctrl.InsertItem(m_stock_listctrl.GetItemCount(), stock_code.c_str());
			std::wstring stock_name = GetStockName(stock_code);
			m_stock_listctrl.SetItemText(nItem, 1, stock_name.c_str());
		}
	}
}

void CManagerDialog::OnMoveUpBtnClick()
{
	int curSelPos = m_stock_listctrl.GetNextItem(-1, LVNI_SELECTED);
	if (curSelPos <= 0 || curSelPos >= m_data.m_stock_codes.size())
	{
		return;
	}

	// 交换列表控件中的项
	CString code = m_stock_listctrl.GetItemText(curSelPos, 0);
	CString name = m_stock_listctrl.GetItemText(curSelPos, 1);
	m_stock_listctrl.DeleteItem(curSelPos);
	m_stock_listctrl.InsertItem(curSelPos - 1, code);
	m_stock_listctrl.SetItemText(curSelPos - 1, 1, name);
	m_stock_listctrl.SetItemState(curSelPos - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	// 交换数据
	std::swap(m_data.m_stock_codes[curSelPos - 1], m_data.m_stock_codes[curSelPos]);
}

void CManagerDialog::OnMoveDownBtnClick()
{
	int curSelPos = m_stock_listctrl.GetNextItem(-1, LVNI_SELECTED);
	if (curSelPos < 0 || curSelPos >= m_data.m_stock_codes.size() - 1)
	{
		return;
	}

	// 交换列表控件中的项
	CString code = m_stock_listctrl.GetItemText(curSelPos, 0);
	CString name = m_stock_listctrl.GetItemText(curSelPos, 1);
	m_stock_listctrl.DeleteItem(curSelPos);
	m_stock_listctrl.InsertItem(curSelPos + 1, code);
	m_stock_listctrl.SetItemText(curSelPos + 1, 1, name);
	m_stock_listctrl.SetItemState(curSelPos + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	// 交换数据
	std::swap(m_data.m_stock_codes[curSelPos], m_data.m_stock_codes[curSelPos + 1]);
}

void CManagerDialog::OnClickedFullDayCheck()
{
	m_data.m_full_day = (IsDlgButtonChecked(IDC_FULL_DAY_CHECK) != 0);
}

void CManagerDialog::OnBnClickedShowStockNameCheck()
{
	m_data.m_show_stock_name = (IsDlgButtonChecked(IDC_SHOW_STOCK_NAME_CHECK) != 0);
}

void CManagerDialog::OnBnClickedColorWithPriceCheck()
{
	m_data.m_color_with_price = (IsDlgButtonChecked(IDC_COLOR_WITH_PRICE_CHECK) != 0);
}

void CManagerDialog::OnBnClickedShowFluctuationCheck()
{
	m_data.m_show_fluctuation = (IsDlgButtonChecked(IDC_SHOW_FLUCTUATION_CHECK) != 0);
}

void CManagerDialog::OnBnClickedOk()
{
	bool stock_code_changed{ g_data.m_setting_data.m_stock_codes != m_data.m_stock_codes };
	CString value;
	GetDlgItemText(IDC_KLINE_WIDTH_EDIT, value);
	m_data.m_kline_width = _ttoi(value);
	GetDlgItemText(IDC_KLINE_HEIGHT_EDIT, value);
	m_data.m_kline_height = _ttoi(value);

	g_data.m_setting_data = m_data;
	g_data.SaveConfig();
	if (stock_code_changed)
	{
		// 找出新增的股票代码（在 m_data 中但不在旧配置中）
		std::set<std::wstring> old_codes(g_data.m_setting_data.m_stock_codes.begin(), g_data.m_setting_data.m_stock_codes.end());
		std::vector<std::wstring> new_codes;
		for (const auto& code : m_data.m_stock_codes)
		{
			if (old_codes.find(code) == old_codes.end())
				new_codes.push_back(code);
		}

		Stock::Instance().SendStockInfoRequest();

		// 为新增的股票异步获取日K线数据
		if (!new_codes.empty())
		{
			std::vector<std::wstring>* pNewCodes = new std::vector<std::wstring>(new_codes);
			AfxBeginThread([](LPVOID pParam) -> UINT {
				std::vector<std::wstring>* pCodes = (std::vector<std::wstring>*)pParam;
				for (const auto& code : *pCodes)
				{
					g_data.RequestKLineData(code, 750);
				}
				delete pCodes;
				return 0;
				}, pNewCodes);
		}
	}
	CDialog::OnOK();
}

void CManagerDialog::OnBnClickedCancel()
{
	CDialog::OnCancel();
}

void CManagerDialog::OnLbnDblclkMgrList(NMHDR* pNMHDR, LRESULT* pResult)
{
	int index = m_stock_listctrl.GetNextItem(-1, LVNI_SELECTED);
	Log1("OnLbnDblclkMgrList: index=%d, stock_codes.size=%d\n", index, m_data.m_stock_codes.size());
	if (index >= 0 && index < m_data.m_stock_codes.size())
	{
		CString code = m_data.m_stock_codes[index].c_str();
		Log1("OnLbnDblclkMgrList: stock_code=%s\n", code.GetString());
		COptionsDlg dlg(m_data.m_stock_codes[index], this);
		auto rtn = dlg.DoModal();
		if (rtn == IDOK)
		{
			if (!dlg.m_stock_code.IsEmpty())
			{
				m_data.m_stock_codes[index] = dlg.m_stock_code;
				m_stock_listctrl.SetItemText(index, 0, dlg.m_stock_code);

				// 更新股票名称
				std::wstring stock_name = GetStockName(dlg.m_stock_code.GetString());
				m_stock_listctrl.SetItemText(index, 1, stock_name.c_str());
			}
		}
	}
	*pResult = 0;
}

void CManagerDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	// 限制窗口最小大小
	lpMMI->ptMinTrackSize.x = m_min_size.cx; // 设置最小宽度
	lpMMI->ptMinTrackSize.y = m_min_size.cy; // 设置最小高度

	CDialog::OnGetMinMaxInfo(lpMMI);
}