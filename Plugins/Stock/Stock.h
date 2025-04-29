#pragma once
#include "Stock.h"
#include "StockItem.h"
#include <string>
#include "PluginInterface.h"
#include "ManagerDialog.h"
#include <map>
#include <vector>
#include <mutex>

constexpr auto kSH = L"sh";    // 上海
constexpr auto kSZ = L"sz";    // 深圳
constexpr auto kHK = L"rt_hk"; // 香港
constexpr auto kMG = L"gb_";   // 美国
constexpr auto kBJ = L"bj";    // 北京

const std::vector<CString> StockTypeSet{kSH, kSZ, kHK, kMG, kBJ};

#define Stock_ITEM_MAX 10

class Stock : public ITMPlugin
{
private:
    Stock();
    virtual ~Stock();

public:
    static Stock &Instance();

    virtual IPluginItem *GetItem(int index) override;
    virtual const wchar_t *GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void *hParent) override;
    virtual const wchar_t *GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t *data) override;
    virtual int GetCommandCount() override;
    virtual const wchar_t *GetCommandName(int command_index) override;
    virtual void OnPluginCommand(int command_index, void *hWnd, void *para) override;
    virtual void *GetPluginIcon() override;

    INT_PTR ShowStockManageDlg(CWnd *pWnd);
    void SendStockInfoRequest();
    void ShowContextMenu(CWnd *pWnd);
    void DisableUpdateCommand();
    void EnableUpdateCommand();

    void ShowFloatingWnd(void *hWnd, CPoint ptScreen, std::wstring stock_id);
    void DestroyFloatingWnd();
    void UpdateKLine();

public:
    std::mutex m_stockDataMutex;

private:
    static UINT ThreadCallback(LPVOID dwUser);
    void LoadContextMenu();
    void updateItems();

private:
    static Stock m_instance;
    vector<StockItem> m_items;

    bool m_is_thread_runing{};
    std::wstring m_tooltop_info;
    CManagerDialog *m_option_dlg{};         // 保存选项设置对话框的句柄
    unsigned __int64 m_last_request_time{}; // 上次请求的时间
    CMenu m_menu;

    std::mutex m_wndMutex;
    CFloatingWnd *m_pFloatingWnd;
};

#ifdef __cplusplus
extern "C"
{
#endif
    __declspec(dllexport) ITMPlugin *TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
