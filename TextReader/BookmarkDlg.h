#pragma once
#include "afxdialogex.h"


// CBookmarkDlg 对话框

class CBookmarkDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CBookmarkDlg)

public:
	CBookmarkDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CBookmarkDlg();

private:
    CListCtrl m_list_ctrl;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_BOOKMARK_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
};
