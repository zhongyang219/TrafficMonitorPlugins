#include "pch.h"
#include "StockItem.h"
#include "DataManager.h"
#include "Stock.h"
#include "Common.h"
#include <algorithm>
#include "FloatingWnd.h"
#undef min
#undef max

StockItem::StockItem()
{
}

StockItem::~StockItem()
{
}

const wchar_t *StockItem::GetItemName() const
{
    // 每实例缓存，避免 static 共享导致宿主存储的指针互相覆盖
    if (m_name_cache.empty())
    {
        m_name_cache = g_data.StringRes(IDS_PLUGIN_ITEM_NAME).GetString();
        m_name_cache += std::to_wstring(index + 1);
    }
    return m_name_cache.c_str();
}

const wchar_t *StockItem::GetItemId() const
{
    if (m_id_cache.empty())
    {
        m_id_cache = L"StockPlus_";
        m_id_cache += std::to_wstring(index);
    }
    return m_id_cache.c_str();
}

const wchar_t *StockItem::GetItemLableText() const
{
    return L"";
}

const wchar_t *StockItem::GetItemValueText() const
{
    return L"";
}

bool StockItem::IsCustomDraw() const
{
    return true;
}

int StockItem::GetItemWidthEx(void *hDC) const
{
    CDC *pDC = CDC::FromHandle((HDC)hDC);

    if (g_data.m_setting_data.m_align_right)
    {
        // 居右对齐：使用固定宽度（缓存）
        if (m_needs_width_recalc || m_fixed_width <= 0)
        {
            int maxW = 80;
            for (const auto &sid : m_queue)
            {
                if (sid.empty()) continue;
                auto pData = g_data.GetStockData(sid);
                if (pData && pData->info.is_ok)
                {
                    std::wstring txt = pData->GetCurrentDisplay(g_data.m_setting_data.m_show_stock_name);
                    int w = pDC->GetTextExtent(txt.c_str()).cx;
                    if (w > maxW) maxW = w;
                }
            }
            m_fixed_width = maxW + 24; // +24 留边距
            m_needs_width_recalc = false;
        }
        return m_fixed_width;
    }
    else
    {
        // 居左对齐：按当前股票宽度，翻卡时会跳动
        std::wstring txt = GetDisplayText(g_data.m_setting_data.m_show_stock_name);
        int width = pDC->GetTextExtent(txt.c_str()).cx + 20;
        return (std::max)(width, 80);
    }
}

std::wstring StockItem::GetCurrentStockId() const
{
    if (m_queue.empty())
        return L"";
    if (m_current_index < 0 || m_current_index >= (int)m_queue.size())
        return L"";
    return m_queue[m_current_index];
}

std::wstring StockItem::GetDisplayText(bool include_name) const
{
    std::wstring sid = GetCurrentStockId();
    if (sid.empty())
        return L"-- --";

    auto pData = g_data.GetStockData(sid);
    if (!pData || !pData->info.is_ok)
        return sid + L" ...";

    return pData->GetCurrentDisplay(include_name);
}

void StockItem::FlipToNext()
{
    if (m_locked || m_queue.size() <= 1)
        return;
    m_current_index = (m_current_index + 1) % (int)m_queue.size();
}

void StockItem::ToggleLock()
{
    m_locked = !m_locked;
    if (m_locked)
    {
        std::wstring sid = GetCurrentStockId();
        if (!sid.empty())
        {
            auto pData = g_data.GetStockData(sid);
            if (pData && pData->info.is_ok)
                m_locked_name = pData->info.displayName;
        }
    }
    else
    {
        m_locked_name.clear();
    }
}

void StockItem::AdvanceIfNeeded()
{
    if (!g_data.m_setting_data.m_carousel_enabled)
        return;
    if (m_locked || m_queue.size() <= 1)
        return;
    // 总股票数 <= 2 时不轮播
    if (g_data.m_setting_data.m_stock_codes.size() <= 2)
        return;
    m_flip_counter++;
    if (m_flip_counter >= m_flip_interval)
    {
        m_flip_counter = 0;
        FlipToNext();
    }
}

void StockItem::DrawItem(void *hDC, int x, int y, int w, int h, bool dark_mode)
{
    CDC *pDC = CDC::FromHandle((HDC)hDC);
    CRect rect(CPoint(x, y), CSize(w, h));

    COLORREF color_default, color_red, color_green, color_lock_border;
    if (dark_mode)
    {
        color_default = RGB(255, 255, 255);
        color_red = RGB(255, 121, 120);
        color_green = RGB(111, 215, 149);
        color_lock_border = RGB(137, 180, 250);
    }
    else
    {
        color_default = RGB(0, 0, 0);
        color_red = RGB(195, 0, 0);
        color_green = RGB(46, 139, 87);
        color_lock_border = RGB(0, 102, 204);
    }

    if (m_locked)
    {
        CPen lock_pen(PS_SOLID, 2, color_lock_border);
        CPen *oldPen = pDC->SelectObject(&lock_pen);
        CBrush *oldBrush = (CBrush *)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Rectangle(rect);
        pDC->SelectObject(oldPen);
        pDC->SelectObject(oldBrush);
    }

    std::wstring sid = GetCurrentStockId();
    if (sid.empty())
    {
        pDC->SetTextColor(color_default);
        pDC->DrawText(L"--", rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    auto pData = g_data.GetStockData(sid);
    if (!pData || !pData->info.is_ok)
    {
        pDC->SetTextColor(color_default);
        CString failTxt = g_data.StringRes(IDS_LOAD_FAIL);
        pDC->DrawText(failTxt, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    CRect textRect = rect;
    textRect.DeflateRect(4, 0);
    bool alignRight = g_data.m_setting_data.m_align_right;

    if (g_data.m_setting_data.m_show_stock_name)
    {
        pDC->SetTextColor(color_default);
        CString nameText(pData->info.displayName.c_str());
        if (m_locked)
            nameText += _T(" \xD83D\xDD12");
        CRect nameRect = textRect;
        if (alignRight)
        {
            // 居右对齐：名称靠左，留出右侧空间给数值
            nameRect.right = nameRect.left + pDC->GetTextExtent(nameText).cx + 4;
        }
        else
        {
            nameRect.right = nameRect.left + pDC->GetTextExtent(nameText).cx + 4;
        }
        pDC->DrawText(nameText, nameRect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (!alignRight)
            textRect.left = nameRect.right;
    }

    if (g_data.m_setting_data.m_color_with_price)
    {
        bool isDown = (pData->realTimeData.displayFluctuation.find(L'-') != std::wstring::npos);
        pDC->SetTextColor(isDown ? color_green : color_red);
    }
    else
    {
        pDC->SetTextColor(color_default);
    }

    std::wstring display = pData->GetCurrentDisplay(false);
    UINT fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    if (alignRight)
        fmt |= DT_RIGHT;
    pDC->DrawText(display.c_str(), textRect, fmt);
}

const wchar_t *StockItem::GetItemValueSampleText() const
{
    return L"";
}

int StockItem::OnMouseEvent(MouseEventType type, int x, int y, void *hWnd, int flag)
{
    CWnd *pWnd = CWnd::FromHandle((HWND)hWnd);
    switch (type)
    {
    case IPluginItem::MT_LCLICKED:
        // 单击无操作
        return 0;
    case IPluginItem::MT_DBCLICKED:
    {
        // 双击：打开股票列表对话框
        Stock::Instance().ShowStockListDlg(pWnd);
        return 1;
    }
    case IPluginItem::MT_RCLICKED:
        Stock::Instance().ShowContextMenu(pWnd);
        return 1;
    default:
        break;
    }
    return 0;
}
