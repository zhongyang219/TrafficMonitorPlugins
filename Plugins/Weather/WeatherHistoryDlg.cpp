// WeatherHistoryDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "afxdialogex.h"
#include "WeatherHistoryDlg.h"
#include "DataManager.h"
#include <strsafe.h>
#include "../utilities/Common.h"
#include "Common.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CHistoryWeatherListCtrl, CListCtrl)
CHistoryWeatherListCtrl::CHistoryWeatherListCtrl()
{
}

CHistoryWeatherListCtrl::~CHistoryWeatherListCtrl()
{
}

BEGIN_MESSAGE_MAP(CHistoryWeatherListCtrl, CListCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CHistoryWeatherListCtrl::OnNMCustomdraw)
END_MESSAGE_MAP()

static void SetDrawRect(CDC* pDC, CRect rect)
{
    CRgn rgn;
    rgn.CreateRectRgnIndirect(rect);
    pDC->SelectClipRgn(&rgn);
}


void CHistoryWeatherListCtrl::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = CDRF_DODEFAULT;
    LPNMLVCUSTOMDRAW lplvdr = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    NMCUSTOMDRAW& nmcd = lplvdr->nmcd;
    switch (lplvdr->nmcd.dwDrawStage)	//判断状态   
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT:
        if (nmcd.dwItemSpec >= 0 && nmcd.dwItemSpec < GetItemCount())
        {
            CDC* pDC = CDC::FromHandle(nmcd.hdc);		//获取绘图DC
            CRect item_rect, draw_rect;
            GetSubItemRect(nmcd.dwItemSpec, CHistoryWeatherListCtrl::COL_WEATHER, LVIR_BOUNDS, item_rect);	//获取绘图单元格的矩形区域

            //设置绘图区域
            SetDrawRect(pDC, item_rect);

            //设置颜色
            COLORREF text_color = GetSysColor(COLOR_BACKGROUND);
            COLORREF back_color = GetSysColor(COLOR_WINDOW);

            bool selected = false;
            CWnd* focus_wnd = GetFocus();
            //如果是行中行且有焦点
            if (focus_wnd == this && GetItemState(nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED)
            {
                text_color = GetSysColor(COLOR_WINDOW);
                back_color = GetSysColor(COLOR_HIGHLIGHT);
                selected = true;
            }
            else
            {
                //获取今天日期
                SYSTEMTIME cur_time{};
                GetLocalTime(&cur_time);
                wchar_t buff[256]{};
                StringCchPrintfW(buff, 256, L"%.4d/%.2d/%.2d", cur_time.wYear, cur_time.wMonth, cur_time.wDay);
                //从列表“日期”列获取日期
                std::wstring date_str = GetItemText(nmcd.dwItemSpec, CHistoryWeatherListCtrl::COL_DATE).GetString();
                std::vector<std::wstring> date_str_vec;
                utilities::StringHelper::StringSplit(date_str, L' ', date_str_vec, false);
                std::wstring date;
                if (!date_str_vec.empty())
                    date = date_str_vec[0];
                //如果是过去的天气
                if (date < buff)
                {
                    text_color = GetSysColor(COLOR_GRAYTEXT);
                }
                //今天的天气
                else if (date == buff)
                {
                    text_color = GetSysColor(COLOR_HIGHLIGHT);
                }
            }
            lplvdr->clrText = text_color;

            //填充背景
            draw_rect = item_rect;
            pDC->FillSolidRect(draw_rect, back_color);

            //从列表“天气”列的文本获取天气类型
            std::wstring weather_text = GetItemText(nmcd.dwItemSpec, CHistoryWeatherListCtrl::COL_WEATHER).GetString();
            std::vector<std::wstring> weather_text_vec;
            utilities::StringHelper::StringSplit(weather_text, L' ', weather_text_vec, false);
            std::wstring weather_type;
            if (!weather_text_vec.empty())
                weather_type = weather_text_vec[0];
            //绘制天气图标
            const int icon_size{ g_data.DPI(16) };
            HICON hIcon = g_data.GetWeatherIcon(weather_type);
            CPoint icon_point{ draw_rect.TopLeft() };
            icon_point.x = draw_rect.left + g_data.DPI(2);
            icon_point.y = draw_rect.top + (draw_rect.Height() - icon_size) / 2;
            if (!selected)
            {
                //填充图标背景，防止白色背景看不清图标
                CRect icon_rect{ draw_rect };
                icon_rect.right = icon_rect.left + icon_size + g_data.DPI(4);
                pDC->FillSolidRect(icon_rect, RGB(185, 209, 234));
            }
            ::DrawIconEx(pDC->GetSafeHdc(), icon_point.x, icon_point.y, hIcon, icon_size, icon_size, 0, NULL, DI_NORMAL);
            //绘制天气文本
            CRect rc_text{ draw_rect };
            rc_text.left += (icon_size + g_data.DPI(8));
            pDC->SetTextColor(text_color);
            pDC->SetBkMode(TRANSPARENT);
            pDC->DrawText(weather_text.c_str(), rc_text, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            //当前列绘制完成后将绘图区域设置为左边的区域，防止当前列的区域被覆盖
            CRect rect1{ item_rect };
            rect1.left = 0;
            rect1.right = item_rect.left;
            SetDrawRect(pDC, rect1);
        }
        *pResult = CDRF_DODEFAULT;
        break;
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// CWeatherHistoryDlg 对话框

IMPLEMENT_DYNAMIC(CWeatherHistoryDlg, CDialog)

CWeatherHistoryDlg::CWeatherHistoryDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_WEATHER_HISTORY_DIALOG, pParent)
{

}

CWeatherHistoryDlg::~CWeatherHistoryDlg()
{
}

void CWeatherHistoryDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list_ctrl);
}


