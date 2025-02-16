#pragma once
#include "afxdialogex.h"


// CBookmarkDlg 对话框

class CBookmarkDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CBookmarkDlg)

public:
    CBookmarkDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CBookmarkDlg();
    void AdjustColumeWidth();
    int GetSelectedPosition() const;        //返回选中章书签的位置

private:
    CListCtrl m_list_ctrl;
    int m_selected_position{ -1 };

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_BOOKMARK_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
};
