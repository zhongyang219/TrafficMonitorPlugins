// DrawScrollView.cpp : 实现文件
//

#include "stdafx.h"
#include "PluginTester.h"
#include "PreviewScrollView.h"
#include "DrawCommon.h"
#include "PluginTesterDlg.h"

// DrawScrollView

IMPLEMENT_DYNCREATE(CDrawScrollView, CScrollView)

CDrawScrollView::CDrawScrollView()
{
}

CDrawScrollView::~CDrawScrollView()
{
}


BEGIN_MESSAGE_MAP(CDrawScrollView, CScrollView)
	ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()


// DrawScrollView 绘图

void CDrawScrollView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	// TODO: 计算此视图的合计大小
	m_size.cx = 0;	
	m_size.cy = 0;	
	SetScrollSizes(MM_TEXT, m_size);
}

void CDrawScrollView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: 在此添加绘制代码

	CRect draw_rect(CPoint(0, 0), m_size);
    pDC->FillSolidRect(draw_rect, RGB(222, 238, 255));

    //双缓冲绘图
    CDrawDoubleBuffer draw_double_buffer(pDC, draw_rect);
    //m_draw_rect = draw_rect;

    CDrawCommon drawer;
    drawer.Create(draw_double_buffer.GetMemDC(), this);

    draw_rect.MoveToY(0);
    drawer.FillRect(draw_rect, GetSysColor(COLOR_WINDOW));
    drawer.SetFont(theApp.m_pMainWnd->GetFont());

    //绘制插件的显示项目
    int index = 0;

    CPluginTesterDlg* dlg = dynamic_cast<CPluginTesterDlg*>(theApp.m_pMainWnd);
    if (dlg == nullptr)
        return;

    PluginInfo cur_plugin = dlg->GetCurrentPlugin();
    for (const auto& item : cur_plugin.plugin_items)
    {
        if (item != nullptr)
        {
            //绘制文本
            CRect rc_text;
            rc_text.left = theApp.DPI(16);
            rc_text.top = theApp.DPI(8) + index * (theApp.DPI(56));
            rc_text.bottom = rc_text.top + theApp.DPI(24);
            rc_text.right = rc_text.left + theApp.DPI(120);
            drawer.DrawWindowText(rc_text, item->GetItemName(), RGB(0, 0, 0));

            //绘制显示项目
            CRect rc_item = rc_text;
            rc_item.MoveToY(rc_text.bottom);
            int item_width = dlg->GetItemWidth(item, drawer.GetDC());
            if (item_width > 0)
                rc_item.right = rc_item.left + item_width;
            drawer.FillRect(rc_item, RGB(205, 221, 234));

            //保存显示项目位置
            m_plugin_item_rect[item->GetItemId()] = rc_item;

            //如果当前视图的宽度小于显示项目的宽度，则修正视图的宽度
            if (m_size.cx < rc_item.Width() + theApp.DPI(32))
            {
                m_size.cx = rc_item.Width() + theApp.DPI(32);
                SetScrollSizes(MM_TEXT, m_size);
            }

            //保存显示项目的位置
            //SavePluginItemRect(item, rc_item);

            if (item->IsCustomDraw())
            {
                item->DrawItem(drawer.GetDC()->GetSafeHdc(), rc_item.left, rc_item.top, rc_item.Width(), rc_item.Height(), false);
            }
            else
            {
                CRect rc_label = rc_item;
                rc_label.right = rc_label.left + drawer.GetDC()->GetTextExtent(item->GetItemLableText()).cx;
                drawer.DrawWindowText(rc_label, item->GetItemLableText(), RGB(0, 0, 0));

                CRect rc_value = rc_item;
                rc_value.left = rc_label.right + theApp.DPI(6);
                drawer.DrawWindowText(rc_value, item->GetItemValueText(), RGB(0, 0, 0));
            }
            index++;
        }
    }
}


// DrawScrollView 诊断
#ifdef _DEBUG
void CDrawScrollView::AssertValid() const
{
	CScrollView::AssertValid();
}

#ifndef _WIN32_WCE
void CDrawScrollView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}
#endif
#endif //_DEBUG

void CDrawScrollView::InitialUpdate()
{
	OnInitialUpdate();
}

void CDrawScrollView::SetSize(int width,int hight)
{
	SetSize(CSize(width, hight));
}



void CDrawScrollView::SetSize(CSize size)
{
    m_size = size;
    SetScrollSizes(MM_TEXT, m_size);
}

IPluginItem* CDrawScrollView::GetPluginItemByPoint(CPoint point)
{
    CPluginTesterDlg* dlg = dynamic_cast<CPluginTesterDlg*>(theApp.m_pMainWnd);
    if (dlg != nullptr)
    {
        PluginInfo plugin_info = dlg->GetCurrentPlugin();
        for (IPluginItem* plugin : plugin_info.plugin_items)
        {
            if (plugin != nullptr)
            {
                CRect rc_item = m_plugin_item_rect[plugin->GetItemId()];
                if (rc_item.PtInRect(point))
                    return plugin;
            }
        }
    }
    return nullptr;
}

BOOL CDrawScrollView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	CRect rect;
	CBrush brush;
	brush.CreateSolidBrush(RGB(235,235,235));
	pDC->GetClipBox(rect);
	pDC->FillRect(rect, &brush);
	return TRUE;

	//return CScrollView::OnEraseBkgnd(pDC);
}


void CDrawScrollView::OnLButtonUp(UINT nFlags, CPoint point)
{
    IPluginItem* plugin_item = GetPluginItemByPoint(point);
    if (plugin_item != nullptr)
    {
        CRect rc_item = m_plugin_item_rect[plugin_item->GetItemId()];
        plugin_item->OnMouseEvent(IPluginItem::MT_LCLICKED, point.x - rc_item.left, point.y - rc_item.top, GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND);
        InvalidateRect(rc_item);
    }
    else
    {
        CScrollView::OnLButtonUp(nFlags, point);
    }
}


void CDrawScrollView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    IPluginItem* plugin_item = GetPluginItemByPoint(point);
    if (plugin_item != nullptr)
    {
        CRect rc_item = m_plugin_item_rect[plugin_item->GetItemId()];
        plugin_item->OnMouseEvent(IPluginItem::MT_DBCLICKED, point.x - rc_item.left, point.y - rc_item.top, GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND);
        InvalidateRect(rc_item);
    }
    else
    {
        CScrollView::OnLButtonDblClk(nFlags, point);
    }
}


void CDrawScrollView::OnRButtonUp(UINT nFlags, CPoint point)
{
    IPluginItem* plugin_item = GetPluginItemByPoint(point);
    if (plugin_item != nullptr)
    {
        CRect rc_item = m_plugin_item_rect[plugin_item->GetItemId()];
        plugin_item->OnMouseEvent(IPluginItem::MT_RCLICKED, point.x - rc_item.left, point.y - rc_item.top, GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND);
        InvalidateRect(rc_item);
    }
    else
    {
        CScrollView::OnRButtonUp(nFlags, point);
    }
}
