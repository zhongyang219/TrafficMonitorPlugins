#pragma once
#include <map>
#include <functional>

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

using WeatherInfoUpdatedCallback = std::function<void(const std::wstring & info)>;

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

    void UpdateWeather(WeatherInfoUpdatedCallback callback = nullptr);

    std::wstring GetWeatherTemperature() const;
    std::wstring GetRealTimeWeatherInfo(bool brief) const;
    std::wstring GetWeatherAlertsInfo(bool brief) const;
    std::wstring GetWeatherInfo() const;

    std::wstring GetTooptipInfo() const;

    SConfiguration& GetConfig();
    const SConfiguration& GetConfig() const;

    void LoadConfigs(const std::wstring &cfg_dir);
    void SaveConfigs() const;

    HICON GetIcon();
    HICON GetIcon(const std::wstring& w_code);
    
private:
    void _updateWeather(WeatherInfoUpdatedCallback callback = nullptr);
    HICON _getIcon(UINT id);
    UINT _getIconIdBlue(const std::wstring &code) const;

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

    std::map<UINT, HICON> m_icons;
};
