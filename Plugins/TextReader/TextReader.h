#pragma once
#include "PluginInterface.h"
#include "TextReaderItem.h"
#include <string>

class CTextReader : public ITMPlugin
{
private:
    CTextReader();

public:
    static CTextReader& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void ShowContextMenu(CWnd* pWnd);
    void SetAutoReadTimer();
    void* GetPluginIcon() override;
    int GetCommandCount() override;
    const wchar_t* GetCommandName(int command_index) override;
    void* GetCommandIcon(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;
    int IsCommandChecked(int command_index) override;

private:
    int ShowOptionsDlg(CWnd* pParent, int cur_tab = 0);
    void LoadContextMenu();

    enum CommandIndex
    {
        CMD_PRE,
        CMD_NEXT,
        CMD_ADD_BOOKMARK,
        CMD_AUTO_PAGE,
        CMD_HIDE,
        CMD_CHAPTER,
        CMD_BOOKMARK,
        CMD_MAX
    };

private:
    static CTextReader m_instance;
    CTextReaderItem m_item;
    CMenu m_menu;
    UINT_PTR m_timer_id{};
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
