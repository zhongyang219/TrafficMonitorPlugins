#pragma once
#include "GP.h"
#include "GPItem.h"
#include <string>
#include "PluginInterface.h"
#include "OptionsDlg.h"

class GP : public ITMPlugin
{
private:
    GP();

public:
    static GP& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void SendGPInfoQequest();
    void ShowContextMenu(CWnd* pWnd);
    void DisableUpdateCommand();
    void EnableUpdateCommand();

private:
    static UINT ThreadCallback(LPVOID dwUser);
    void ParseJsonData(std::string json_data);
    void LoadContextMenu();

private:
    static GP m_instance;
    GPItem m_item;
    bool m_is_thread_runing{};
    std::wstring m_tooltop_info;
    COptionsDlg* m_option_dlg{};      //保存选项设置对话框的句柄
    unsigned __int64 m_last_request_time{}; //上次请求的时间
    CMenu m_menu;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
