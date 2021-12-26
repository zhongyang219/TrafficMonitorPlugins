// SelectCityDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "SelectCityDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "CityCode.h"


// CSelectCityDlg 对话框

IMPLEMENT_DYNAMIC(CSelectCityDlg, CDialog)

CSelectCityDlg::CSelectCityDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_SELECT_CITY_DIALOG, pParent)
{

}

CSelectCityDlg::~CSelectCityDlg()
{
}

int CSelectCityDlg::GetCityIndex() const
{
    if (m_searched)
    {
        if (m_index >= 0 && m_index < static_cast<int>(m_search_result.size()))
        {
            return m_search_result[m_index];
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return m_index;
    }
}

void CSelectCityDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CITY_LIST, m_list_ctrl);
    DDX_Control(pDX, IDC_SEARCH_EDIT, m_search_edit);
}

void CSelectCityDlg::ShowList()
{
    m_list_ctrl.DeleteAllItems();
    if (m_searched)
    {
        int i{};
        for (int index : m_search_result)
        {
            if (index >= 0 && index < static_cast<int>(CityCode.size()))
            {
                const CityCodeItem& item{ CityCode[index] };
                m_list_ctrl.InsertItem(i, item.name.c_str());
                m_list_ctrl.SetItemText(i, 1, item.code.c_str());
                i++;
            }
        }
    }
    else
    {
        int i{};
        for (const auto& item : CityCode)
        {
            m_list_ctrl.InsertItem(i, item.name.c_str());
            m_list_ctrl.SetItemText(i, 1, item.code.c_str());
            i++;
        }
    }
}

void CSelectCityDlg::QuickSearch(const std::wstring& key_word)
{
    m_search_result.clear();
    for (int i = 0; i < static_cast<int>(CityCode.size()); i++)
    {
        const auto& city = CityCode[i];
        if (city.name.find(key_word) != std::wstring::npos)
        {
            m_search_result.push_back(i);
        }
    }
}


BEGIN_MESSAGE_MAP(CSelectCityDlg, CDialog)
    ON_NOTIFY(NM_CLICK, IDC_CITY_LIST, &CSelectCityDlg::OnNMClickCityList)
    ON_NOTIFY(NM_RCLICK, IDC_CITY_LIST, &CSelectCityDlg::OnNMRClickCityList)
    ON_EN_CHANGE(IDC_SEARCH_EDIT, &CSelectCityDlg::OnEnChangeSearchEdit)
END_MESSAGE_MAP()


// CSelectCityDlg 消息处理程序


BOOL CSelectCityDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetIcon(g_data.GetIcon(IDI_WEATHER), FALSE);
    CRect rect;
    m_list_ctrl.GetClientRect(rect);
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
    int width0, width1;
    width0 = rect.Width() / 4;
    width1 = rect.Width() - width0 - g_data.DPI(20) - 1;
    m_list_ctrl.InsertColumn(0, g_data.StringRes(IDS_CITY), LVCFMT_LEFT, width0);
    m_list_ctrl.InsertColumn(1, g_data.StringRes(IDS_CITY_CODE), LVCFMT_LEFT, width1);

    m_search_edit.SetCueBanner(g_data.StringRes(IDS_INPUT_KEY_WORD));
    ShowList();

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CSelectCityDlg::OnNMClickCityList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    m_index = pNMItemActivate->iItem;
    *pResult = 0;
}


void CSelectCityDlg::OnNMRClickCityList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    m_index = pNMItemActivate->iItem;
    *pResult = 0;
}


void CSelectCityDlg::OnEnChangeSearchEdit()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CDialog::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码
    CString str;
    GetDlgItemText(IDC_SEARCH_EDIT, str);
    QuickSearch(str.GetString());
    m_searched = !str.IsEmpty();
    ShowList();
}
