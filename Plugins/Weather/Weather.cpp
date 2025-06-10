#include "pch.h"
#include "Weather.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "utilities/yyjson/yyjson.h"
#include "OptionsDlg.h"
#include <sstream>
#include "CurLocationHelper.h"
#include "utilities/Common.h"
#include "utilities/JsonHelper.h"
#include <iomanip>
#include "HistoryWeatherMgr.h"
#include "WeatherHistoryDlg.h"

CWeather CWeather::m_instance;

CWeather::CWeather()
{
}

CWeather& CWeather::Instance()
{
    return m_instance;
}

UINT CWeather::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3)  //确保请求天气信息的时间距离上次请求时超过3秒
    {
        m_instance.m_last_request_time = cur_time;

        //禁用选项设置中的“更新”按钮
        m_instance.DisableUpdateWeatherCommand();

        if (g_data.m_setting_data.auto_locate)      //自动获取当前城市
        {
            std::wstring cur_city = CCurLocationHelper::GetCurrentCity();
            CCurLocationHelper::Location location = CCurLocationHelper::ParseCityName(cur_city);
            int auto_located_city = CCurLocationHelper::FindCityCodeItem(location);
            g_data.m_auto_locate_succeed = (auto_located_city >= 0);
            g_data.m_auto_located = true;
            if (g_data.m_auto_locate_succeed)
                g_data.m_setting_data.m_city_index = auto_located_city;
        }

        //获取天气信息
        std::wstring url{ L"http://t.weather.itboy.net/api/weather/city/" };
        url += g_data.CurCity().code;
        std::string weather_data;
        if (CCommon::GetURL(url, weather_data))
        {
            if (m_instance.ParseJsonData(weather_data))
            {
                //解析成功时，将天气信息保存到Weather.json
                std::ofstream stream{ g_data.m_config_dir + L"Weather.json" };
                stream << weather_data;
                //保存历史天气数据
                g_data.HistoryWeatherMgr().Save();
            }
        }

        if (m_instance.m_option_dlg != nullptr)
            m_instance.m_option_dlg->UpdateAutoLocteResult();

        //启用选项设置中的“更新”按钮
        m_instance.EnableUpdateWeatherCommand();
    }
    return 0;
}

static void ParseWeatherInfo(WeatherInfo& weather_info, yyjson_val* forecast)
{
    if (forecast != nullptr)
    {
        weather_info.m_type = utilities::JsonHelper::GetJsonWString(forecast, "type");
        weather_info.m_high = utilities::JsonHelper::GetJsonWString(forecast, "high");
        weather_info.m_low = utilities::JsonHelper::GetJsonWString(forecast, "low");

        //去掉前面的“高温”和“低温”
        weather_info.m_high = weather_info.m_high.substr(2);
        weather_info.m_low = weather_info.m_low.substr(2);
        //去年低温后面的摄氏度符号
        weather_info.m_low.pop_back();
        utilities::StringHelper::StringNormalize(weather_info.m_high);
        utilities::StringHelper::StringNormalize(weather_info.m_low);

        //风向和风力
        std::wstring fx = utilities::JsonHelper::GetJsonWString(forecast, "fx");
        std::wstring fl = utilities::JsonHelper::GetJsonWString(forecast, "fl");
        weather_info.m_wind = fx + L' ' + fl;
    }
}

//获取一个日期的字符串。如果日期是今天、明天或后天，则返回“今天”、“明天”和“后天”的字符串，否则返回几月几日
static std::wstring GetDateString(CTime date)
{
    date = CCommon::GetDateOnly(date);
    CTime now_date = CCommon::GetDateOnly(CTime::GetCurrentTime());
    const CTimeSpan one_day_span(1, 0, 0, 0);
    CTime tomorrow_date = now_date + one_day_span;
    CTime the_day_after_tomorrow_date = tomorrow_date + one_day_span;
    CTime yesterday_date = now_date - one_day_span;
    if (date == now_date)
        return g_data.StringRes(IDS_TODAY_WEATHER).GetString();
    else if (date == tomorrow_date)
        return g_data.StringRes(IDS_TOMMORROW_WEATHER).GetString();
    else if (date == the_day_after_tomorrow_date)
        return  g_data.StringRes(IDS_THE_DAY_AFTER_TOMMORROW_WEATHER).GetString();
    else if (date == yesterday_date)
        return  g_data.StringRes(IDS_YESTERDAY).GetString();
    else
        return utilities::StringHelper::StringFormat(g_data.StringRes(IDS_DATE_FORMAT).GetString(), { date.GetMonth(), date.GetDay() });
}

