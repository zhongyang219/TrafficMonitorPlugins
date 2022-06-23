#pragma once
#include <vector>
#include "afxdialogex.h"

// CChapterDlg 对话框

class CChapterDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CChapterDlg)

public:
    CChapterDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CChapterDlg();
    int GetSelectedPosition() const;        //返回选中章节的位置

private:
    CListBox m_lst_box;
    std::vector<int> m_chapter_index;       //储存所有书签的索引
    int m_selected_position{ -1 };
    bool m_selected_changed{ false };

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_CHAPTER_DIALOG };
#endif

protected:
    void ShowData();

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    //afx_msg void OnBnClickedReParseButton();
    afx_msg void OnLbnSelchangeList1();
};
