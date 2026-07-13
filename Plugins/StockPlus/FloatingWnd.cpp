#include "pch.h"
#include "FloatingWnd.h"
#include <algorithm>
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>

// K线图已注释，不再需要 GDI+

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

BEGIN_MESSAGE_MAP(CFloatingWnd, CWnd)
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_LBUTTONDOWN()
ON_WM_CREATE()
ON_MESSAGE(FWND_MSG_UPDATE_STATUS, OnUpdateStatus)
ON_MESSAGE(FWND_MSG_REQUEST_DATA, OnRequestData)
END_MESSAGE_MAP()

//// 将新浪代码（sh600000/sz002497/bj430047）映射为东方财富 nid（1.600000/0.002497）
//static std::wstring SinaToEastmoneyNid(const std::wstring &stock_id)
//{
//    if (stock_id.size() < 3)
//        return L"";
//    if (stock_id.compare(0, 2, kSH) == 0)
//        return L"1." + stock_id.substr(2);
//    if (stock_id.compare(0, 2, kSZ) == 0 || stock_id.compare(0, 2, kBJ) == 0)
//        return L"0." + stock_id.substr(2);
//    return L""; // 港股/美股不支持
//}

int CFloatingWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
    return 0;
}

// 处理消息
LRESULT CFloatingWnd::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
    // K线图图片更新已注释
    Invalidate();
    return 0;
}

LRESULT CFloatingWnd::OnRequestData(WPARAM wParam, LPARAM lParam)
{
    time_t req_time = (time_t)wParam;
    if (req_time)
    {
        if (req_time - m_last_request_time > 10)
        {
            m_last_request_time = req_time;
            // 开始网络请求
            RequestData();
        }
        loading_state_txt += L".";
        Invalidate();
    }
    return 0;
}

CFloatingWnd::CFloatingWnd() : m_isDestroying(FALSE)
{
}

CFloatingWnd::~CFloatingWnd()
{
    // 标记窗口正在销毁
    m_isDestroying = TRUE;

    // 只关闭句柄，不等待线程（避免 MFC DllMain 死锁）
    if (m_hNetworkThread != nullptr)
    {
        CloseHandle(m_hNetworkThread);
        m_hNetworkThread = nullptr;
    }

    if (m_CTransparentWnd.GetSafeHwnd())
        m_CTransparentWnd.DestroyWindow();
}

BOOL CFloatingWnd::Create(CFont *font, CPoint pt, std::wstring stock_id)
{
    m_stock_id = stock_id;
    // 注册窗口类
    WNDCLASS wndcls;
    HINSTANCE hInst = AfxGetInstanceHandle();
    if (!(::GetClassInfo(hInst, L"CTransparentWnd", &wndcls)))
    {
        wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wndcls.lpfnWndProc = ::DefWindowProc;
        wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
        wndcls.hInstance = hInst;
        wndcls.hIcon = NULL;
        wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
        // wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wndcls.hbrBackground = NULL; // 重要：设置为NULL
        wndcls.lpszMenuName = NULL;
        wndcls.lpszClassName = L"CTransparentWnd";
        if (!AfxRegisterClass(&wndcls))
            return FALSE;
    }

    // 设置父窗口指针
    m_CTransparentWnd.SetParent(this);

    m_pfont = font;

    // 获取包含鼠标点的显示器
    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};
    GetMonitorInfo(hMonitor, &mi);
    CRect screenRect = mi.rcWork; // 工作区域

    // 创建透明全屏窗口
    if (!m_CTransparentWnd.CreateEx(WS_EX_TOOLWINDOW /* | WS_EX_LAYERED */ /* | WS_EX_TRANSPARENT */,
                                    L"CTransparentWnd", L"", WS_POPUP | WS_VISIBLE,
                                    screenRect, NULL, 0, NULL))
    {
        TRACE(L"Failed to create transparent window\n");
        return FALSE;
    }

    const int WIDTH = g_data.RDPI(g_data.m_setting_data.m_kline_width);
    const int HEIGHT = g_data.RDPI(g_data.m_setting_data.m_kline_height);
    int x = pt.x;
    int y = pt.y;

    // 调整位置
    if (x + WIDTH > screenRect.right)
        x = x - WIDTH;
    if (y + HEIGHT > screenRect.bottom)
        y = y - HEIGHT;
    x = max(screenRect.left, x);
    y = max(screenRect.top, y);

    CRect rect(x, y, x + WIDTH, y + HEIGHT);

    // 创建实际的浮动窗口
    if (!CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                  AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW),
                  L"", WS_POPUP | WS_VISIBLE | WS_BORDER,
                  rect, &m_CTransparentWnd, 0))
    {
        TRACE(L"Failed to create floating window\n");
        m_CTransparentWnd.DestroyWindow();
        return FALSE;
    }

    // 确保浮动窗口在最顶层
    BringWindowToTop();
    SetForegroundWindow();

    // 设置完全透明
    m_CTransparentWnd.SetLayeredWindowAttributes(0, 0, LWA_ALPHA);
    m_CTransparentWnd.ShowWindow(SW_SHOW);

    TRACE(L"Windows created successfully\n");
    return TRUE;
}

