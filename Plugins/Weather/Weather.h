#pragma once
#include "PluginInterface.h"
#include "WeatherItem.h"
#include <string>
#include "OptionsDlg.h"

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
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void* GetPluginIcon() override;
    void SendWetherInfoQequest();
    void ShowContextMenu(CWnd* pWnd);
    void DisableUpdateWeatherCommand();
    void EnableUpdateWeatherCommand();
    int GetCommandCount() override;
    const wchar_t* GetCommandName(int command_index) override;
    void* GetCommandIcon(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;

    void Init();

private:
    static UINT ThreadCallback(LPVOID dwUser);
    void ParseJsonData(std::string json_data);
    void LoadContextMenu();

private:
    static CWeather m_instance;
    CWeatherItem m_item;
    bool m_is_thread_runing{};
    std::wstring m_tooltop_info;
    COptionsDlg* m_option_dlg{};      //保存选项设置对话框的句柄
    unsigned __int64 m_last_request_time{}; //上次请求天气的时间
    CMenu m_menu;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
