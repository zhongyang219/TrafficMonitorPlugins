#pragma once
#include "afxdialogex.h"
#include "DataManager.h"


// CManagerDialog 对话框

class CManagerDialog : public CDialog
{
	DECLARE_DYNAMIC(CManagerDialog)

public:
	CManagerDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CManagerDialog();

	SettingData m_data;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MANAGER_DIALOG };
#endif

private:
	CSize m_min_size;		//窗口的最小大小

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CListBox m_stock_listbox;
	afx_msg void OnListItemClick();
	afx_msg void OnDelBtnClick();
	afx_msg void OnAddBtnClick();
	afx_msg void OnClickedFullDayCheck();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
    afx_msg void OnLbnDblclkMgrList();
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};
