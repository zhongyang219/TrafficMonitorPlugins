#pragma once

#include <StockDef.h>
#include <TransparentWnd.h>

// 定义自定义消息
#define FWND_MSG_UPDATE_STATUS (WM_USER + 100)
#define FWND_MSG_REQUEST_DATA (WM_USER + 101)

class CFloatingWnd : public CWnd
{
public:
    CFloatingWnd();
    virtual ~CFloatingWnd();

    BOOL Create(CFont *font, CPoint pt, std::wstring stock_id);
    void RequestData();

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC *pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    LRESULT OnUpdateStatus(WPARAM wParam, LPARAM lParam);
    LRESULT OnRequestData(WPARAM wParam, LPARAM lParam);

private:
    static UINT NetworkThreadProc(LPVOID pParam); // 线程函数
    CPoint Stock2Point(int x, int y, int w, int h, float unitY, const STOCK::TimelinePoint &item, const STOCK::Price prevClosePrice);

    CTransparentWnd m_CTransparentWnd;
    std::wstring m_stock_id;
    bool m_is_thread_running{};
    volatile BOOL m_isDestroying; // 添加销毁标志
    CFont *m_pfont{};

    unsigned __int64 m_last_request_time{};
};
