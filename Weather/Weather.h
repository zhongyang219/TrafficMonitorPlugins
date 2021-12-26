#pragma once
#include "PluginInterface.h"
#include "WeatherItem.h"
#include <string>

class CWeather : public ITMPlugin
{
private:
    CWeather();

public:
    static CWeather& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    void SendWetherInfoQequest();

private:
    static UINT ThreadCallback(LPVOID dwUser);
    void ParseJsonData(std::string json_data);

private:
    static CWeather m_instance;
    CWeatherItem m_item;
    bool m_is_thread_runing{};
    std::wstring m_tooltop_info;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
