// StockDetailDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "afxdialogex.h"
#include "StockDetailDlg.h"
#include "DataManager.h"
#include "Common.h"

IMPLEMENT_DYNAMIC(CStockDetailDlg, CDialog)

CStockDetailDlg::CStockDetailDlg(const std::wstring &stock_id, CWnd *pParent /*=nullptr*/)
    : CDialog(IDD_STOCK_DETAIL_DIALOG, pParent), m_stock_id(stock_id)
{
}

CStockDetailDlg::~CStockDetailDlg()
{
}

void CStockDetailDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DETAIL_LIST, m_list);
}

BEGIN_MESSAGE_MAP(CStockDetailDlg, CDialog)
ON_WM_DESTROY()
ON_WM_TIMER()
ON_BN_CLICKED(IDC_REFRESH_BTN, &CStockDetailDlg::OnBnClickedRefresh)
END_MESSAGE_MAP()

// CStockDetailDlg 消息处理程序

BOOL CStockDetailDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    HICON hIcon = g_data.GetIcon(IDI_STOCK);
    SetIcon(hIcon, FALSE);

    // 标题显示股票代码
    CString title;
    title.Format(_T("%s"), m_stock_id.c_str());
    SetWindowText(title);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, g_data.StringRes(IDS_COL_ITEM), LVCFMT_LEFT, 90);
    m_list.InsertColumn(1, g_data.StringRes(IDS_COL_VALUE), LVCFMT_LEFT, 200);

    RefreshList();
    SetTimer(TIMER_REFRESH, 3000, nullptr);
    return TRUE;
}

void CStockDetailDlg::OnDestroy()
{
    KillTimer(TIMER_REFRESH);
    CDialog::OnDestroy();
}

void CStockDetailDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_REFRESH)
        RefreshList();
    CDialog::OnTimer(nIDEvent);
}

void CStockDetailDlg::OnBnClickedRefresh()
{
    Stock::Instance().SendStockInfoRequest();
    RefreshList();
}

static void AddRow(CListCtrl &list, int &row, const CString &name, const CString &value)
{
    list.InsertItem(row, name);
    list.SetItemText(row, 1, value);
    row++;
}

static CString FormatPrice(double v)
{
    CString s;
    s.Format(_T("%.2f"), v);
    return s;
}

static CString FormatVolume(long long v)
{
    CString s;
    if (v >= 100000000)
        s.Format(_T("%.2f亿股"), v / 100000000.0);
    else if (v >= 10000)
        s.Format(_T("%.2f万股"), v / 10000.0);
    else
        s.Format(_T("%lld股"), v);
    return s;
}

static CString FormatAmount(double v)
{
    CString s;
    if (v >= 100000000)
        s.Format(_T("%.2f亿"), v / 100000000.0);
    else if (v >= 10000)
        s.Format(_T("%.2f万"), v / 10000.0);
    else
        s.Format(_T("%.2f"), v);
    return s;
}

void CStockDetailDlg::RefreshList()
{
    m_list.DeleteAllItems();

    auto pData = g_data.GetStockData(m_stock_id);
    if (!pData || !pData->info.is_ok)
    {
        int row = 0;
        AddRow(m_list, row, g_data.StringRes(IDS_COL_NAME), CString(m_stock_id.c_str()));
        AddRow(m_list, row, _T(""), g_data.StringRes(IDS_LOAD_FAIL));
        return;
    }

    const auto &rt = pData->realTimeData;
    int row = 0;
    AddRow(m_list, row, g_data.StringRes(IDS_COL_NAME), CString(pData->info.displayName.c_str()));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_PRICE), CString(rt.displayPrice.c_str()));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_PERCENT), CString(rt.displayFluctuation.c_str()));

    if (rt.currentPrice > 0 && rt.prevClosePrice > 0)
    {
        CString diff;
        diff.Format(_T("%+.2f"), rt.currentPrice - rt.prevClosePrice);
        AddRow(m_list, row, g_data.StringRes(IDS_COL_CHANGE), diff);
    }
    AddRow(m_list, row, g_data.StringRes(IDS_COL_PREVCLOSE), FormatPrice(rt.prevClosePrice));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_OPEN), FormatPrice(rt.openPrice));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_HIGH), FormatPrice(rt.highPrice));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_LOW), FormatPrice(rt.lowPrice));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_VOLUME), FormatVolume(rt.volume));
    AddRow(m_list, row, g_data.StringRes(IDS_COL_TURNOVER), FormatAmount(rt.turnover));

    // 5 档买卖盘
    for (int i = 4; i >= 0; i--)
    {
        CString name;
        name.Format(_T("卖%d"), 5 - i);
        CString val;
        val.Format(_T("%.2f  %lld"), rt.askLevels[i].price, rt.askLevels[i].volume);
        AddRow(m_list, row, name, val);
    }
    for (int i = 0; i < 5; i++)
    {
        CString name;
        name.Format(_T("买%d"), i + 1);
        CString val;
        val.Format(_T("%.2f  %lld"), rt.bidLevels[i].price, rt.bidLevels[i].volume);
        AddRow(m_list, row, name, val);
    }
}