bool CWeather::ParseJsonData(std::string json_data)
{
    yyjson_doc* doc = yyjson_read(json_data.c_str(), json_data.size(), 0);
    if (doc == nullptr)
        return false;
    //获取Json根节点
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (root == nullptr)
        return false;

    g_data.ResetText();

    //获取日期
    int year{};
    int month{};
    int day{};
    std::string str_date = utilities::JsonHelper::GetJsonString(root, "date");
    if (str_date.size() >= 4)
        year = atoi(str_date.substr(0, 4).c_str());
    if (str_date.size() >= 6)
        month = atoi(str_date.substr(4, 2).c_str());
    if (str_date.size() >= 8)
        day = atoi(str_date.substr(6, 2).c_str());

    //获取城市
    yyjson_val* city_info = yyjson_obj_get(root, "cityInfo");
    std::wstring str_city = utilities::JsonHelper::GetJsonWString(city_info, "city");

    //获取时间
    std::string str_time = utilities::JsonHelper::GetJsonString(city_info, "updateTime");
    std::vector<std::string> time_split;
    utilities::StringHelper::StringSplit(str_time, ':', time_split);
    int hour{};
    int minute{};
    if (time_split.size() >= 1)
        hour = atoi(time_split[0].c_str());
    if (time_split.size() >= 2)
        minute = atoi(time_split[1].c_str());
    g_data.m_update_time = CTime(year, month, day, hour, minute, 0);

    //获取当前天气
    yyjson_val* data = yyjson_obj_get(root, "data");
    g_data.m_weather_info[WEATHER_CURRENT].m_high = utilities::JsonHelper::GetJsonWString(data, "wendu") + L"℃";
    g_data.m_weather_info[WEATHER_CURRENT].is_cur_weather = true;

    //空气质量
    g_data.m_pm2_5 = utilities::JsonHelper::GetJsonFloat(data, "pm25");
    g_data.m_quality = utilities::JsonHelper::GetJsonWString(data, "quality");

    //获取3天的天气
    yyjson_val* forecast_arr = yyjson_obj_get(data, "forecast");
    if (forecast_arr != nullptr && yyjson_is_arr(forecast_arr))
    {
        yyjson_val* forecast_today = yyjson_arr_get_first(forecast_arr);
        yyjson_val* forecast_tommorrow = yyjson_arr_get(forecast_arr, 1);
        yyjson_val* forecast_day2 = yyjson_arr_get(forecast_arr, 2);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TODAY], forecast_today);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TOMMORROW], forecast_tommorrow);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_DAY2], forecast_day2);
        g_data.m_weather_info[WEATHER_CURRENT].m_type = g_data.m_weather_info[WEATHER_TODAY].m_type;
        //添加到历史记录
        g_data.HistoryWeatherMgr().AddWeatherInfo(forecast_today);
        g_data.HistoryWeatherMgr().AddWeatherInfo(forecast_tommorrow);
        g_data.HistoryWeatherMgr().AddWeatherInfo(forecast_day2);
        //获取所有天气并添加到历史记录
        for (int i = 3; ; i++)
        {
            yyjson_val* forecast = yyjson_arr_get(forecast_arr, i);
            if (forecast != nullptr)
                g_data.HistoryWeatherMgr().AddWeatherInfo(forecast);
            else
                break;
        }
    }

    //生成鼠标提示字符串
    const WeatherInfo& weather_current{ g_data.m_weather_info[WEATHER_CURRENT] };
    const WeatherInfo& weather_today{ g_data.m_weather_info[WEATHER_TODAY] };
    const WeatherInfo& weather_tomorrow{ g_data.m_weather_info[WEATHER_TOMMORROW] };
    const WeatherInfo& weather_day2{ g_data.m_weather_info[WEATHER_DAY2] };
    CTime update_date = CCommon::GetDateOnly(g_data.m_update_time);
    const CTimeSpan one_day_span(1, 0, 0, 0);
    CTime tomorrow_date = update_date + one_day_span;
    CTime the_day_after_tomorrow_date = tomorrow_date + one_day_span;
    std::wstringstream wss;
    wss << str_city << L' ' << weather_current.ToString()
        << L" PM2.5: " << g_data.GetPM25AsString().GetString() << L' ' << g_data.m_quality
        << std::endl << g_data.StringRes(IDS_UPDATE_TIME).GetString() << L": " << g_data.GetUpdateTimeAsString().GetString()
        << std::endl << GetDateString(update_date) << L": " << weather_today.ToString()
        << std::endl << GetDateString(tomorrow_date) << L": " << weather_tomorrow.ToString()
        << std::endl << GetDateString(the_day_after_tomorrow_date) << L": " << weather_day2.ToString()
        ;
    m_tooltop_info = wss.str();

    yyjson_doc_free(doc);
    return true;
}

