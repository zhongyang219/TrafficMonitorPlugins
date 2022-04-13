#include "pch.h"
#include "GP.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "ManagerDialog.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "OptionsDlg.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <Windows.h>

GP GP::m_instance;

GP::GP()
{
    m_items = vector<GPItem>(GP_ITEM_MAX);
    fill(m_items.begin(), m_items.end(), GPItem());
    for (int index = 0; index < m_items.size(); index++)
    {
        m_items[index].index = index;
    }
}

GP& GP::Instance()
{
    return m_instance;
}

UINT GP::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    if (g_data.m_setting_data.m_gp_codes.empty()) {
        CCommon::WriteLog(L"gp_code not setting!", g_data.m_log_path.c_str());
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
        //url += g_data.m_setting_data.m_gp_code;
        //url += L"?licence=";
        //url += g_data.m_setting_data.m_licence;

        // https://hq.sinajs.cn/list=sz002497
        std::wstring url{ L"https://hq.sinajs.cn/list=" };
        url += g_data.m_setting_data.m_all_gp_code_str;
        CString strHeaders = _T("Referer: https://finance.sina.com.cn");
        CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

        CString UA = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/98.0.4758.102 Safari/537.36");

        std::string gp_data;
        if (CCommon::GetURL(url, gp_data, false, UA, strHeaders, strHeaders.GetLength()))
        {
            m_instance.ParseJsonData(gp_data);
        }

        //启用选项设置中的“更新”按钮
        m_instance.EnableUpdateCommand();
    }
    return 0;
}

void GP::ParseJsonData(std::string json_data)
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

        GupiaoInfo& gpInfo = g_data.GetGPInfo(key.c_str());

        int data_size = data_arr.size();

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
        gpInfo.p = CCommon::StrToUnicode(buff);

        sprintf_s(buff, "%.2f%%", ((now - yesterday) / yesterday * 100));
        gpInfo.pc = CCommon::StrToUnicode(buff);

        gpInfo.name = name;
    }
}

void GP::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }
}

IPluginItem* GP::GetItem(int index)
{
    if (index >= m_items.size()) {
        return nullptr;
    }
    return &(m_items[index]);
}

const wchar_t* GP::GetTooltipInfo()
{
    return m_tooltop_info.c_str();
}

void GP::DataRequired()
{
    static time_t last_req_time{ -1 };
    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3) {
        last_req_time = cur_time;
        SendGPInfoQequest();
    }
}

ITMPlugin::OptionReturn GP::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    CManagerDialog dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        g_data.m_setting_data = dlg.m_data;
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* GP::GetInfo(PluginInfoIndex index)
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
        return L"https://github.com/CListery";
    case TMI_VERSION:
        return L"1.02";
    default:
        break;
    }
    return L"";
}

void GP::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));
        updateItems();
        break;
    default:
        break;
    }
}

void GP::updateItems()
{
    for (GPItem item : m_items)
    {
        item.enable = FALSE;
    }
    for (int index = 0; index < g_data.m_setting_data.m_gp_codes.size(); index++)
    {
        CString key = g_data.m_setting_data.m_gp_codes[index];
        if (index > m_items.size() - 1)
        {
            break;
        }
        m_items[index].enable = TRUE;
        m_items[index].gp_id = key;
    }
}

void GP::SendGPInfoQequest()
{
    if (!m_is_thread_runing)    //确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

void GP::ShowContextMenu(CWnd* pWnd)
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
            AFX_MANAGE_STATE(AfxGetStaticModuleState());
            CManagerDialog dlg;
            dlg.m_data = g_data.m_setting_data;
            m_option_dlg = &dlg;
            auto rtn = dlg.DoModal();
            m_option_dlg = nullptr;
            if (rtn == IDOK)
            {
                updateItems();
            }
        }
        //点击了“更新”
        else if (id == ID_UPDATE)
        {
            SendGPInfoQequest();
        }
    }
}

void GP::DisableUpdateCommand()
{
    //if (m_option_dlg != nullptr)
    //    m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_GRAYED);
}

void GP::EnableUpdateCommand()
{
    //if (m_instance.m_option_dlg != nullptr)
    //    m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_ENABLED);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &GP::Instance();
}
