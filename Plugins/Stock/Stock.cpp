#include "pch.h"
#include "Stock.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "ManagerDialog.h"
#include "Common.h"

Stock Stock::m_instance;

Stock::Stock()
{
    m_items = vector<StockItem>(Stock_ITEM_MAX);
    fill(m_items.begin(), m_items.end(), StockItem());
    for (int index = 0; index < m_items.size(); index++)
    {
        m_items[index].index = index;
    }
}

Stock& Stock::Instance()
{
    return m_instance;
}

UINT Stock::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    if (g_data.m_setting_data.m_stock_codes.empty()) {
        //CCommon::WriteLog(L"Stock_code not setting!", g_data.m_log_path.c_str());
        g_data.ResetText();
        return 0;
    }

    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3)
    {
        m_instance.m_last_request_time = cur_time;

        if (g_data.m_setting_data.m_full_day != 1) {
            SYSTEMTIME now_time;
            GetLocalTime(&now_time);
            //CCommon::WriteLog(now_time.wHour, g_data.m_log_path.c_str());
            //CCommon::WriteLog(now_time.wMinute, g_data.m_log_path.c_str());
            if (now_time.wHour < 9 || now_time.wHour > 15 || (now_time.wHour == 15 && now_time.wMinute > 30)) {
                CCommon::WriteLog(L"Not currently in trading time!", g_data.m_log_path.c_str());
                g_data.ResetText();
                return 0;
            }
        }

        //禁用选项设置中的“更新”按钮
        m_instance.DisableUpdateCommand();

        //std::wstring url{ L"http://ig507.com/data/time/real/" };
        //url += g_data.m_setting_data.m_stock_code;
        //url += L"?licence=";
        //url += g_data.m_setting_data.m_licence;

        // https://hq.sinajs.cn/list=sz002497
        std::wstring url{ L"https://hq.sinajs.cn/list=" };
        url += CCommon::vectorJoinString(g_data.m_setting_data.m_stock_codes, L",");
        CString strHeaders = _T("Referer: https://finance.sina.com.cn");
        CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

        CString UA = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/98.0.4758.102 Safari/537.36");

        std::string Stock_data;
        if (CCommon::GetURL(url, Stock_data, false, UA, strHeaders, strHeaders.GetLength()))
        {
            m_instance.ParseJsonData(Stock_data);
        }

        //启用选项设置中的“更新”按钮
        m_instance.EnableUpdateCommand();
    }
    return 0;
}

