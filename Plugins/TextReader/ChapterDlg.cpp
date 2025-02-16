// ChapterDlg.cpp: 实现文件
//

#include "pch.h"
#include "TextReader.h"
#include "ChapterDlg.h"
#include "DataManager.h"


// CChapterDlg 对话框

IMPLEMENT_DYNAMIC(CChapterDlg, CDialogEx)

CChapterDlg::CChapterDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_CHAPTER_DIALOG, pParent)
{

}

CChapterDlg::~CChapterDlg()
{
}

int CChapterDlg::GetSelectedPosition() const
{
    return m_selected_position;
}

void CChapterDlg::ShowData()
{
    //计算当前进度的章节
    int cur_chapter_position = g_data.GetChapter().GetChapterIndexByPos(g_data.m_setting_data.current_position);
    if (cur_chapter_position >= 0)
    {
        std::wstring chapter_title = g_data.GetChapter().GetChapterTitle(cur_chapter_position);
        SetDlgItemText(IDC_CHRRENT_CHAPTER_STATIC, chapter_title.c_str());

    }

    //显示列表内容
    m_chapter_index.clear();
    m_lst_box.ResetContent();
    int index{};
    int selected_index{};
    for (const auto& item : g_data.GetChapter().GetChapterData())
    {
        m_chapter_index.push_back(item.first);
        m_lst_box.AddString(item.second.c_str());
        if (cur_chapter_position == item.first)
        {
            selected_index = index;
        }
        index++;
    }
    m_lst_box.SetCurSel(selected_index);
}

void CChapterDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_lst_box);
}


BEGIN_MESSAGE_MAP(CChapterDlg, CDialogEx)
    //ON_BN_CLICKED(IDC_RE_PARSE_BUTTON, &CChapterDlg::OnBnClickedReParseButton)
    ON_LBN_SELCHANGE(IDC_LIST1, &CChapterDlg::OnLbnSelchangeList1)
END_MESSAGE_MAP()


// CChapterDlg 消息处理程序


BOOL CChapterDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetBackgroundColor(RGB(255, 255, 255));
    
    ShowData();


    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CChapterDlg::OnOK()
{
    // TODO: 在此添加专用代码和/或调用基类
    m_selected_position = -1;
    if (m_selected_changed && IsWindow(m_lst_box.m_hWnd))
    {
        int index = m_lst_box.GetCurSel();
        if (index >= 0 && index <= m_chapter_index.size())
            m_selected_position = m_chapter_index[index];
    }

    CDialogEx::OnOK();
}


//void CChapterDlg::OnBnClickedReParseButton()
//{
//    // TODO: 在此添加控件通知处理程序代码
//    g_data.GetChapter().Parse();
//    ShowData();
//}


void CChapterDlg::OnLbnSelchangeList1()
{
    // TODO: 在此添加控件通知处理程序代码
    m_selected_changed = true;
}
