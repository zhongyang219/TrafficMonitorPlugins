#include "pch.h"
#include "LoudnessMeter.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#pragma comment(lib, "ole32.lib")

CLoudnessMeter CLoudnessMeter::m_instance;

CLoudnessMeter::CLoudnessMeter()
{
    CoInitialize(NULL);
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (pEnumerator == nullptr)
        return;
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (pDevice == nullptr)
        return;
    pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo);
}

CLoudnessMeter::~CLoudnessMeter()
{
    if (pMeterInfo != nullptr)
        pMeterInfo->Release();
    if (pDevice != nullptr)
        pDevice->Release();
    if (pEnumerator != nullptr)
        pEnumerator->Release();
}

CLoudnessMeter& CLoudnessMeter::Instance()
{
    return m_instance;
}

IPluginItem* CLoudnessMeter::GetItem(int index)
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

const wchar_t* CLoudnessMeter::GetTooltipInfo()
{
    return m_tooltip_info.c_str();
}

void CLoudnessMeter::DataRequired()
{
    float peakValue = 0.0f;
    if (pMeterInfo != nullptr)
    {
        pMeterInfo->GetPeakValue(&peakValue);
        if (peakValue > 0.0f)
        {
            float dB = static_cast<float>(20 * log10(peakValue));
            m_item.SetValue(dB, peakValue * 100, CLoudnessMeterItem::DB_VALID);
        }
        else
        {
            m_item.SetValue(0, 0, CLoudnessMeterItem::DB_MUTE);
        }
    }
    else
    {
        m_item.SetValue(0, 0, CLoudnessMeterItem::DB_INVALID);
    }
}

ITMPlugin::OptionReturn CLoudnessMeter::ShowOptionsDialog(void* hParent)
{
    return ITMPlugin::OR_OPTION_NOT_PROVIDED;
}

const wchar_t* CLoudnessMeter::GetInfo(PluginInfoIndex index)
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
        return L"Copyright (C) by Zhong Yang 2025";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    case TMI_VERSION:
        return L"1.00";
    default:
        break;
    }
    return L"";
}

void CLoudnessMeter::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
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

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CLoudnessMeter::Instance();
}
