#pragma once
#include <afxwin.h>

class CTransparentWnd : public CWnd
{
public:
	CTransparentWnd();
	void SetParent(CWnd* pParent) { m_pParent = pParent; }

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

private:
	CWnd* m_pParent;
};
