#pragma once
#include "CommonData.h"

// DrawScrollView 视图
class CDrawScrollView : public CScrollView
{
    DECLARE_DYNCREATE(CDrawScrollView)

protected:
    CDrawScrollView();           // 动态创建所使用的受保护的构造函数
    virtual ~CDrawScrollView();
#ifdef _DEBUG
    virtual void AssertValid() const;
#ifndef _WIN32_WCE
    virtual void Dump(CDumpContext& dc) const;
#endif
#endif

//成员函数
public:
    void InitialUpdate();
    void SetSize(int width,int hight);
    void SetSize(CSize size);
    void PluginChanged();
    CSize GetSize() const;
    void UpdateMouseToolTip();

private:
    IPluginItem* GetPluginItemByPoint(CPoint point);

//成员变量
protected:
    CSize m_size;
    std::map<std::wstring, CRect> m_plugin_item_rect;   //保存每个插件项目的显示位置
    IPluginItem* m_plugin_item_clicked{};
    CToolTipCtrl m_tool_tips;

protected:
    virtual void OnDraw(CDC* pDC);      // 重写以绘制该视图
    virtual void OnInitialUpdate();     // 构造后的第一次

    DECLARE_MESSAGE_MAP()
public:
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};
