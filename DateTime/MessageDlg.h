#pragma once


// CMessageDlg 对话框

class CMessageDlg : public CDialog
{
    DECLARE_DYNAMIC(CMessageDlg)

public:
    CMessageDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CMessageDlg();

    void SetTitle(const CString& title);
    void SetText(const CString& text);

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MESSAGE_DIALOG };
#endif

private:
    CString m_text;
    CString m_title;
    CSize m_min_size;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};
