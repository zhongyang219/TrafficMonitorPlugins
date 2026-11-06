#pragma once
#include "afxdialogex.h"
#include "DataManager.h"

// CRelateStockDlg 对话框
// 用于选择关联股票，并为每个勾选的股票设置占比（0~1，用户手动输入）

class CRelateStockDlg : public CDialog
{
	DECLARE_DYNAMIC(CRelateStockDlg)

public:
	CRelateStockDlg(const std::wstring& current_code, CWnd* pParent = nullptr);
	virtual ~CRelateStockDlg();

	std::vector<RelatedStockInfo> m_selected;   // 勾选的关联股票（含占比）

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RELATED_STOCK_DIALOG };
#endif

private:
	std::wstring m_current_code;
	CListCtrl m_list;                              // 股票列表控件（报表风格，带复选框）
	CEdit m_ratio_edit;                            // 占比输入框
	std::map<std::wstring, double> m_ratio_values; // 每只股票已设置的占比（未均分时也记录）

	std::wstring GetCodeOfRow(int row) const;      // 获取列表指定行对应的股票代码
	void SetCellRatioText(int row, double ratio);  // 更新列表指定行的占比文本
	void LoadRatioToEdit(int row);                 // 将指定行的占比载入输入框

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedApplyRatio();

	DECLARE_MESSAGE_MAP()
};