void CFloatingWnd::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    // 双缓冲绘制
    CDC memDC;
    CBitmap memBitmap;
    memDC.CreateCompatibleDC(&dc);
    if (m_pfont)
    {
        memDC.SelectObject(m_pfont);
    }
    memBitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
    CBitmap *pOldBitmap = memDC.SelectObject(&memBitmap);

    // 绘制背景
    memDC.FillSolidRect(rect, RGB(255, 255, 255));
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(154, 151, 157));
    // K线图已注释，仅显示加载状态文本
    CString txt = loading_state_txt.IsEmpty() ? CString(g_data.StringRes(IDS_LOADING)) : loading_state_txt;
    memDC.DrawText(txt, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // 复制到屏幕
    dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(pOldBitmap);
}

BOOL CFloatingWnd::OnEraseBkgnd(CDC *pDC)
{
    return TRUE; // 不擦除背景
}

void CFloatingWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    // DestroyWindow();
}

// CreateThread 兼容包装
static DWORD WINAPI NetworkThreadProcWrapper(LPVOID pParam)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return CFloatingWnd::NetworkThreadProc(pParam);
}

void CFloatingWnd::RequestData()
{
    if (!m_is_thread_running)
    {
        loading_state_txt = g_data.StringRes(IDS_LOADING).GetString();

        // 清理上一个已退出的线程句柄
        if (m_hNetworkThread != nullptr)
        {
            CloseHandle(m_hNetworkThread);
            m_hNetworkThread = nullptr;
        }

        // 用 CreateThread 代替 AfxBeginThread
        DWORD tid;
        HANDLE hThread = CreateThread(nullptr, 0, NetworkThreadProcWrapper, this, 0, &tid);
        if (hThread)
        {
            m_hNetworkThread = hThread;
        }
    }
}

UINT CFloatingWnd::NetworkThreadProc(LPVOID pParam)
{
    CFloatingWnd *pFW = (CFloatingWnd *)pParam;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(pFW->m_is_thread_running);

    if (pFW->m_isDestroying || pFW->m_stock_id.empty())
        return 0;

    // K线图已注释，不再下载图片
    // HWND hWnd = pFW->GetSafeHwnd();
    // std::wstring nid = SinaToEastmoneyNid(pFW->m_stock_id);
    // if (nid.empty() || hWnd == NULL)
    //     return 0;
    // std::wstring url = L"https://webquotepic.eastmoney.com/GetPic.aspx?nid=" + nid + L"&imageType=RJY";
    // auto *bytes = new std::vector<BYTE>();
    // if (CCommon::GetURLBinary(url, *bytes, WEB_USERAGENT) && !bytes->empty())
    // {
    //     if (!::PostMessage(hWnd, FWND_MSG_UPDATE_STATUS, 0, reinterpret_cast<LPARAM>(bytes)))
    //         delete bytes;
    // }
    // else
    // {
    //     delete bytes;
    //     CCommon::WriteLog(L"trend image download failed", g_data.m_log_path.c_str());
    // }

    return 0;
}
