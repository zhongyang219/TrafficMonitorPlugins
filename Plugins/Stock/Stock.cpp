#include "pch.h"
#include "Stock.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "ManagerDialog.h"
#include "Common.h"

Stock Stock::m_instance;

Stock::Stock() : m_pFloatingWnd(NULL)
{
    m_items = vector<StockItem>(Stock_ITEM_MAX);
    fill(m_items.begin(), m_items.end(), StockItem());
    for (int index = 0; index < m_items.size(); index++)
    {
        m_items[index].index = index;
    }
}

Stock::~Stock()
{
    DestroyFloatingWnd();
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

        // 启用选项设置中的“更新”按钮
        m_instance.EnableUpdateCommand();
    }
    return 0;
}

void Stock::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }
}

IPluginItem *Stock::GetItem(int index)
{
    size_t item_size = m_items.size();
    if (g_data.m_setting_data.m_stock_codes.size() < item_size)
        item_size = g_data.m_setting_data.m_stock_codes.size();
    if (item_size == 0)
        item_size = 1;
    if (index >= item_size)
        return nullptr;
    return &(m_items[index]);
}

const wchar_t *Stock::GetTooltipInfo()
{
    return m_tooltop_info.c_str();
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
    std::lock_guard<std::mutex> lock(m_wndMutex);
    if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
    {
        m_pFloatingWnd->SendMessage(FWND_MSG_REQUEST_DATA, cur_time, 0);
        // DWORD_PTR dwResult = 0;
        // LRESULT lr = ::SendMessageTimeout(
        //     m_pFloatingWnd->GetSafeHwnd(),  // 目标窗口句柄
        //     FWND_MSG_REQUEST_DATA,          // 消息ID
        //     cur_time,                       // wParam
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
        return L"CListery";
    case TMI_COPYRIGHT:
    {
        static std::wstring copyright;
        SYSTEMTIME now_time;
        GetLocalTime(&now_time);
        copyright = L"Copyright © 2022-" + std::to_wstring(now_time.wYear) + L" CListery. All rights reserved.";
        return copyright.c_str();
    }
    case ITMPlugin::TMI_URL:
        return L"https://github.com/CListery/TrafficMonitorPlugins";
    case TMI_VERSION:
        return L"1.14";
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
    for (StockItem item : m_items)
    {
        item.enable = FALSE;
    }
    for (int index = 0; index < g_data.m_setting_data.m_stock_codes.size(); index++)
    {
        std::wstring key = g_data.m_setting_data.m_stock_codes[index];
        if (index > m_items.size() - 1)
        {
            break;
        }
        m_items[index].enable = TRUE;
        m_items[index].stock_id = key;
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

void Stock::SendStockInfoRequest()
{
    if (!m_is_thread_runing) // 确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

void Stock::ShowContextMenu(CWnd *pWnd)
{
    LoadContextMenu();
    CMenu *context_menu = m_menu.GetSubMenu(0);
    if (context_menu != nullptr)
    {
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
