#pragma once
#include "afxdialogex.h"
#include <string>

class CStockDetailDlg : public CDialog
{
    DECLARE_DYNAMIC(CStockDetailDlg)
public:
    CStockDetailDlg(const std::wstring &stock_id, CWnd *pParent = nullptr);
    virtual ~CStockDetailDlg();

    enum
    {
        IDD = IDD_STOCK_DETAIL_DIALOG
    };

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedRefresh();
    DECLARE_MESSAGE_MAP()

private:
    CListCtrl m_list;
    std::wstring m_stock_id;
    void RefreshList();

    static const UINT_PTR TIMER_REFRESH = 1003;
};
