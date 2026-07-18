#include "pch.h"
#include "Stock.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "ManagerDialog.h"
#include "QuickAddDlg.h"
#include "StockListDlg.h"
#include "Common.h"
#include <algorithm>

Stock Stock::m_instance;

Stock::Stock() : m_pFloatingWnd(NULL)
{
    m_items = vector<StockItem>(POSITION_COUNT);
    fill(m_items.begin(), m_items.end(), StockItem());
    for (int index = 0; index < m_items.size(); index++)
    {
        m_items[index].index = index;
    }
}

Stock::~Stock()
{
    DestroyFloatingWnd();
    // 不等待线程退出（MFC DllMain 不会等 CreateThread），只关闭句柄防泄漏
    if (m_hRequestThread != nullptr)
    {
        CloseHandle(m_hRequestThread);
        m_hRequestThread = nullptr;
    }
}

Stock &Stock::Instance()
{
    return m_instance;
}

UINT Stock::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    if (g_data.m_setting_data.m_stock_codes.empty())
    {
        // CCommon::WriteLog(L"Stock_code not setting!", g_data.m_log_path.c_str());
        g_data.ResetText();
        return 0;
    }

    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3)
    {
        m_instance.m_last_request_time = cur_time;

        if (g_data.m_setting_data.m_full_day != 1)
        {
            SYSTEMTIME now_time;
            GetLocalTime(&now_time);
            // CCommon::WriteLog(now_time.wHour, g_data.m_log_path.c_str());
            // CCommon::WriteLog(now_time.wMinute, g_data.m_log_path.c_str());
            if (now_time.wHour < 9 || now_time.wHour > 15 || (now_time.wHour == 15 && now_time.wMinute > 30))
            {
                CCommon::WriteLog(L"Not currently in trading time!", g_data.m_log_path.c_str());
                g_data.ResetText();
                return 0;
            }
        }

        // 禁用选项设置中的“更新”按钮
        m_instance.DisableUpdateCommand();

        g_data.RequestRealtimeData();

        // 数据更新后刷新宽度缓存，避免显示框截断
        for (auto &item : m_instance.m_items)
        {
            item.m_needs_width_recalc = true;
        }

        // 启用选项设置中的”更新”按钮
        m_instance.EnableUpdateCommand();
    }
    return 0;
}

void Stock::LoadContextMenu()
{
    // 每次都重新加载，避免 AppendMenu 重复追加
    if (m_menu.m_hMenu != NULL)
        m_menu.DestroyMenu();
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    m_menu.LoadMenu(IDR_MENU1);
}

IPluginItem *Stock::GetItem(int index)
{
    if (index < 0)
        return nullptr;
    // 只返回已勾选启用的位置
    int count = 0;
    for (int i = 0; i < (int)m_items.size(); i++)
    {
        if (g_data.m_setting_data.m_show_positions[i])
        {
            if (count == index)
                return &m_items[i];
            count++;
        }
    }
    return nullptr;
}

const wchar_t *Stock::GetTooltipInfo()
{
    return L"";
}

void Stock::DataRequired()
{
    static time_t last_req_time{-1};
    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3)
    {
        last_req_time = cur_time;
        SendStockInfoRequest();
    }

    // 翻卡轮播：每调用一次 DataRequired 就尝试推进
    for (auto &item : m_items)
    {
        item.AdvanceIfNeeded();
    }

    std::lock_guard<std::mutex> lock(m_wndMutex);
    if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
    {
        m_pFloatingWnd->SendMessage(FWND_MSG_REQUEST_DATA, cur_time, 0);
    }
}
ITMPlugin::OptionReturn Stock::ShowOptionsDialog(void *hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd *pParent = CWnd::FromHandle((HWND)hParent);
    if (ShowStockManageDlg(pParent) == IDOK)
    {
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t *Stock::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"luox-e";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/CListery/TrafficMonitorPlugins";
    case TMI_VERSION:
        return L"2.0";
    default:
        break;
    }
    return L"";
}

void Stock::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t *data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        // 从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));
        updateItems();
        break;
    case ITMPlugin::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN:
        // 获取TrafficMonitor任务栏窗口中“数值右对齐”设置
        g_data.m_right_align = (_wtoi(data) != 0);
        break;
    default:
        break;
    }
}

int Stock::GetCommandCount()
{
    return 1;
}

const wchar_t *Stock::GetCommandName(int command_index)
{
    switch (command_index)
    {
    case 0:
        return g_data.StringRes(IDS_MENU_UPDATE_STOCK).GetString();
    }
    return nullptr;
}

