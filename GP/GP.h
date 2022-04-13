#pragma once
#include "GP.h"
#include "GPItem.h"
#include <string>
#include "PluginInterface.h"
#include "ManagerDialog.h"
#include <map>

#define kSH "sh" // 上海
#define kSZ "sz" // 深圳
#define kHK "rt_hk" // 香港
#define kMG "gb" // 美国
#define kBJ "bj" // 北京

#define GP_ITEM_MAX 10

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
    void updateItems();

private:
    static GP m_instance;
    vector<GPItem> m_items;
    //GPItem m_item;
    bool m_is_thread_runing{};
    std::wstring m_tooltop_info;
    CManagerDialog* m_option_dlg{};      //保存选项设置对话框的句柄
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
