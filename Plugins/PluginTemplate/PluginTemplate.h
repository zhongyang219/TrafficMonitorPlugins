#pragma once
#include "PluginInterface.h"
#include "PluginTemplateItem.h"
#include <string>
#include <map>

#define g_plugin CPluginTemplate::Instance()

struct SettingData
{
    //TODO: 在此添加选项设置的数据
};

class CPluginTemplate : public ITMPlugin
{
private:
    CPluginTemplate();
    ~CPluginTemplate();

public:
    static CPluginTemplate& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;
    virtual void* GetPluginIcon() override;

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    HICON GetIcon(UINT id);
    int DPI(int pixel);

    SettingData m_setting_data;

private:
    static CPluginTemplate m_instance;
    CPluginTemplateItem m_item;
    std::wstring m_tooltip_info;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    ITrafficMonitor* m_app{};

};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