BEGIN_MESSAGE_MAP(CWeatherHistoryDlg, CDialog)
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()


// CWeatherHistoryDlg 消息处理程序

BOOL CWeatherHistoryDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(g_data.GetIcon(IDI_WEATHER), FALSE);
    SetWindowText(g_data.StringRes(IDS_WEATHER_HISTORY));

    //获取初始时窗口的大小
    CRect rect;
    GetWindowRect(rect);
    m_min_size.cx = rect.Width();
    m_min_size.cy = rect.Height();

    //初始化列表控件
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    m_list_ctrl.GetClientRect(rect);
    int width0 = g_data.DPI(120);
    int width1 = rect.Width() - width0 - g_data.DPI(20) - 1;
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    m_list_ctrl.InsertColumn(CHistoryWeatherListCtrl::COL_DATE, g_data.StringRes(IDS_DATE), LVCFMT_LEFT, width0);
    m_list_ctrl.InsertColumn(CHistoryWeatherListCtrl::COL_WEATHER, g_data.StringRes(IDS_WEATHER), LVCFMT_LEFT, width1);

    //填充数据
    const auto& history_weather{ g_data.HistoryWeatherMgr().GetHistoryWeather() };
    for (auto iter = history_weather.rbegin(); iter != history_weather.rend(); ++iter)
    {
        wchar_t buff[256]{};
        //日期
        StringCchPrintfW(buff, 256, L"%.4d/%.2d/%.2d (", iter->first.year, iter->first.month, iter->first.day);
        std::wstring date_str = buff;
        int week_day = CCommon::CaculateWeekDay(iter->first.year, iter->first.month, iter->first.day);
        switch (week_day)
        {
        case 0: date_str += g_data.StringRes(IDS_SUNDAY); break;
        case 1: date_str += g_data.StringRes(IDS_MONDAY); break;
        case 2: date_str += g_data.StringRes(IDS_TUESDAY); break;
        case 3: date_str += g_data.StringRes(IDS_WEDNESDAY); break;
        case 4: date_str += g_data.StringRes(IDS_THURSDAY); break;
        case 5: date_str += g_data.StringRes(IDS_FRIDAY); break;
        case 6: date_str += g_data.StringRes(IDS_SATURDAY); break;
        default: break;
        }
        date_str += L')';
        int index = m_list_ctrl.GetItemCount();
        m_list_ctrl.InsertItem(index, date_str.c_str());

        //天气
        StringCchPrintfW(buff, 256, L"%s %d°C~%d°C %s", iter->second.type.GetString(), iter->second.low_temp, iter->second.high_temp, iter->second.wind.GetString());
        m_list_ctrl.SetItemText(index, CHistoryWeatherListCtrl::COL_WEATHER, buff);
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

void CWeatherHistoryDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    //限制窗口最小大小
    lpMMI->ptMinTrackSize.x = m_min_size.cx;		//设置最小宽度
    lpMMI->ptMinTrackSize.y = m_min_size.cy;		//设置最小高度

    CDialog::OnGetMinMaxInfo(lpMMI);
}
