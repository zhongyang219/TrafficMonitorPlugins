#include "pch.h"
#include "GP.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "utilities/yyjson/yyjson.h"
#include "OptionsDlg.h"
#include <sstream>
#include <iostream>

//static std::wstring convert(double t)
//{
//    std::wstringstream str;
//    str << t;
//    std::wstring result;
//    str >> result;
//    return result;
//}

static std::vector<std::string> split(const std::string& str, const char pattern)
{
    std::vector<std::string> res;
    std::stringstream input(str);   //读取str到字符串流中
    std::string temp;
    //使用getline函数从字符串流中读取,遇到分隔符时停止,和从cin中读取类似
    //注意,getline默认是可以读取空格的
    while (getline(input, temp, pattern))
    {
        res.push_back(temp);
    }
    return res;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t)
{
    std::stringstream str;
    str << t;
    out_type result;
    str >> result;
    return result;
}

template<class out_type, class in_value>
static out_type convert(const in_value& t, bool bISWSring) //转wchar，wstring要用到这个
{
    std::wstringstream str;
    str << t;
    out_type result;
    str >> result;
    return result;
}

GP GP::m_instance;

GP::GP()
{
}

GP& GP::Instance()
{
    return m_instance;
}

UINT GP::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    if (g_data.m_setting_data.m_gp_code == "") {
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
        url += g_data.m_setting_data.m_gp_code;
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

    g_data.ResetText();

    if (json_data == "") {
        CCommon::WriteLog("response is EMPTY!", g_data.m_log_path.c_str());
        return;
    }

    std::vector<std::string> origin_arr = split(json_data, '"');
    if (origin_arr.size() < 1) {
        CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
        return;
    }
    std::string data = origin_arr[1];
    std::vector<std::string> data_arr = split(data, ',');
    if (data_arr.size() != 33) {
        CCommon::WriteLog("data is INVALID!", g_data.m_log_path.c_str());
        return;
    }

    float now { convert<float>(data_arr[3]) };
    float yesterday { convert<float>(data_arr[2]) };

    char buff[32];
    sprintf_s(buff, "%.2f", now);
    g_data.m_gupiao_info.p = CCommon::StrToUnicode(buff);

    sprintf_s(buff, "%.2f%%", ((now - yesterday) / yesterday * 100));
    g_data.m_gupiao_info.pc = CCommon::StrToUnicode(buff);

    //yyjson_doc* doc = yyjson_read(json_data.c_str(), json_data.size(), 0);
    //if (doc != nullptr)
    //{
    //    //获取Json根节点
    //    yyjson_val* root = yyjson_doc_get_root(doc);

    //    g_data.m_gupiao_info.pc = convert(yyjson_get_real(yyjson_obj_get(root, "pc"))) + L"%";
    //    g_data.m_gupiao_info.p = convert(yyjson_get_real(yyjson_obj_get(root, "p")));

    //    const CDataManager::GupiaoInfo& gupiao_current{ g_data.m_gupiao_info };

    //    std::wstringstream wss;
    //    wss << gupiao_current.ToString();
    //    m_tooltop_info = wss.str();

    //    CCommon::WriteLog(m_tooltop_info.c_str(), g_data.m_log_path.c_str());

    //    yyjson_doc_free(doc);
    //}
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
    switch (index)
    {
    case 0:
        return &m_item;
    default:
        break;
    }
    return nullptr;
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
    COptionsDlg dlg(pParent);
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
        break;
    default:
        break;
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
        //点击了“选项”
        if (id == ID_OPTIONS)
        {
            AFX_MANAGE_STATE(AfxGetStaticModuleState());
            COptionsDlg dlg;
            dlg.m_data = g_data.m_setting_data;
            m_option_dlg = &dlg;
            auto rtn = dlg.DoModal();
            m_option_dlg = nullptr;
            if (rtn == IDOK)
            {
                bool gp_code_changed{ g_data.m_setting_data.m_gp_code != dlg.m_data.m_gp_code };
                g_data.m_setting_data = dlg.m_data;
                if (gp_code_changed)
                {
                    GP::Instance().SendGPInfoQequest();
                }
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
    if (m_option_dlg != nullptr)
        m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_GRAYED);
}

void GP::EnableUpdateCommand()
{
    if (m_instance.m_option_dlg != nullptr)
        m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_ENABLED);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &GP::Instance();
}
