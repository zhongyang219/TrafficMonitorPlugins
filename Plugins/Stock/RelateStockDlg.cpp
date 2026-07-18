// RelateStockDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "RelateStockDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"

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
	DDX_Control(pDX, IDC_RELATED_STOCK_LIST, m_check_list);
}

BEGIN_MESSAGE_MAP(CRelateStockDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CRelateStockDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CRelateStockDlg::OnBnClickedCancel)
END_MESSAGE_MAP()

BOOL CRelateStockDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 获取当前已关联的股票列表
	std::vector<std::wstring> existing_related = g_data.GetRelatedStocks(m_current_code);

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

		int index = m_check_list.AddString(display_text);

		// 如果该股票已在关联列表中，则勾选
		bool is_related = (std::find(existing_related.begin(), existing_related.end(), code) != existing_related.end());
		m_check_list.SetCheck(index, is_related ? 1 : 0);

		// 保存股票代码到item data，便于后续获取
		m_check_list.SetItemDataPtr(index, new std::wstring(code));
	}

	return TRUE;
}

void CRelateStockDlg::OnBnClickedOk()
{
	m_selected_codes.clear();

	int count = m_check_list.GetCount();
	for (int i = 0; i < count; i++)
	{
		if (m_check_list.GetCheck(i) == 1)
		{
			std::wstring* pCode = static_cast<std::wstring*>(m_check_list.GetItemDataPtr(i));
			if (pCode)
				m_selected_codes.push_back(*pCode);
		}
	}

	// 清理item data中动态分配的wstring
	for (int i = 0; i < count; i++)
	{
		std::wstring* pCode = static_cast<std::wstring*>(m_check_list.GetItemDataPtr(i));
		if (pCode)
			delete pCode;
	}

	CDialog::OnOK();
}

void CRelateStockDlg::OnBnClickedCancel()
{
	// 清理item data中动态分配的wstring
	int count = m_check_list.GetCount();
	for (int i = 0; i < count; i++)
	{
		std::wstring* pCode = static_cast<std::wstring*>(m_check_list.GetItemDataPtr(i));
		if (pCode)
			delete pCode;
	}

	CDialog::OnCancel();
}
