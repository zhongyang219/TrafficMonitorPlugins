#pragma once
#include "PluginInterface.h"
#include "FloatingWnd.h"
#include <vector>

class StockItem : public IPluginItem
{
public:
    StockItem();
    virtual ~StockItem();

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidthEx(void* hDC) const override;

    int index;

    /// <summary>GetItemName / GetItemId 的每实例缓存（避免 static 共享导致宿主读取互相覆盖）</summary>
    mutable std::wstring m_name_cache;
    mutable std::wstring m_id_cache;

    /// <summary>该卡片轮播的股票代码队列</summary>
    std::vector<std::wstring> m_queue;

    /// <summary>当前显示的股票在队列中的索引</summary>
    int m_current_index{0};

    /// <summary>是否锁定（锁定后不翻卡）</summary>
    bool m_locked{false};

    /// <summary>锁定到的股票名（显示锁定标记）</summary>
    std::wstring m_locked_name;

    /// <summary>翻卡计数器（每调用一次 DataRequired 加一，达到间隔翻卡）</summary>
    int m_flip_counter{0};

    /// <summary>翻卡间隔（DataRequired 调用次数）</summary>
    int m_flip_interval{3};

    /// <summary>是否启用（有分配股票）</summary>
    bool enable{false};

    /// <summary>固定宽度（居右对齐时使用，缓存队列中最宽文本）</summary>
    mutable int m_fixed_width{0};

    /// <summary>是否需要重新计算固定宽度（队列变化时设为 true）</summary>
    mutable bool m_needs_width_recalc{true};

    /// <summary>
    /// 翻卡到下一只股票
    /// </summary>
    void FlipToNext();

    /// <summary>
    /// 切换锁定/解锁
    /// </summary>
    void ToggleLock();

    /// <summary>
    /// 检查并执行翻卡（由 DataRequired 调用）
    /// </summary>
    void AdvanceIfNeeded();

    /// <summary>
    /// 获取当前显示的股票 ID（空字符串表示无数据）
    /// </summary>
    std::wstring GetCurrentStockId() const;

    /// <summary>
    /// 获取当前显示文本
    /// </summary>
    std::wstring GetDisplayText(bool include_name) const;
};
