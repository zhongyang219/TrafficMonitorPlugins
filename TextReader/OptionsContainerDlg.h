#pragma once
#include "TabCtrlEx.h"
#include "OptionsDlg.h"
#include "ChapterDlg.h"
#include "BookmarkDlg.h"

// COptionsContainerDlg 对话框

class COptionsContainerDlg : public CDialog
{
    DECLARE_DYNAMIC(COptionsContainerDlg)

public:
    COptionsContainerDlg(int tab_selected = 0, CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~COptionsContainerDlg();

public:
    COptionsDlg m_options_dlg;
    CChapterDlg m_chapter_dlg;
    CBookmarkDlg m_bookmark_dlg;

protected:
    CTabCtrlEx m_tab_ctrl;
    int m_tab_selected{};

private:
    CSize m_min_size;

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_CONTAINER_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual void OnOK();
    virtual BOOL OnInitDialog();
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnNMClickHelpSyslink1(NMHDR* pNMHDR, LRESULT* pResult);
};
