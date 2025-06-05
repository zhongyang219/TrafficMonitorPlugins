#pragma once
#include "afxdialogex.h"


class CHistoryWeatherListCtrl :
	public CListCtrl
{
	DECLARE_DYNAMIC(CHistoryWeatherListCtrl)
public:
	CHistoryWeatherListCtrl();
	~CHistoryWeatherListCtrl();

	enum Column
	{
		COL_DATE,
		COL_WEATHER
	};

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
};


// CWeatherHistoryDlg 对话框

class CWeatherHistoryDlg : public CDialog
{
	DECLARE_DYNAMIC(CWeatherHistoryDlg)

public:
	CWeatherHistoryDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CWeatherHistoryDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_WEATHER_HISTORY_DIALOG };
#endif

private:
	CHistoryWeatherListCtrl m_list_ctrl;
	CSize m_min_size;		//窗口的最小大小

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};
