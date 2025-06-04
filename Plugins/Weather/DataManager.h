#pragma once
#include <string>
#include <map>
#include "resource.h"
#include "CityCode.h"
#include "HistoryWeatherMgr.h"
#include "CommonData.h"

#define g_data CDataManager::Instance()

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);
    CityCodeItem CurCity() const;
    void ResetText();
    HICON GetWeatherIcon(const std::wstring weather_type);
    CHistoryWeatherMgr& HistoryWeatherMgr();

    WeatherInfo& GetWeather();

    std::map<WeahterSelected, WeatherInfo> m_weather_info;
    CTime m_update_time;        //更新时间
    float m_pm2_5{};            //PM2.5
    std::wstring m_quality;     //空气质量

    CString GetUpdateTimeAsString();  //获取更新日间的字符串格式
    CString GetPM25AsString() const;

    SettingData m_setting_data;
    std::wstring m_config_dir;
    bool m_auto_located{};          //是否执行了自动定位
    bool m_auto_locate_succeed{};   //自动定位是否成功

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    std::map<std::wstring, UINT> m_weather_icon_id;     //保存所有天气图标的ID
    int m_dpi{ 96 };
    CHistoryWeatherMgr m_history_weather_mgr;
};
