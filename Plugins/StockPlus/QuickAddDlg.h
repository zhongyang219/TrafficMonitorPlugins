#pragma once
#include "afxdialogex.h"
#include <vector>
#include <string>

// 快速添加股票搜索结果
struct StockSearchResult
{
    std::wstring prefix; // sh/sz/bj
    std::wstring code;   // 600000
    std::wstring name;   // 浦发银行
    std::wstring StockId() const { return prefix + code; }
};

class CQuickAddDlg : public CDialog
{
    DECLARE_DYNAMIC(CQuickAddDlg)
public:
    CQuickAddDlg(CWnd *pParent = nullptr);
    virtual ~CQuickAddDlg();

    // 用户选中的股票ID（如 sh600000）；空表示未选中
    std::wstring m_selected;

    enum
    {
        IDD = IDD_QUICK_ADD_DIALOG
    };

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnChangeSearch();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnLbnDblclkList();
    afx_msg void OnBnClickedOk();
    LRESULT OnResults(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

public:
    static UINT SearchThreadProc(LPVOID pParam); // public 供 CreateThread 包装调用

    CEdit m_search_edit;
    CListBox m_result_list;

    bool m_searching{false};
    // m_results 仅在 UI 线程访问（OnResults 写、OnBnClickedOk 读）
    std::vector<StockSearchResult> m_results;

    static const UINT_PTR TIMER_SEARCH = 1001;
    static const UINT WM_QA_RESULTS = WM_USER + 200;
};