void Stock::ParseJsonData(std::string json_data)
{
    //CCommon::WriteLog(json_data.c_str(), g_data.m_log_path.c_str());
    //LogX(L"ParseJsonData: %s", CCommon::StrToUnicode(json_data.c_str()).c_str());

    g_data.ResetText();

    if (json_data == "") {
        CCommon::WriteLog("response is EMPTY!", g_data.m_log_path.c_str());
        return;
    }

    std::vector<std::string> origin_arr = CCommon::split(json_data, "var hq_str_");
    if (origin_arr.size() < 1) {
        CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
        return;
    }

    for (int index = 0; index < origin_arr.size(); index++)
    {
        std::vector<std::string> item_arr = CCommon::split(origin_arr[index], "=\"");
        if (item_arr.size() < 2)
        {
            CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
            continue;
        }

        std::string key = item_arr[0];
        std::string data = item_arr[1];
        std::vector<std::string> data_arr = CCommon::split(data, ',');

        StockInfo& StockInfo = g_data.GetStockInfo(CCommon::StrToUnicode(key.c_str()));

        int data_size = static_cast<int> (data_arr.size());

        CString name;
        float now = -1;
        float yesterday = -1;
        if (key.find(kMG) == 0)
        {
            if (data_size != 35) {
                CCommon::WriteLog("data is INVALID!", g_data.m_log_path.c_str());
                continue;
            }
            name = data_arr[0].c_str();
            now = { convert<float>(data_arr[1]) };
            yesterday = { convert<float>(data_arr[26]) };
        }
        else if (key.find(kHK) == 0) 
        {
            if (data_size != 25) {
                CCommon::WriteLog("data is INVALID!", g_data.m_log_path.c_str());
                continue;
            }
            name = data_arr[1].c_str();
            now = { convert<float>(data_arr[6]) };
            yesterday = { convert<float>(data_arr[3]) };
        }
        else if (key.find(kBJ) == 0)
        {
            if (data_size != 39) {
                CCommon::WriteLog("data is INVALID!", g_data.m_log_path.c_str());
                continue;
            }
            name = data_arr[0].c_str();
            now = { convert<float>(data_arr[3]) };
            yesterday = { convert<float>(data_arr[2]) };
        }
        else // key.find(kSH) != -1 || key.find(kSZ) != -1
        {
            if (data_size != 33 && data_size != 34) {
                CCommon::WriteLog("data is INVALID!", g_data.m_log_path.c_str());
                continue;
            }
            name = data_arr[0].c_str();
            now = { convert<float>(data_arr[3]) };
            yesterday = { convert<float>(data_arr[2]) };
        }

        char buff[32];
        sprintf_s(buff, "%.2f", now);
        StockInfo.p = CCommon::StrToUnicode(buff);

        sprintf_s(buff, "%.2f%%", ((now - yesterday) / yesterday * 100));
        StockInfo.pc = CCommon::StrToUnicode(buff);

        StockInfo.name = name;
    }
}

void Stock::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }
}

IPluginItem* Stock::GetItem(int index)
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

const wchar_t* Stock::GetTooltipInfo()
{
    return m_tooltop_info.c_str();
}

void Stock::DataRequired()
{
    static time_t last_req_time{ -1 };
    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3) {
        last_req_time = cur_time;
        SendStockInfoQequest();
    }
}

ITMPlugin::OptionReturn Stock::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    if (ShowStockManageDlg(pParent) == IDOK)
    {
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* Stock::GetInfo(PluginInfoIndex index)
{
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"CListery";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by CListery 2022";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
    case TMI_VERSION:
        return L"1.13";
    default:
        break;
    }
    return L"";
}

void Stock::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));
        updateItems();
        break;
    case ITMPlugin::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN:
        //获取TrafficMonitor任务栏窗口中“数值右对齐”设置
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

const wchar_t* Stock::GetCommandName(int command_index)
{
    switch (command_index)
    {
    case 0: return g_data.StringRes(IDS_MENU_UPDATE_STOCK).GetString();
    }
    return nullptr;
}

void Stock::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    switch (command_index)
    {
    case 0:
        SendStockInfoQequest();
        break;
    }
}

void* Stock::GetPluginIcon()
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

INT_PTR Stock::ShowStockManageDlg(CWnd* pWnd)
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
    }
    return rtn;
}

void Stock::SendStockInfoQequest()
{
    if (!m_is_thread_runing)    //确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

void Stock::ShowContextMenu(CWnd* pWnd)
{
    LoadContextMenu();
    CMenu* context_menu = m_menu.GetSubMenu(0);
    if (context_menu != nullptr)
    {
        CPoint point1;
        GetCursorPos(&point1);
        DWORD id = context_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point1.x, point1.y, pWnd);
        //点击了“管理”
        if (id == ID_OPTIONS)
        {
            ShowStockManageDlg(pWnd);
        }
        //点击了“更新”
        else if (id == ID_UPDATE)
        {
            SendStockInfoQequest();
        }
    }
}

void Stock::DisableUpdateCommand()
{
    //if (m_option_dlg != nullptr)
    //    m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_GRAYED);
}

void Stock::EnableUpdateCommand()
{
    //if (m_instance.m_option_dlg != nullptr)
    //    m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_ENABLED);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &Stock::Instance();
}
