// RelateStockDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "RelateStockDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include <algorithm>

// CRelateStockDlg 对话框

IMPLEMENT_DYNAMIC(CRelateStockDlg, CDialog)

CRelateStockDlg::CRelateStockDlg(const std::wstring& current_code, CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_RELATED_STOCK_DIALOG, pParent)
	, m_current_code(current_code)
{
}

CRelateStockDlg::~CRelateStockDlg()
{
}

void CRelateStockDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RELATED_STOCK_LIST, m_list);
	DDX_Control(pDX, IDC_RELATED_RATIO_EDIT, m_ratio_edit);
}

BEGIN_MESSAGE_MAP(CRelateStockDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CRelateStockDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CRelateStockDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_RELATED_RATIO_BUTTON, &CRelateStockDlg::OnBnClickedApplyRatio)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RELATED_STOCK_LIST, &CRelateStockDlg::OnListItemChanged)
END_MESSAGE_MAP()

BOOL CRelateStockDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 报表风格 + 复选框，整行选中，显示网格线
	m_list.SetExtendedStyle(m_list.GetExtendedStyle()
		| LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
	// 列表项超出时显示垂直滚动条
	m_list.ModifyStyle(0, WS_VSCROLL);

	// 添加两列：股票、占比
	m_list.InsertColumn(0, L"股票", LVCFMT_LEFT, g_data.DPI(180));
	m_list.InsertColumn(1, L"占比(0~1)", LVCFMT_LEFT, g_data.DPI(80));

	// 获取当前已关联的股票（含占比）
	std::vector<RelatedStockInfo> existing_related = g_data.GetRelatedStocks(m_current_code);
	std::map<std::wstring, double> existing_ratio_map;
	for (const auto& ri : existing_related)
	{
		existing_ratio_map[ri.code] = ri.ratio;
		m_ratio_values[ri.code] = ri.ratio;
	}

	// 遍历所有股票，排除当前编辑的股票本身
	for (const auto& code : g_data.m_setting_data.m_stock_codes)
	{
		if (code == m_current_code)
			continue;

		// 获取股票名称
		CString display_text;
		auto stockData = g_data.GetStockData(code);
		if (stockData && !stockData->info.displayName.empty())
			display_text = CString(stockData->info.displayName.c_str()) + _T("(") + CString(code.c_str()) + _T(")");
		else
			display_text = CString(code.c_str());

		int row = m_list.InsertItem(m_list.GetItemCount(), display_text);

		// 保存股票代码到item data，便于后续获取
		m_list.SetItemData(row, reinterpret_cast<DWORD_PTR>(new std::wstring(code)));

		// 已关联的股票勾选并显示历史占比
		auto it = existing_ratio_map.find(code);
		if (it != existing_ratio_map.end())
		{
			m_list.SetCheck(row, TRUE);
			SetCellRatioText(row, it->second);
		}
	}

	return TRUE;
}

