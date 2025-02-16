#pragma once
#include "PluginInterface.h"

class CLoudnessMeterItem : public IPluginItem
{
public:
    CLoudnessMeterItem();

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

    enum DBState
    {
        DB_INVALID,     //无效
        DB_MUTE,        //静音
        DB_VALID        //有效
    };

    void SetValue(const float value, float percent, DBState state);
private:
    float m_db{ 1 };        //响度的分贝值
    float m_percent{};
    DBState m_state;
    CFont m_font;
};
