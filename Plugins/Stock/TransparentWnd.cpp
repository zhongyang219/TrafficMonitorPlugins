#include "pch.h"
#include "TransparentWnd.h"
#include "FloatingWnd.h"
#include <Stock.h>

BEGIN_MESSAGE_MAP(CTransparentWnd, CWnd)
ON_WM_LBUTTONDOWN()
ON_WM_ERASEBKGND()
ON_WM_CREATE()
END_MESSAGE_MAP()

int CTransparentWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    // Add debug output
    TRACE(L"CTransparentWnd Created\n");
    return 0;
}

CTransparentWnd::CTransparentWnd() : m_pParent(nullptr)
{
}

void CTransparentWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    // Add debug output
    TRACE(L"CTransparentWnd OnLButtonDown\n");

    // Get mouse position
    CPoint ptScreen;
    GetCursorPos(&ptScreen);

    // Get floating window area
    CRect rcFloat;
    if (m_pParent && m_pParent->GetSafeHwnd())
    {
        m_pParent->GetWindowRect(rcFloat);
        if (!rcFloat.PtInRect(ptScreen))
        {
            TRACE(L"Destroying floating window\n");
            DestroyWindow();
            Stock::Instance().DestroyFloatingWnd();
        }
        else
        {
            // If click is inside floating window, pass message to floating window
            m_pParent->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
        }
    }
}

BOOL CTransparentWnd::OnEraseBkgnd(CDC *pDC)
{
    return TRUE; // Do not erase background
}