void Stock::OnPluginCommand(int command_index, void *hWnd, void *para)
{
    switch (command_index)
    {
    case 0:
        SendStockInfoRequest();
        break;
    }
}

void *Stock::GetPluginIcon()
{
    return g_data.GetIcon(IDI_STOCK);
}

void Stock::updateItems()
{
    // 清空所有位置的队列
    for (auto &item : m_items)
    {
        item.m_queue.clear();
        item.enable = FALSE;
        item.m_current_index = 0;
        item.m_flip_counter = 0;
    }

    // 收集已勾选启用的位置索引
    std::vector<int> enabled;
    for (int i = 0; i < (int)m_items.size(); i++)
    {
        if (g_data.m_setting_data.m_show_positions[i])
            enabled.push_back(i);
    }

    // 将股票代码轮询分配到已启用的位置
    // 启用1个位置：所有股票进该位置队列 → 单位置连续轮播
    // 启用2个位置：平分到两个队列 → 两个两个轮播
    auto &codes = g_data.m_setting_data.m_stock_codes;
    if (!enabled.empty())
    {
        for (size_t i = 0; i < codes.size(); i++)
        {
            int card = enabled[i % enabled.size()];
            m_items[card].m_queue.push_back(codes[i]);
            m_items[card].enable = TRUE;
        }
    }

    // 标记宽度缓存失效
    for (auto &item : m_items)
    {
        item.m_needs_width_recalc = true;
        item.m_fixed_width = 0;
    }

    // 未分配到股票的位置给一个空条目避免空白
    for (auto &item : m_items)
    {
        if (item.m_queue.empty())
        {
            item.m_queue.push_back(L"");
        }
    }
}

INT_PTR Stock::ShowStockManageDlg(CWnd *pWnd)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CManagerDialog dlg(pWnd);
    dlg.m_data = g_data.m_setting_data;
    m_option_dlg = &dlg;
    INT_PTR rtn = dlg.DoModal();
    m_option_dlg = nullptr;
    if (rtn == IDOK)
    {
        g_data.m_setting_data = dlg.m_data;
        updateItems();
        g_data.SaveConfig();
    }
    return rtn;
}

INT_PTR Stock::ShowQuickAddDlg(CWnd *pWnd)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CQuickAddDlg dlg(pWnd);
    INT_PTR rtn = dlg.DoModal();
    if (rtn == IDOK && !dlg.m_selected.empty())
    {
        auto &codes = g_data.m_setting_data.m_stock_codes;
        if (codes.size() >= Stock_ITEM_MAX)
        {
            pWnd->MessageBox(g_data.StringRes(IDS_STOCK_NUM_LIMIT_WARNING), g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONWARNING | MB_OK);
            return rtn;
        }
        if (std::find(codes.begin(), codes.end(), dlg.m_selected) != codes.end())
        {
            pWnd->MessageBox(g_data.StringRes(IDS_STOCK_EXISTS), g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONINFORMATION | MB_OK);
            return rtn;
        }
        codes.push_back(dlg.m_selected);
        g_data.SaveConfig();
        updateItems();
        SendStockInfoRequest();

        CString tip = g_data.StringRes(IDS_STOCK_ADDED);
        tip += dlg.m_selected.c_str();
        pWnd->MessageBox(tip, g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONINFORMATION | MB_OK);
    }
    return rtn;
}

INT_PTR Stock::ShowStockListDlg(CWnd *pWnd)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CStockListDlg dlg(pWnd);
    return dlg.DoModal();
}

// CreateThread 兼容包装，避免 MFC 的 AfxBeginThread 在 DLL 卸载时等待导致进程残留
static DWORD WINAPI ThreadCallbackWrapper(LPVOID lpParam)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return Stock::ThreadCallback(lpParam);
}

void Stock::SendStockInfoRequest()
{
    if (!m_is_thread_runing) // 确保线程已退出
    {
        // 清理上一个已退出的线程句柄
        if (m_hRequestThread != nullptr)
        {
            CloseHandle(m_hRequestThread);
            m_hRequestThread = nullptr;
        }

        // 用 CreateThread 代替 AfxBeginThread，避免 MFC 线程追踪导致 DllMain 死锁
        DWORD tid;
        HANDLE hThread = CreateThread(nullptr, 0, ThreadCallbackWrapper, nullptr, 0, &tid);
        if (hThread)
        {
            m_hRequestThread = hThread;
        }
    }
}

