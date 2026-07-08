#pragma once
#include "DataManager.h"

// CRelateStockDlg 对话框

class CRelateStockDlg : public CDialog
{
	DECLARE_DYNAMIC(CRelateStockDlg)

public:
	CRelateStockDlg(const std::wstring& current_code, CWnd* pParent = nullptr);
	virtual ~CRelateStockDlg();

	std::vector<std::wstring> m_selected_codes;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RELATED_STOCK_DIALOG };
#endif

private:
	std::wstring m_current_code;
	CCheckListBox m_check_list;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();

	DECLARE_MESSAGE_MAP()
};
