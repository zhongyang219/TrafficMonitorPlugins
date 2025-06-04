#pragma once
#include "afxdialogex.h"


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

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CListCtrl m_list_ctrl;
};