void CWeather::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }
}

IPluginItem* CWeather::GetItem(int index)
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

const wchar_t* CWeather::GetTooltipInfo()
{
    if (g_data.m_setting_data.m_show_weather_in_tooltips)
        return m_tooltop_info.c_str();
    else
        return L"";
}

void CWeather::DataRequired()
{
    static int last_hour{ -1 };
    SYSTEMTIME system_time{};
    GetLocalTime(&system_time);
    //每隔一小时获取一次天气
    if (system_time.wHour != last_hour)
    {
        last_hour = system_time.wHour;
        SendWetherInfoQequest();
    }

}

ITMPlugin::OptionReturn CWeather::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    //g_data.DPIFromWindow(pParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    m_option_dlg = &dlg;
    auto rtn = dlg.DoModal();
    m_option_dlg = nullptr;
    if (rtn == IDOK)
    {
        bool city_changed{ g_data.m_setting_data.m_city_index != dlg.m_data.m_city_index ||
            g_data.m_setting_data.auto_locate != dlg.m_data.auto_locate };
        g_data.m_setting_data = dlg.m_data;
        if (city_changed)
        {
            CWeather::Instance().SendWetherInfoQequest();   //城市改变后，重新发送天气请求
        }
        g_data.SaveConfig();
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CWeather::GetInfo(PluginInfoIndex index)
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
        return L"1.03";
    default:
        break;
    }
    return L"";
}

void CWeather::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
    {
        //从配置文件读取配置
        std::wstring cfg_dir(data);
        cfg_dir += L"Weather\\";
        CreateDirectory(cfg_dir.c_str(), NULL);
        g_data.LoadConfig(cfg_dir);
        //初始化天气
        Init();
    }
        break;
    default:
        break;
    }
}

void* CWeather::GetPluginIcon()
{
    return g_data.GetIcon(IDI_WEATHER);
}

void CWeather::SendWetherInfoQequest()
{
    if (!m_is_thread_runing)    //确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

void CWeather::ShowContextMenu(CWnd* pWnd)
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
                bool city_changed{ g_data.m_setting_data.m_city_index != dlg.m_data.m_city_index };
                g_data.m_setting_data = dlg.m_data;
                if (city_changed)
                {
                    CWeather::Instance().SendWetherInfoQequest();   //城市改变后，重新发送天气请求
                }
            }
        }
        //点击了“更新天气”
        else if (id == ID_UPDATE_WEATHER)
        {
            SendWetherInfoQequest();
        }
    }
}

void CWeather::DisableUpdateWeatherCommand()
{
    if (m_option_dlg != nullptr)
        m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE_WEATHER, MF_BYCOMMAND | MF_GRAYED);
}

void CWeather::EnableUpdateWeatherCommand()
{
    if (m_instance.m_option_dlg != nullptr)
        m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE_WEATHER, MF_BYCOMMAND | MF_ENABLED);
}

const wchar_t* CWeather::GetCommandName(int command_index)
{
    switch (command_index)
    {
    case 0:
        return g_data.StringRes(IDS_UPDATE_WEATHER).GetString();
    case 1:
        return g_data.StringRes(IDS_WEATHER_HISTORY).GetString();
    default:
        return nullptr;
    }
}

void* CWeather::GetCommandIcon(int command_index)
{
    switch (command_index)
    {
    case 0:
        return g_data.GetIcon(IDI_UPDATE);
        break;
    default:
        return nullptr;
    }
}

void CWeather::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    CWnd* parent = CWnd::FromHandle((HWND)hWnd);
    switch (command_index)
    {
        //更新天气
    case 0:
        SendWetherInfoQequest();
        break;
        //显示天气历史记录
    case 1:
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        CWeatherHistoryDlg dlg(parent);
        dlg.DoModal();
    }
    default:
        break;
    }
}

void CWeather::Init()
{
    //从Weather.json获取天气
    std::wstring json_path{ g_data.m_config_dir + L"Weather.json" };
    std::string weather_data;
    if (utilities::CCommon::GetFileContent(json_path.c_str(), weather_data))
    {
        ParseJsonData(weather_data);
    }
}

int CWeather::GetCommandCount()
{
    return 2;
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CWeather::Instance();
}
