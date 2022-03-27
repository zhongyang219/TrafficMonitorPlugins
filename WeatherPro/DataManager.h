#pragma once
#include <map>
#include "DataQuerier.h"

enum class EWeatherInfoType
{
    WEATHER_REALTIME,
    WEATHER_TODAY,
    WEATHER_TOMMROW
};

struct SConfiguration
{
    SConfiguration();

    EWeatherInfoType m_wit;
    bool m_show_weather_icon;
    bool m_show_weather_in_tooltips;
    bool m_show_brief_rt_weather_info;
    bool m_show_weather_alerts;
    bool m_show_brief_weather_alert_info;
};

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();
public:
    static const CDataManager& Instance();
    static CDataManager& InstanceRef();

    const CString& StringRes(UINT id);

    int DPI(int pixel) const;
    float DPIF(float pixel) const;
    int RDPI(int pixel) const;

    void SetCurrentCityInfo(SCityInfo info);
    const SCityInfo& GetCurrentCityInfo() const;

    void UpdateWeather();

    std::wstring GetWeatherTemperature() const;
    std::wstring GetRealTimeWeatherInfo(bool brief) const;
    std::wstring GetWeatherAlertsInfo(bool brief) const;
    std::wstring GetWeatherInfo() const;

    SConfiguration& GetConfig();
    const SConfiguration& GetConfig() const;

    void LoadConfigs(const std::wstring &cfg_dir);
    void SaveConfigs() const;
    
private:
    void _updateWeather();

    static CDataManager m_instance;
    int m_dpi;
    std::map<UINT, CString> m_string_resources;

    SConfiguration m_config;
    std::wstring m_config_file_path;

    SCityInfo m_currentCityInfo;

    SRealTimeWeather m_rt_weather;
    WeatherAlertList m_weather_alerts;
    SWeatherInfo m_weather_today;
    SWeatherInfo m_weather_tommrow;
};