BOOL CRelateStockDlg::PreTranslateMessage(MSG* pMsg)
{
	// 在占比输入框中按回车时执行"应用"，而不是触发默认的确定按钮
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		if (GetFocus() == &m_ratio_edit)
		{
			OnBnClickedApplyRatio();
			return TRUE;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

std::wstring CRelateStockDlg::GetCodeOfRow(int row) const
{
	if (row < 0 || row >= m_list.GetItemCount())
		return std::wstring();
	std::wstring* pCode = reinterpret_cast<std::wstring*>(m_list.GetItemData(row));
	if (!pCode)
		return std::wstring();
	return *pCode;
}

void CRelateStockDlg::SetCellRatioText(int row, double ratio)
{
	// 占比取值范围为0~1，显示2位小数
	CString str;
	str.Format(_T("%.4f"), ratio);
	m_list.SetItemText(row, 1, str);
}

void CRelateStockDlg::LoadRatioToEdit(int row)
{
	std::wstring code = GetCodeOfRow(row);
	if (code.empty())
		return;
	auto it = m_ratio_values.find(code);
	if (it == m_ratio_values.end())
		m_ratio_edit.SetWindowText(L"");
	else
	{
		CString str;
		str.Format(_T("%.4f"), it->second);
		m_ratio_edit.SetWindowText(str);
	}
}

void CRelateStockDlg::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	if (pNMLV->iItem < 0 || pNMLV->iItem >= m_list.GetItemCount())
		return;

	// 复选框状态变化：首次勾选时默认按均分分配占比
	UINT oldCheckState = pNMLV->uOldState & LVIS_STATEIMAGEMASK;
	UINT newCheckState = pNMLV->uNewState & LVIS_STATEIMAGEMASK;
	if (oldCheckState != newCheckState
		&& newCheckState == INDEXTOSTATEIMAGEMASK(2))
	{
		std::wstring code = GetCodeOfRow(pNMLV->iItem);
		if (!code.empty() && m_ratio_values.find(code) == m_ratio_values.end())
		{
			int checkedCount = 0;
			for (int i = 0; i < m_list.GetItemCount(); i++)
			{
				if (m_list.GetCheck(i))
					checkedCount++;
			}
			double equal_ratio = checkedCount > 0 ? 1.0 / checkedCount : 0.0;
			m_ratio_values[code] = equal_ratio;
			SetCellRatioText(pNMLV->iItem, equal_ratio);
			LoadRatioToEdit(pNMLV->iItem);
		}
	}

	// 选中状态变化：将选中行的占比载入输入框
	UINT oldSelState = pNMLV->uOldState & LVIS_SELECTED;
	UINT newSelState = pNMLV->uNewState & LVIS_SELECTED;
	if (newSelState && !oldSelState)
		LoadRatioToEdit(pNMLV->iItem);
}

void CRelateStockDlg::OnBnClickedApplyRatio()
{
	int row = m_list.GetSelectionMark();
	if (row < 0)
	{
		AfxMessageBox(_T("请先在列表中选择一个股票"));
		return;
	}

	CString str;
	m_ratio_edit.GetWindowText(str);
	str.Trim();
	if (str.IsEmpty())
	{
		AfxMessageBox(_T("请输入占比数值"));
		return;
	}

	// 兼容输入框中使用逗号作为小数点的情况
	std::wstring ratio_str = str.GetString();
	std::replace(ratio_str.begin(), ratio_str.end(), L',', L'.');

	double ratio = 0.0;
	try
	{
		ratio = std::stod(ratio_str);
	}
	catch (...)
	{
		AfxMessageBox(_T("占比数值无效"));
		return;
	}

	// 占比要求介于0与1之间
	if (ratio < 0.0 || ratio > 1.0)
	{
		AfxMessageBox(_T("占比必须在0到1之间"));
		return;
	}

	std::wstring code = GetCodeOfRow(row);
	if (code.empty())
		return;

	m_ratio_values[code] = ratio;
	SetCellRatioText(row, ratio);
}

void CRelateStockDlg::OnBnClickedOk()
{
	m_selected.clear();

	int count = m_list.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		if (!m_list.GetCheck(i))
			continue;

		std::wstring code = GetCodeOfRow(i);
		if (code.empty())
			continue;

		RelatedStockInfo info;
		info.code = code;
		auto it = m_ratio_values.find(code);
		info.ratio = (it != m_ratio_values.end()) ? it->second : 0.0;
		m_selected.push_back(info);
	}

	// 清理item data中动态分配的wstring
	for (int i = 0; i < count; i++)
	{
		std::wstring* pCode = reinterpret_cast<std::wstring*>(m_list.GetItemData(i));
		if (pCode)
			delete pCode;
	}

	CDialog::OnOK();
}

void CRelateStockDlg::OnBnClickedCancel()
{
	// 清理item data中动态分配的wstring
	int count = m_list.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		std::wstring* pCode = reinterpret_cast<std::wstring*>(m_list.GetItemData(i));
		if (pCode)
			delete pCode;
	}

	CDialog::OnCancel();
}