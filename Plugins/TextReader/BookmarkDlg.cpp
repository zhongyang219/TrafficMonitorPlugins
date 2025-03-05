// BookmarkDlg.cpp: 实现文件
//

#include "pch.h"
#include "TextReader.h"
#include "afxdialogex.h"
#include "BookmarkDlg.h"
#include "DataManager.h"

// CBookmarkDlg 对话框

IMPLEMENT_DYNAMIC(CBookmarkDlg, CDialogEx)

CBookmarkDlg::CBookmarkDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_BOOKMARK_DLG, pParent)
{

}

CBookmarkDlg::~CBookmarkDlg()
{
}


void CBookmarkDlg::AdjustColumeWidth()
{
    CRect rect;
    m_list_ctrl.GetClientRect(rect);
    int width0, width1, width2;
    width0 = rect.Width() / 5;
    width2 = rect.Width() / 3;
    width1 = rect.Width() - width0 - width2 - g_data.DPI(20) - 1;
    m_list_ctrl.SetColumnWidth(0, width0);
    m_list_ctrl.SetColumnWidth(1, width1);
    m_list_ctrl.SetColumnWidth(2, width2);

}

int CBookmarkDlg::GetSelectedPosition() const
{
    return m_selected_position;
}

void CBookmarkDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list_ctrl);
}


BEGIN_MESSAGE_MAP(CBookmarkDlg, CDialogEx)
END_MESSAGE_MAP()


// CBookmarkDlg 消息处理程序


BOOL CBookmarkDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetBackgroundColor(RGB(255, 255, 255));

    //初始化列表控件
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    m_list_ctrl.InsertColumn(0, g_data.StringRes(IDS_BOOKMARK_POSITION), LVCFMT_LEFT, 100);
    m_list_ctrl.InsertColumn(1, g_data.StringRes(IDS_BOOKMARK_CONTENTS), LVCFMT_LEFT, 100);
    m_list_ctrl.InsertColumn(2, g_data.StringRes(IDS_CHAPTER), LVCFMT_LEFT, 100);

    //显示列表内容
    int index{};
    for (const auto& bookmark_pos : g_data.GetBookmark())
    {
        //书签位置的百分比
        wchar_t buff[32];
        swprintf_s(buff, 16, L"%.2f%%", static_cast<double>(bookmark_pos) * 100 / g_data.GetTextContexts().size());
        m_list_ctrl.InsertItem(index, buff);
        m_list_ctrl.SetItemData(index, bookmark_pos);
        //书签内容
        std::wstring bookmark_contents;
        if (bookmark_pos < g_data.GetTextContexts().size())
        {
            bookmark_contents = g_data.GetTextContexts().substr(bookmark_pos, 32);
        }
        m_list_ctrl.SetItemText(index, 1, bookmark_contents.c_str());
        //章节
        std::wstring chapter{ g_data.GetChapter().GetChapterByPos(bookmark_pos) };
        m_list_ctrl.SetItemText(index, 2, chapter.c_str());
        index++;
    }

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CBookmarkDlg::OnOK()
{
    // TODO: 在此添加专用代码和/或调用基类
    int cur_selection{ m_list_ctrl.GetSelectionMark() };
    if (cur_selection >= 0 && cur_selection < m_list_ctrl.GetItemCount())
    {
        m_selected_position = static_cast<int>(m_list_ctrl.GetItemData(cur_selection));
    }

    CDialogEx::OnOK();
}
