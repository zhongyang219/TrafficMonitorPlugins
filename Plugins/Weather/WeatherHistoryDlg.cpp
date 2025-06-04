// WeatherHistoryDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "afxdialogex.h"
#include "WeatherHistoryDlg.h"
#include "DataManager.h"
#include <strsafe.h>

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
END_MESSAGE_MAP()


// CWeatherHistoryDlg 消息处理程序

BOOL CWeatherHistoryDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	//初始化列表控件
	CRect rect;
	m_list_ctrl.GetClientRect(rect);
	int width0 = g_data.DPI(100);
	int width1 = rect.Width() - width0 - g_data.DPI(20) - 1;
	m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
	m_list_ctrl.InsertColumn(0, g_data.StringRes(IDS_DATE), LVCFMT_LEFT, width0);
	m_list_ctrl.InsertColumn(1, g_data.StringRes(IDS_WEATHER), LVCFMT_LEFT, width1);

	//填充数据
	const auto& history_weather{ g_data.HistoryWeatherMgr().GetHistoryWeather() };
	for (auto iter = history_weather.rbegin(); iter != history_weather.rend(); ++iter)
	{
		wchar_t buff[256]{};
		//日期
		StringCchPrintfW(buff, 256, L"%.4d/%.2d/%.2d", iter->first.year, iter->first.month, iter->first.day);
		int index = m_list_ctrl.GetItemCount();
		m_list_ctrl.InsertItem(index, buff);

		//天气
		StringCchPrintfW(buff, 256, L"%s %d°C~%d°C %s", iter->second.type.GetString(), iter->second.low_temp, iter->second.high_temp, iter->second.wind.GetString());
		m_list_ctrl.SetItemText(index, 1, buff);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
