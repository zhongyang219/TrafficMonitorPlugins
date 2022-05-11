#include "pch.h"
#include <wchar.h>
#include "PluginSystemTime.h"
#include "DataManager.h"

CPluginSystemTime::CPluginSystemTime()
{
}

const wchar_t* CPluginSystemTime::GetItemName() const
{
    return CDataManager::Instance().StringRes(IDS_TIME);
}

const wchar_t* CPluginSystemTime::GetItemId() const
{
    return L"ra1YX2g1";
}

const wchar_t* CPluginSystemTime::GetItemLableText() const
{
    return CDataManager::Instance().StringRes(IDS_TIME);
}

const wchar_t* CPluginSystemTime::GetItemValueText() const
{
    return CDataManager::Instance().m_cur_time.c_str();
}

const wchar_t* CPluginSystemTime::GetItemValueSampleText() const
{
    SYSTEMTIME sample_time{};
    GetLocalTime(&sample_time);
    sample_time.wHour = 10;
    sample_time.wMinute = 59;
    sample_time.wMinute = 59;
    return g_data.m_format_helper.GetDateTimeByFormat(g_data.m_setting_data.time_format.c_str(), sample_time);
}
