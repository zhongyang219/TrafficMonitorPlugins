#include "pch.h"
#include "TextReader.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "OptionsContainerDlg.h"

CTextReader CTextReader::m_instance;

CTextReader::CTextReader()
{
}

CTextReader& CTextReader::Instance()
{
    return m_instance;
}

IPluginItem* CTextReader::GetItem(int index)
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

const wchar_t* CTextReader::GetTooltipInfo()
{
    return m_tooltip_info.c_str();
}

void CTextReader::DataRequired()
{
    //TODO: 在此添加获取监控数据的代码
}

ITMPlugin::OptionReturn CTextReader::ShowOptionsDialog(void* hParent)
{
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    int result = ShowOptionsDlg(pParent);
    return (result == IDOK ? ITMPlugin::OR_OPTION_CHANGED : ITMPlugin::OR_OPTION_UNCHANGED);
}

const wchar_t* CTextReader::GetInfo(PluginInfoIndex index)
{
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"zhongyang219";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Zhong Yang 2022";
    case ITMPlugin::TMI_URL:
        //TODO: 在此返回URL
        return L"";
        break;
    case TMI_VERSION:
        //TODO: 在此修改插件的版本
        return L"1.00";
    default:
        break;
    }
    return L"";
}

void CTextReader::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
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


void CTextReader::ShowContextMenu(CWnd* pWnd)
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
            ShowOptionsDlg(pWnd);
        }
        //点击了“章节”
        else if (id == ID_CHAPTERS)
        {
            ShowOptionsDlg(pWnd, 1);
        }
        //点击了“上一页”
        else if (id == ID_PREVIOUS)
        {
            g_data.PageUp();
        }
        //点击了“下一页”
        else if (id == ID_NEXT)
        {
            g_data.PageDown();
        }
    }

}


int CTextReader::ShowOptionsDlg(CWnd* pParent, int cur_tab /*= 0*/)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    COptionsContainerDlg dlg(cur_tab, pParent);
    dlg.m_options_dlg.m_data = g_data.m_setting_data;
    int rtn = dlg.DoModal();
    if (rtn == IDOK)
    {
        g_data.m_setting_data = dlg.m_options_dlg.m_data;
        int chapter_position = dlg.m_chapter_dlg.GetSelectedPosition();
        if (chapter_position >= 0)
            g_data.m_setting_data.current_position = chapter_position;
        g_data.SaveConfig();
    }
    return rtn;
}

void CTextReader::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }

}


ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CTextReader::Instance();
}
