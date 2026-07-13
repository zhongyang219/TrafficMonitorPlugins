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

    // 添加调试输出
    TRACE(L"CTransparentWnd Created\n");
    return 0;
}

CTransparentWnd::CTransparentWnd() : m_pParent(nullptr)
{
}

void CTransparentWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    // 添加调试输出
    TRACE(L"CTransparentWnd OnLButtonDown\n");

    // 获取鼠标位置
    CPoint ptScreen;
    GetCursorPos(&ptScreen);

    // 获取浮动窗口区域
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
            // 如果点击在浮动窗口内部，将消息传递给浮动窗口
            m_pParent->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
        }
    }
    // else
    // {
    //     TRACE(L"Destroying transparent window\n");
    //     DestroyWindow();
    // }
}

BOOL CTransparentWnd::OnEraseBkgnd(CDC *pDC)
{
    return TRUE; // 不擦除背景
}