void Stock::ShowContextMenu(CWnd *pWnd)
{
    LoadContextMenu();
    CMenu *context_menu = m_menu.GetSubMenu(0);
    if (context_menu != nullptr)
    {
        // 设置“轮播”项和“居右对齐”项的勾选状态
        context_menu->CheckMenuItem(ID_TOGGLE_CAROUSEL, MF_BYCOMMAND | (g_data.m_setting_data.m_carousel_enabled ? MF_CHECKED : MF_UNCHECKED));

        // 动态追加”居右对齐”菜单项
        context_menu->AppendMenu(MF_SEPARATOR);
        context_menu->AppendMenu(MF_STRING | (g_data.m_setting_data.m_align_right ? MF_CHECKED : MF_UNCHECKED),
            ID_TOGGLE_ALIGN_RIGHT, L"居右对齐(&R)");

        CPoint point1;
        GetCursorPos(&point1);
        DWORD id = context_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point1.x, point1.y, pWnd);
        // 点击了“管理”
        if (id == ID_OPTIONS)
        {
            ShowStockManageDlg(pWnd);
        }
        // 点击了“更新”
        else if (id == ID_UPDATE)
        {
            SendStockInfoRequest();
        }
        // 点击了“快速添加”
        else if (id == ID_QUICK_ADD)
        {
            ShowQuickAddDlg(pWnd);
        }
        // 点击了“轮播”开关
        else if (id == ID_TOGGLE_CAROUSEL)
        {
            g_data.m_setting_data.m_carousel_enabled = !g_data.m_setting_data.m_carousel_enabled;
            g_data.SaveConfig();
        }
        // 点击了“居右对齐”开关
        else if (id == ID_TOGGLE_ALIGN_RIGHT)
        {
            g_data.m_setting_data.m_align_right = !g_data.m_setting_data.m_align_right;
            for (auto &item : m_items)
            {
                item.m_needs_width_recalc = true;
                item.m_fixed_width = 0;
            }
            g_data.SaveConfig();
        }
    }
}

void Stock::ShowFloatingWnd(void *hWnd, CPoint ptScreen, std::wstring stock_id)
{
    // 如果已有悬浮窗，先销毁
    DestroyFloatingWnd();

    ClientToScreen((HWND)hWnd, &ptScreen);

    CWnd *pWnd = CWnd::FromHandle((HWND)hWnd);

    CFont *font = pWnd->GetParent()->GetFont();

    std::lock_guard<std::mutex> lock(m_wndMutex);
    // 创建新的悬浮窗
    m_pFloatingWnd = new CFloatingWnd;
    if (!m_pFloatingWnd->Create(font, ptScreen, stock_id))
    {
        delete m_pFloatingWnd;
        m_pFloatingWnd = NULL;
    }
}

void Stock::DestroyFloatingWnd()
{
    std::lock_guard<std::mutex> lock(m_wndMutex);
    if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
    {
        m_pFloatingWnd->DestroyWindow();
        delete m_pFloatingWnd;
        m_pFloatingWnd = NULL;
    }
}

void Stock::UpdateKLine()
{
    std::lock_guard<std::mutex> lock(m_wndMutex);
    if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
    {
        m_pFloatingWnd->SendMessage(FWND_MSG_UPDATE_STATUS, FALSE, 0);
        // DWORD_PTR dwResult = 0;
        // LRESULT lr = ::SendMessageTimeout(
        //     m_pFloatingWnd->GetSafeHwnd(),  // 目标窗口句柄
        //     FWND_MSG_UPDATE_STATUS,          // 消息ID
        //     FALSE,                       // wParam
        //     0,                              // lParam
        //     SMTO_ABORTIFHUNG | SMTO_BLOCK,  // 如果窗口挂起则放弃，并阻塞调用线程
        //     2000,                           // 2秒超时
        //     &dwResult);                     // 接收返回值

        // if (lr == 0) // 失败
        //{
        //     DWORD dwErr = GetLastError();
        //     // 处理错误：记录日志或销毁无效窗口等
        //     if (dwErr == ERROR_TIMEOUT)
        //     {
        //         TRACE("SendMessageTimeout timed out\n");
        //     }
        // }
    }
}

void Stock::DisableUpdateCommand()
{
    // if (m_option_dlg != nullptr)
    //     m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_GRAYED);
}

void Stock::EnableUpdateCommand()
{
    // if (m_instance.m_option_dlg != nullptr)
    //     m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_ENABLED);
}

ITMPlugin *TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &Stock::Instance();
}
