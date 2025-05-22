#pragma once
#include <afxwin.h>

class CTransparentWnd : public CWnd
{
public:
    CTransparentWnd();
    void SetParent(CWnd *pParent) { m_pParent = pParent; } // 添加设置父窗口的方法

protected:
    DECLARE_MESSAGE_MAP()
    //afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC *pDC);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

private:
    CWnd *m_pParent;
};
