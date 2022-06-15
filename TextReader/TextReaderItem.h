#pragma once
#include "PluginInterface.h"

class CTextReaderItem : public IPluginItem
{
public:
    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
    virtual int OnKeboardEvent(int key, bool ctrl, bool shift, bool alt, void* hWnd, int flag) override;
};
