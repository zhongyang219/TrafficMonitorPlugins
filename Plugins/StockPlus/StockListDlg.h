#pragma once
#include "afxdialogex.h"
#include <vector>

class CStockListDlg : public CDialog
{
    DECLARE_DYNAMIC(CStockListDlg)
public:
    CStockListDlg(CWnd *pParent = nullptr);
    virtual ~CStockListDlg();

    enum
    {
        IDD = IDD_STOCK_LIST_DIALOG
    };

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult);
    DECLARE_MESSAGE_MAP()

private:
    CListCtrl m_list;
    void RefreshList();

    // 每行是否下跌（用于红绿着色）
    std::vector<bool> m_is_down;

    static const UINT_PTR TIMER_REFRESH = 1002;
};
