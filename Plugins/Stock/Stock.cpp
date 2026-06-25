#include "pch.h"
#include <cmath>

#include "Stock.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "ManagerDialog.h"
#include "Common.h"
#include "FloatingWnd.h"

Stock Stock::m_instance;

Stock::Stock() : m_pFloatingWnd(NULL)
{
	m_items = vector<StockItem>(Stock_ITEM_MAX);
	fill(m_items.begin(), m_items.end(), StockItem());
	for (int index = 0; index < m_items.size(); index++)
	{
		m_items[index].index = index;
	}
}

Stock::~Stock()
{
	DestroyFloatingWnd();
}

Stock& Stock::Instance()
{
	return m_instance;
}

void Stock::OnInitialize(ITrafficMonitor* pApp)
{
	m_pMonitor = pApp;
}

UINT Stock::ThreadCallback(LPVOID dwUser)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CFlagLocker flag_locker(m_instance.m_is_thread_runing);

	if (g_data.m_setting_data.m_stock_codes.empty())
	{
		// CCommon::WriteLog(L"Stock_code not setting!", g_data.m_log_path.c_str());
		g_data.ResetText();
		return 0;
	}

	time_t cur_time = time(nullptr);
	if (cur_time - m_instance.m_last_request_time > 2)
	{
		m_instance.m_last_request_time = cur_time;

		if (g_data.m_setting_data.m_full_day != 1)
		{
			SYSTEMTIME now_time;
			GetLocalTime(&now_time);
			// CCommon::WriteLog(now_time.wHour, g_data.m_log_path.c_str());
			// CCommon::WriteLog(now_time.wMinute, g_data.m_log_path.c_str());
			if (now_time.wHour < 9 || now_time.wHour > 15 || (now_time.wHour == 15 && now_time.wMinute > 30))
			{
				CCommon::WriteLog(L"Not currently in trading time!", g_data.m_log_path.c_str());
				g_data.ResetText();
				return 0;
			}
		}

		// 禁用选项设置中的“更新”按钮
		m_instance.DisableUpdateCommand();

		g_data.RequestRealtimeData();

		// 获取当前时间（历史价格告警需要）
		struct tm now_tm;
		time_t now = time(nullptr);
		localtime_s(&now_tm, &now);

		// 在一个循环中完成所有股票的告警判定
		for (const auto& code : g_data.m_setting_data.m_stock_codes)
		{
			// 检查价格关注条件并弹窗提醒
			m_instance.CheckPriceAlertForStock(code);

			// 检查百分比涨跌幅条件并弹窗提醒
			m_instance.CheckPercentageAlertForStock(code);

			// 检查成本价告警条件并弹窗提醒
			m_instance.CheckCostPriceAlertForStock(code);

			// 检查历史价格告警条件并弹窗提醒
			m_instance.CheckHistoricalPriceAlertForStock(code, now_tm, now);

			// 检查趋势预警条件并弹窗提醒
			//m_instance.CheckTrendAlertForStock(code);

			// 检查T+0买卖点告警
			m_instance.CheckT0AlertForStock(code);
		}

		// 启用选项设置中的"更新"按钮
		m_instance.EnableUpdateCommand();
	}
	return 0;
}

void Stock::LoadContextMenu()
{
	if (m_menu.m_hMenu == NULL)
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		m_menu.LoadMenu(IDR_MENU1);
	}
}

IPluginItem* Stock::GetItem(int index)
{
	size_t item_size = m_items.size();
	if (g_data.m_setting_data.m_stock_codes.size() < item_size)
		item_size = g_data.m_setting_data.m_stock_codes.size();
	if (item_size == 0)
		item_size = 1;
	if (index >= item_size)
		return nullptr;
	return &(m_items[index]);
}

const wchar_t* Stock::GetTooltipInfo()
{
	return L"";
}

void Stock::DataRequired()
{
	static time_t last_req_time{ -1 };
	time_t cur_time = time(nullptr);
	if (cur_time - m_instance.m_last_request_time > 3)
	{
		last_req_time = cur_time;
		SendStockInfoRequest();
	}
	std::lock_guard<std::mutex> lock(m_wndMutex);
	if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
	{
		m_pFloatingWnd->SendMessage(FWND_MSG_REQUEST_DATA, cur_time, 0);
		// DWORD_PTR dwResult = 0;
		// LRESULT lr = ::SendMessageTimeout(
		//     m_pFloatingWnd->GetSafeHwnd(),  // 目标窗口句柄
		//     FWND_MSG_REQUEST_DATA,          // 消息ID
		//     cur_time,                       // wParam
		//     0,                              // lParam
		//     SMTO_ABORTIFHUNG | SMTO_BLOCK,  // 如果窗口挂起则放弃，并阻塞调用线程
		//     2000,                           // 2秒超时
		//     &dwResult);                     // 接收返回值

		// if (lr == 0) // 失败
		//{
		//     DWORD dwErr = GetLastError();
		//     // 处理错误：记录日志或销毁无效窗口等
		//     if (dwErr == ERROR_TIMEOUT)
		//     {
		//         TRACE("SendMessageTimeout timed out\n");
		//     }
		// }
	}
}

ITMPlugin::OptionReturn Stock::ShowOptionsDialog(void* hParent)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CWnd* pParent = CWnd::FromHandle((HWND)hParent);
	if (ShowStockManageDlg(pParent) == IDOK)
	{
		return ITMPlugin::OR_OPTION_CHANGED;
	}
	return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* Stock::GetInfo(PluginInfoIndex index)
{
	switch (index)
	{
	case TMI_NAME:
		return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
	case TMI_DESCRIPTION:
		return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
	case TMI_AUTHOR:
		return L"CListery";
	case TMI_COPYRIGHT:
		return L"Copyright (C) by CListery 2022";
	case ITMPlugin::TMI_URL:
		return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
	case TMI_VERSION:
		return L"1.14";
	default:
		break;
	}
	return L"";
}

void Stock::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
	switch (index)
	{
	case ITMPlugin::EI_CONFIG_DIR:
		// 从配置文件读取配置
		g_data.LoadConfig(std::wstring(data));
		updateItems();
		// 重置价格关注弹窗状态（配置重载后所有弹窗状态清零）
		{
			std::lock_guard<std::mutex> lock(m_instance.m_alert_mutex);
			m_instance.m_last_alert_price.clear();
		}
		// 程序启动后异步预加载所有股票的日K数据
		m_instance.PreloadAllKLineData();
		break;
	case ITMPlugin::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN:
		// 获取TrafficMonitor任务栏窗口中“数值右对齐”设置
		g_data.m_right_align = (_wtoi(data) != 0);
		break;
	default:
		break;
	}
}

int Stock::GetCommandCount()
{
	return 1;
}

const wchar_t* Stock::GetCommandName(int command_index)
{
	switch (command_index)
	{
	case 0:
		return g_data.StringRes(IDS_MENU_UPDATE_STOCK).GetString();
	}
	return nullptr;
}

void Stock::OnPluginCommand(int command_index, void* hWnd, void* para)
{
	switch (command_index)
	{
	case 0:
		SendStockInfoRequest();
		break;
	}
}

void* Stock::GetPluginIcon()
{
	return g_data.GetIcon(IDI_STOCK);
}

void Stock::updateItems()
{
	for (StockItem& item : m_items)
	{
		item.enable = FALSE;
	}
	size_t code_count = g_data.m_setting_data.m_stock_codes.size();
	for (size_t index = 0; index < code_count; index++)
	{
		std::wstring key = g_data.m_setting_data.m_stock_codes[index];
		if (index >= m_items.size())
		{
			m_items.push_back(StockItem());
		}
		m_items[index].enable = TRUE;
		m_items[index].stock_id = key;
	}
}

INT_PTR Stock::ShowStockManageDlg(CWnd* pWnd)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CManagerDialog dlg(pWnd);
	dlg.m_data = g_data.m_setting_data;
	m_option_dlg = &dlg;
	INT_PTR rtn = dlg.DoModal();
	m_option_dlg = nullptr;
	if (rtn == IDOK)
	{
		g_data.m_setting_data = dlg.m_data;
		updateItems();
		g_data.SaveConfig();
	}
	return rtn;
}

void Stock::SendStockInfoRequest()
{
	if (!m_is_thread_runing) // 确保线程已退出
		AfxBeginThread(ThreadCallback, nullptr);
}

void Stock::ShowContextMenu(CWnd* pWnd)
{
	LoadContextMenu();
	CMenu* context_menu = m_menu.GetSubMenu(0);
	if (context_menu != nullptr)
	{
		CPoint point1;
		GetCursorPos(&point1);
		DWORD id = context_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point1.x, point1.y, pWnd);
		// 点击了“管理”
		if (id == ID_OPTIONS)
		{
			ShowStockManageDlg(pWnd);
		}
		// 点击了“更新”
		else if (id == ID_UPDATE)
		{
			SendStockInfoRequest();
		}
	}
}

void Stock::ShowFloatingWnd(void* hWnd, CPoint ptScreen, std::wstring stock_id)
{
	// 如果已有悬浮窗，先销毁
	DestroyFloatingWnd();

	ClientToScreen((HWND)hWnd, &ptScreen);

	CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);

	CFont* font = pWnd->GetParent()->GetFont();

	std::lock_guard<std::mutex> lock(m_wndMutex);
	// 创建新的悬浮窗
	m_pFloatingWnd = new CFloatingWnd;
	if (!m_pFloatingWnd->Create(font, ptScreen, stock_id))
	{
		delete m_pFloatingWnd;
		m_pFloatingWnd = NULL;
	}
}

void Stock::DestroyFloatingWnd()
{
	std::lock_guard<std::mutex> lock(m_wndMutex);
	if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
	{
		m_pFloatingWnd->DestroyWindow();
		delete m_pFloatingWnd;
		m_pFloatingWnd = NULL;
	}
}

void Stock::PreloadAllKLineData()
{
	std::lock_guard<std::mutex> lock(m_preload_mutex);
	if (m_kline_preloaded)
		return;
	m_kline_preloaded = true;

	std::vector<std::wstring> codes = g_data.m_setting_data.m_stock_codes;
	if (codes.empty())
		return;

	auto preloadThread = [](LPVOID pParam) -> UINT {
		std::vector<std::wstring>* pCodes = (std::vector<std::wstring>*)pParam;
		for (const auto& code : *pCodes)
		{
			g_data.RequestKLineData(code, 750);
		}
		delete pCodes;
		return 0;
		};

	std::vector<std::wstring>* pCodes = new std::vector<std::wstring>(codes);
	AfxBeginThread(preloadThread, pCodes);
}

void Stock::UpdateKLine()
{
	// 先检查窗口是否存在，如果不存在直接返回，避免获取未初始化的锁
	// 注：在插件初始化预加载时，m_pFloatingWnd 还未创建，此时不需要更新窗口
	auto* pWnd = m_pFloatingWnd;
	if (pWnd == NULL)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_wndMutex);
	if (m_pFloatingWnd != NULL && ::IsWindow(m_pFloatingWnd->GetSafeHwnd()))
	{
		m_pFloatingWnd->SendMessage((WM_USER + 100), FALSE, 0);
		// DWORD_PTR dwResult = 0;
		// LRESULT lr = ::SendMessageTimeout(
		//     m_pFloatingWnd->GetSafeHwnd(),  // 目标窗口句柄
		//     (WM_USER + 100),          // 消息ID
		//     FALSE,                       // wParam
		//     0,                              // lParam
		//     SMTO_ABORTIFHUNG | SMTO_BLOCK,  // 如果窗口挂起则放弃，并阻塞调用线程
		//     2000,                           // 2秒超时
		//     &dwResult);                     // 接收返回值

		// if (lr == 0) // 失败
		//{
		//     DWORD dwErr = GetLastError();
		//     // 处理错误：记录日志或销毁无效窗口等
		//     if (dwErr == ERROR_TIMEOUT)
		//     {
		//         TRACE("SendMessageTimeout timed out\n");
		//     }
		// }
	}
}

void Stock::DisableUpdateCommand()
{
	// if (m_option_dlg != nullptr)
	//     m_option_dlg->EnableUpdateBtn(false);
	if (m_menu.m_hMenu != NULL)
		m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_GRAYED);
}

void Stock::EnableUpdateCommand()
{
	// if (m_instance.m_option_dlg != nullptr)
	//     m_instance.m_option_dlg->EnableUpdateBtn(true);
	if (m_menu.m_hMenu != NULL)
		m_menu.EnableMenuItem(ID_UPDATE, MF_BYCOMMAND | MF_ENABLED);
}

bool Stock::IsPriceInSafeZone(double current_price, double bid1_price, double ask1_price, double low, double high)
{
	bool is_in_safe_zone = true;

	if (high > 0)
	{
		double safe_high = high * 0.995;
		if ((current_price > 0 && current_price >= safe_high) ||
			(ask1_price > 0 && ask1_price >= safe_high))
		{
			is_in_safe_zone = false;
		}
	}

	if (low > 0)
	{
		double safe_low = low * 1.005;
		if ((current_price > 0 && current_price <= safe_low) ||
			(bid1_price > 0 && bid1_price <= safe_low))
		{
			is_in_safe_zone = false;
		}
	}

	return is_in_safe_zone;
}

void Stock::CheckPriceAlertForStock(const std::wstring& code)
{
	std::lock_guard<std::mutex> lock(m_alert_mutex);

	double low = g_data.GetAlertLowPrice(code);
	double high = g_data.GetAlertHighPrice(code);
	if (low <= 0 && high <= 0)
		return; // 未设置关注价格，跳过

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	double current_price = stock_data->info.currentPrice;
	double bid1_price = stock_data->info.bidLevels[0].price;
	double ask1_price = stock_data->info.askLevels[0].price;

	// 过滤无效价格：当前价格和买一价格都为0时跳过
	if (current_price <= 0 && bid1_price <= 0 && ask1_price <= 0)
		return;

	auto it = m_last_alert_price.find(code);
	double last_price = (it != m_last_alert_price.end()) ? it->second : 0.0;
	bool is_first_alert = (it == m_last_alert_price.end());
	bool show_alert = false;
	CString alert_msg;

	// 先用买一/卖一价格检查告警
	bool bid1_alert_triggered = false;
	if (bid1_price > 0)
	{
		if (low > 0 && bid1_price <= low && (is_first_alert || bid1_price < last_price))
		{
			bid1_alert_triggered = true;
			show_alert = true;
			alert_msg.Format(L"%s (%s):\n%s\n买一价格:%.3f, 关注低价: %.3f",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				g_data.StringRes(IDS_PRICE_ALERT_LOW_MSG).GetString(),
				bid1_price,
				low);
			m_last_alert_price[code] = bid1_price;
		}
		else if (high > 0 && ask1_price >= high && (is_first_alert || ask1_price > last_price))
		{
			bid1_alert_triggered = true;
			show_alert = true;
			alert_msg.Format(L"%s (%s):\n%s\n卖一价格:%.3f, 关注高价: %.3f",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				g_data.StringRes(IDS_PRICE_ALERT_HIGH_MSG).GetString(),
				ask1_price,
				high);
			m_last_alert_price[code] = ask1_price;  // 修复：保存卖一价而不是买一价
		}
	}

	// 买一价格未触发告警，再用当前价格检查
	if (!bid1_alert_triggered && current_price > 0)
	{
		if (low > 0 && current_price <= low && (is_first_alert || current_price < last_price))
		{
			show_alert = true;
			alert_msg.Format(L"%s (%s):\n%s\n当前价格:%g, 关注低价: %g",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				g_data.StringRes(IDS_PRICE_ALERT_LOW_MSG).GetString(),
				current_price,
				low);
			m_last_alert_price[code] = current_price;
		}
		else if (high > 0 && current_price >= high && (is_first_alert || current_price > last_price))
		{
			show_alert = true;
			alert_msg.Format(L"%s (%s):\n%s\n当前价格:%g, 关注高价: %g",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				g_data.StringRes(IDS_PRICE_ALERT_HIGH_MSG).GetString(),
				current_price,
				high);
			m_last_alert_price[code] = current_price;
		}
		else if (!bid1_alert_triggered)
		{
			// 价格回到安全区间才重置状态，避免小幅波动重复触发
			if (IsPriceInSafeZone(current_price, bid1_price, ask1_price, low, high))
			{
				m_last_alert_price.erase(code);
			}
		}
	}
	else
	{
		// 买一/卖一价格触发告警，检查是否回到安全区间
		if (IsPriceInSafeZone(current_price, bid1_price, ask1_price, low, high))
		{
			m_last_alert_price.erase(code);
		}
	}

	if (show_alert && m_pMonitor != nullptr)
	{
		m_pMonitor->ShowNotifyMessage(alert_msg);
	}
}

void Stock::CheckPercentageAlertForStock(const std::wstring& code)
{
	std::lock_guard<std::mutex> lock(m_pct_alert_mutex);

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	double current_price = stock_data->info.currentPrice;
	double prev_close = stock_data->info.prevClosePrice;

	if (current_price <= 0 || prev_close <= 0)
		return;

	CheckPercentageAlerts(current_price, prev_close, code, stock_data->info.displayName);
}

void Stock::CheckCostPriceAlertForStock(const std::wstring& code)
{
	std::lock_guard<std::mutex> lock(m_cost_alert_mutex);

	static const int PCT_STEP = 5;        // 5 * 0.1% = 0.5% 告警阈值
	static const double PCT_UNIT = 0.001; // 0.1% 单位

	double cost_price = g_data.GetCostPrice(code);
	if (cost_price <= 0)
		return;

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	double current_price = stock_data->info.currentPrice;
	if (current_price <= 0)
		return;

	double change_pct = (current_price - cost_price) / cost_price;
	int change_pct_unit = static_cast<int>(std::round(change_pct / PCT_UNIT));

	int& last_alert_level = m_cost_price_alert_levels[code];

	if (std::abs(change_pct_unit) >= PCT_STEP && change_pct > 0)
	{
		int level = change_pct_unit;
		int alert_level = (level / PCT_STEP) * PCT_STEP;
		// 首次告警(last_alert_level==0)或超过上次告警级别才触发
		if (last_alert_level == 0 || alert_level > last_alert_level)
		{
			double pct = alert_level * PCT_UNIT * 100;
			CString alert_msg;
			alert_msg.Format(L"%s (%s):\n收益上涨提醒\n当前价格:%g, 成本:%g,上涨:%g,涨幅:%g%%",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				current_price,
				cost_price,
				current_price - cost_price,
				pct);
			if (m_pMonitor != nullptr)
				m_pMonitor->ShowNotifyMessage(alert_msg);
			last_alert_level = alert_level;
		}
	}
	else if (std::abs(change_pct_unit) >= PCT_STEP && change_pct < 0)
	{
		int level = change_pct_unit;
		int alert_level = (level / PCT_STEP) * PCT_STEP;
		// 首次告警(last_alert_level==0)或低于上次告警级别才触发
		if (last_alert_level == 0 || alert_level < last_alert_level)
		{
			double pct = std::abs(alert_level) * PCT_UNIT * 100;
			CString alert_msg;
			alert_msg.Format(L"%s (%s):\n收益下跌提醒\n当前价格:%g, 成本:%g,下跌:%g, 跌幅:%g%%",
				stock_data->info.displayName.c_str(),
				code.c_str(),
				current_price,
				cost_price,
				current_price - cost_price,
				pct);
			if (m_pMonitor != nullptr)
				m_pMonitor->ShowNotifyMessage(alert_msg);
			last_alert_level = alert_level;
		}
	}
}

void Stock::CheckHistoricalPriceAlertForStock(const std::wstring& code, const struct tm& now_tm, time_t now)
{
	std::lock_guard<std::mutex> lock(m_history_alert_mutex);

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	double current_price = stock_data->info.currentPrice;
	if (current_price <= 0)
		return;

	auto kline_data = stock_data->getKLineData();
	if (!kline_data || kline_data->data.empty())
		return;

	auto& stock_alerts = m_history_alert_status[code];

	struct HistoryPeriodConfig {
		HistoryPeriod period;
		int years;
		const wchar_t* period_name;
		double history_high;
		double history_low;
		bool has_data;
	};

	std::vector<HistoryPeriodConfig> periods = {
		{ HistoryPeriod::YEAR_1, 1, L"近1年", 0.0, 0.0, false },
		{ HistoryPeriod::YEAR_2, 2, L"近2年", 0.0, 0.0, false },
		{ HistoryPeriod::YEAR_3, 3, L"近3年", 0.0, 0.0, false }
	};

	for (auto& config : periods)
	{
		struct tm start_tm = now_tm;
		start_tm.tm_year -= config.years;

		time_t start_time = mktime(&start_tm);

		for (const auto& kline : kline_data->data)
		{
			struct tm kline_tm = {};
			int year, month, day;
			if (sscanf_s(kline.day.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
				continue;

			kline_tm.tm_year = year - 1900;
			kline_tm.tm_mon = month - 1;
			kline_tm.tm_mday = day;
			kline_tm.tm_hour = 0;
			kline_tm.tm_min = 0;
			kline_tm.tm_sec = 0;

			time_t kline_time = mktime(&kline_tm);

			if (kline_time >= start_time && kline_time <= now)
			{
				if (!config.has_data)
				{
					config.history_high = kline.high;
					config.history_low = kline.low;
					config.has_data = true;
				}
				else
				{
					if (kline.high > config.history_high)
						config.history_high = kline.high;
					if (kline.low < config.history_low)
						config.history_low = kline.low;
				}
			}
		}
	}

	for (size_t i = 0; i < periods.size(); ++i)
	{
		auto& config = periods[i];
		if (!config.has_data || config.history_high <= 0 || config.history_low <= 0)
			continue;

		auto& period_alerts = stock_alerts[config.period];

		bool should_alert_high = true;
		if (i > 0)
		{
			for (size_t j = 0; j < i; ++j)
			{
				if (periods[j].has_data && periods[j].history_high > 0 &&
					std::abs(config.history_high - periods[j].history_high) < 0.0001)
				{
					should_alert_high = false;
					break;
				}
			}
		}

		if (current_price >= config.history_high)
		{
			if (period_alerts.find(HistoryAlertType::HIGH) == period_alerts.end() && should_alert_high)
			{
				CString alert_msg;
				alert_msg.Format(L"%s (%s):\n%s历史最高价突破\n当前价格:%g, 历史最高价:%g",
					stock_data->info.displayName.c_str(),
					code.c_str(),
					config.period_name,
					current_price,
					config.history_high);
				if (m_pMonitor != nullptr)
					m_pMonitor->ShowNotifyMessage(alert_msg);
				period_alerts.insert(HistoryAlertType::HIGH);
			}
		}
		else
		{
			period_alerts.erase(HistoryAlertType::HIGH);
		}

		bool should_alert_low = true;
		if (i > 0)
		{
			for (size_t j = 0; j < i; ++j)
			{
				if (periods[j].has_data && periods[j].history_low > 0 &&
					std::abs(config.history_low - periods[j].history_low) < 0.0001)
				{
					should_alert_low = false;
					break;
				}
			}
		}

		if (current_price <= config.history_low)
		{
			if (period_alerts.find(HistoryAlertType::LOW) == period_alerts.end() && should_alert_low)
			{
				CString alert_msg;
				alert_msg.Format(L"%s (%s):\n%s历史最低价突破\n当前价格:%g, 历史最低价:%g",
					stock_data->info.displayName.c_str(),
					code.c_str(),
					config.period_name,
					current_price,
					config.history_low);
				if (m_pMonitor != nullptr)
					m_pMonitor->ShowNotifyMessage(alert_msg);
				period_alerts.insert(HistoryAlertType::LOW);
			}
		}
		else
		{
			period_alerts.erase(HistoryAlertType::LOW);
		}
	}
}

void Stock::CheckPercentageAlerts(double current_price, double prev_close, const std::wstring& code, const std::wstring& display_name)
{
	static const int PCT_STEP = 10;       // 10 * 0.1% = 1% 告警阈值
	static const double PCT_UNIT = 0.001; // 0.1% 单位

	double change_pct = (current_price - prev_close) / prev_close;
	int change_pct_unit = static_cast<int>(std::round(change_pct / PCT_UNIT));

	int& last_alert_level = m_percentage_alert_levels[code];

	if (change_pct > 0)
	{
		// 上涨：检查达到或超过的1%倍数级别
		int level = change_pct_unit;
		if (level >= PCT_STEP)
		{
			// 计算应该告警的级别（向下取到最近的PCT_STEP倍数）
			int alert_level = (level / PCT_STEP) * PCT_STEP;
			// 首次告警(last_alert_level==0)或超过上次告警级别才触发
			if (last_alert_level == 0 || alert_level > last_alert_level)
			{
				// 首次达到这个级别，弹窗告警
				double pct = alert_level * PCT_UNIT * 100;
				CString alert_msg;
				alert_msg.Format(L"%s (%s):\n%s\n当前价格:%g, 上涨:%g,涨幅:%g%%",
					display_name.c_str(),
					code.c_str(),
					g_data.StringRes(IDS_PERCENT_ALERT_RISE_MSG).GetString(),
					current_price,
					current_price - prev_close,
					pct);
				m_pMonitor->ShowNotifyMessage(alert_msg);
				last_alert_level = alert_level;
			}
		}
	}
	else if (change_pct < 0)
	{
		// 下跌：检查达到或超过的1%倍数级别（负数）
		int level = change_pct_unit;
		if (level <= -PCT_STEP)
		{
			// 计算应该告警的级别（向上取到最近的-PCT_STEP倍数）
			int alert_level = (level / PCT_STEP) * PCT_STEP;
			// 首次告警(last_alert_level==0)或低于上次告警级别才触发
			if (last_alert_level == 0 || alert_level < last_alert_level)
			{
				// 首次达到这个级别，弹窗告警
				double pct = std::abs(alert_level) * PCT_UNIT * 100;
				CString alert_msg;
				alert_msg.Format(L"%s (%s):\n%s\n当前价格:%g, 下跌:%g,跌幅:%g%%",
					display_name.c_str(),
					code.c_str(),
					g_data.StringRes(IDS_PERCENT_ALERT_FALL_MSG).GetString(),
					current_price,
					current_price - prev_close,
					pct);
				m_pMonitor->ShowNotifyMessage(alert_msg);
				last_alert_level = alert_level;
			}
		}
	}
}

void Stock::TrendAlertData::add_price(double price)
{
	if (price <= 0)
		return;

	double removed_price = 0.0;
	bool removed_oldest = false;

	if (price_history.size() >= MAX_HISTORY)
	{
		removed_price = price_history[0];
		price_history.erase(price_history.begin());
		removed_oldest = true;
	}

	price_history.push_back(price);

	if (cache_valid)
	{
		if (price > cached_max_price)
			cached_max_price = price;
		if (price < cached_min_price)
			cached_min_price = price;
		if (removed_oldest && (removed_price == cached_max_price || removed_price == cached_min_price))
			cache_valid = false;
	}
	else
	{
		recalculate_cache();
	}
}

void Stock::TrendAlertData::recalculate_cache()
{
	if (price_history.empty())
	{
		cached_max_price = 0.0;
		cached_min_price = 0.0;
		cache_valid = false;
		return;
	}

	cached_max_price = price_history[0];
	cached_min_price = price_history[0];
	for (double price : price_history)
	{
		if (price > cached_max_price) cached_max_price = price;
		if (price < cached_min_price) cached_min_price = price;
	}
	cache_valid = true;
}

double Stock::TrendAlertData::get_max_price() const
{
	if (price_history.empty())
		return 0.0;

	if (cache_valid)
		return cached_max_price;

	double max_price = price_history[0];
	for (double price : price_history)
	{
		if (price > max_price)
			max_price = price;
	}
	return max_price;
}

double Stock::TrendAlertData::get_min_price() const
{
	if (price_history.empty())
		return 0.0;

	if (cache_valid)
		return cached_min_price;

	double min_price = price_history[0];
	for (double price : price_history)
	{
		if (price < min_price)
			min_price = price;
	}
	return min_price;
}

double Stock::TrendAlertData::calculate_amplitude() const
{
	double max_price = get_max_price();
	double min_price = get_min_price();

	if (min_price <= 0)
		return 0.0;

	return (max_price - min_price) / min_price;
}

double Stock::TrendAlertData::get_max_price_excluding_last() const
{
	if (price_history.size() < 2)
		return 0.0;

	double max_price = price_history[0];
	for (size_t i = 0; i < price_history.size() - 1; ++i)
	{
		if (price_history[i] > max_price)
			max_price = price_history[i];
	}
	return max_price;
}

double Stock::TrendAlertData::get_min_price_excluding_last() const
{
	if (price_history.size() < 2)
		return 0.0;

	double min_price = price_history[0];
	for (size_t i = 0; i < price_history.size() - 1; ++i)
	{
		if (price_history[i] < min_price)
			min_price = price_history[i];
	}
	return min_price;
}

double Stock::TrendAlertData::get_max_price_recent(int count) const
{
	if (price_history.empty())
		return 0.0;

	int start = static_cast<int>(price_history.size()) - count;
	if (start < 0)
		start = 0;

	double max_price = price_history[start];
	for (size_t i = start; i < price_history.size(); ++i)
	{
		if (price_history[i] > max_price)
			max_price = price_history[i];
	}
	return max_price;
}

double Stock::TrendAlertData::get_min_price_recent(int count) const
{
	if (price_history.empty())
		return 0.0;

	int start = static_cast<int>(price_history.size()) - count;
	if (start < 0)
		start = 0;

	double min_price = price_history[start];
	for (size_t i = start; i < price_history.size(); ++i)
	{
		if (price_history[i] < min_price)
			min_price = price_history[i];
	}
	return min_price;
}

double Stock::TrendAlertData::get_first_half_average() const
{
	if (price_history.size() < MIN_ANALYSIS_SIZE)
		return 0.0;

	size_t half_size = price_history.size() / 2;
	if (half_size == 0)
		return 0.0;

	double sum = 0.0;
	for (size_t i = 0; i < half_size; ++i)
	{
		sum += price_history[i];
	}
	return sum / half_size;
}

double Stock::TrendAlertData::get_second_half_average() const
{
	if (price_history.size() < MIN_ANALYSIS_SIZE)
		return 0.0;

	size_t half_size = price_history.size() / 2;
	if (half_size == 0)
		return 0.0;

	double sum = 0.0;
	for (size_t i = half_size; i < price_history.size(); ++i)
	{
		sum += price_history[i];
	}
	return sum / (price_history.size() - half_size);
}

bool Stock::TrendAlertData::can_alert(AlertType type) const
{
	auto it = last_alert_time.find(type);
	if (it == last_alert_time.end())
		return true;

	time_t now = time(nullptr);
	return (now - it->second) >= COOLDOWN_SECONDS;
}

void Stock::TrendAlertData::record_alert(AlertType type)
{
	last_alert_time[type] = time(nullptr);
}

double Stock::GetTickValue(const std::wstring& code)
{
	if (code.length() < 2)
		return 0.01;

	std::wstring first2 = code.substr(2, 2);
	const std::vector<std::wstring> etf_prefixes = { L"50", L"51", L"56", L"15", L"16", L"18" };

	for (const auto& prefix : etf_prefixes)
	{
		if (first2 == prefix)
			return 0.001;
	}

	return 0.01;
}

double Stock::CalculateMinorFluctuationThreshold(double price, double tick)
{
	if (price <= 0)
		return 0.001;
	return tick / price;
}

double Stock::CalculateTrendThreshold(double price, double tick)
{
	return CalculateMinorFluctuationThreshold(price, tick) * 3;
}

double Stock::CalculateBreakoutThreshold(double price, double tick)
{
	return CalculateMinorFluctuationThreshold(price, tick) * 2;
}

double Stock::CalculateAmplitudeThreshold(double price, double tick)
{
	return CalculateMinorFluctuationThreshold(price, tick) * 8;
}

bool Stock::IsPriceAtLimit(double current_price, double prev_close, const std::wstring& code)
{
	if (prev_close <= 0 || current_price <= 0)
		return false;

	if (code.find(kHK) == 0 || code.find(kMG) == 0 ||
		code.find(kHF) == 0 || code.find(kNF) == 0)
		return false;

	double change_pct = std::abs(current_price - prev_close) / prev_close;
	double limit = 0.10;

	if (code.find(kBJ) == 0)
	{
		limit = 0.30;
	}
	else if (code.length() > 4)
	{
		std::wstring num_part = code.substr(2, 3);
		if (num_part == L"300" || num_part == L"688")
			limit = 0.20;
	}

	return change_pct >= limit * 0.98;
}

void Stock::CheckTrendAlertForStock(const std::wstring& code)
{
	std::lock_guard<std::mutex> lock(m_trend_mutex);

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	double current_price = stock_data->info.currentPrice;
	double prev_close = stock_data->info.prevClosePrice;

	if (current_price <= 0)
		return;

	if (stock_data->info.volume == 0)
		return;

	if (IsPriceAtLimit(current_price, prev_close, code))
		return;

	double tick = GetTickValue(code);
	double minor_threshold = CalculateMinorFluctuationThreshold(current_price, tick);
	double trend_threshold = CalculateTrendThreshold(current_price, tick);
	double breakout_threshold = CalculateBreakoutThreshold(current_price, tick);
	double amplitude_threshold = CalculateAmplitudeThreshold(current_price, tick);

	auto& trend_data = m_trend_alert_data[code];

	trend_data.add_price(current_price);

	if (trend_data.price_history.size() < TrendAlertData::MIN_ANALYSIS_SIZE)
		return;

	trend_data.consecutive_rising_count = 0;
	trend_data.consecutive_falling_count = 0;
	{
		int current_rise = 0, current_fall = 0;
		for (size_t i = 1; i < trend_data.price_history.size(); ++i)
		{
			double prev = trend_data.price_history[i - 1];
			double curr = trend_data.price_history[i];

			if (curr > prev * (1 + minor_threshold))
			{
				current_rise++;
				current_fall = 0;
			}
			else if (curr < prev * (1 - minor_threshold))
			{
				current_fall++;
				current_rise = 0;
			}
			else
			{
				current_rise = 0;
				current_fall = 0;
			}
		}
		trend_data.consecutive_rising_count = current_rise;
		trend_data.consecutive_falling_count = current_fall;
	}

	double amplitude = trend_data.calculate_amplitude();

	double first_half_avg = trend_data.get_first_half_average();
	double second_half_avg = trend_data.get_second_half_average();

	bool is_center_rise = false;
	bool is_center_fall = false;
	if (first_half_avg > 0 && second_half_avg > 0)
	{
		is_center_rise = (second_half_avg > first_half_avg * (1 + trend_threshold));
		is_center_fall = (second_half_avg < first_half_avg * (1 - trend_threshold));
	}

	bool continuous_rising_break = false;
	if (trend_data.consecutive_rising_count >= TrendAlertData::TREND_CONTINUOUS_COUNT)
	{
		double resistance_level = trend_data.get_max_price_excluding_last();
		if (resistance_level > 0 && current_price > resistance_level * (1 + breakout_threshold))
		{
			continuous_rising_break = true;
		}
	}

	bool continuous_falling_break = false;
	if (trend_data.consecutive_falling_count >= TrendAlertData::TREND_CONTINUOUS_COUNT)
	{
		double support_level = trend_data.get_min_price_excluding_last();
		if (support_level > 0 && current_price < support_level * (1 - breakout_threshold))
		{
			continuous_falling_break = true;
		}
	}

	time_t now = time(nullptr);
	bool can_transition_out = (trend_data.trend_state == TrendState::OSCILLATING) ||
		(now - trend_data.state_enter_time >= TrendAlertData::STATE_HOLD_SECONDS);

	if (trend_data.trend_state == TrendState::OSCILLATING)
	{
		if (amplitude >= amplitude_threshold)
		{
			if (continuous_rising_break || is_center_rise)
			{
				trend_data.trend_state = TrendState::RISING;
				trend_data.state_enter_time = now;
				if (trend_data.can_alert(AlertType::TREND_RISING) && m_pMonitor != nullptr)
				{
					CString alert_msg;
					alert_msg.Format(L"%s (%s):\n上涨趋势形成\n当前价格: %g",
						stock_data->info.displayName.c_str(),
						code.c_str(),
						current_price);
					m_pMonitor->ShowNotifyMessage(alert_msg);
					trend_data.record_alert(AlertType::TREND_RISING);
				}
			}
			else if (continuous_falling_break || is_center_fall)
			{
				trend_data.trend_state = TrendState::FALLING;
				trend_data.state_enter_time = now;
				if (trend_data.can_alert(AlertType::TREND_FALLING) && m_pMonitor != nullptr)
				{
					CString alert_msg;
					alert_msg.Format(L"%s (%s):\n下跌趋势形成\n当前价格: %g",
						stock_data->info.displayName.c_str(),
						code.c_str(),
						current_price);
					m_pMonitor->ShowNotifyMessage(alert_msg);
					trend_data.record_alert(AlertType::TREND_FALLING);
				}
			}
		}
	}
	else if (trend_data.trend_state == TrendState::RISING && can_transition_out)
	{
		if (trend_data.consecutive_falling_count >= TrendAlertData::REVERSE_COUNT)
		{
			double recent_high = trend_data.get_max_price_recent(TrendAlertData::REVERSE_WINDOW);
			if (recent_high > 0 && current_price < recent_high)
			{
				trend_data.trend_state = TrendState::OSCILLATING;
				trend_data.state_enter_time = now;
				trend_data.consecutive_rising_count = 0;
				trend_data.consecutive_falling_count = 0;
				if (trend_data.can_alert(AlertType::REVERSAL_DOWN) && m_pMonitor != nullptr)
				{
					CString alert_msg;
					alert_msg.Format(L"%s (%s):\n上涨转震荡\n当前价格: %g",
						stock_data->info.displayName.c_str(),
						code.c_str(),
						current_price);
					m_pMonitor->ShowNotifyMessage(alert_msg);
					trend_data.record_alert(AlertType::REVERSAL_DOWN);
				}
			}
		}
	}
	else if (trend_data.trend_state == TrendState::FALLING && can_transition_out)
	{
		if (trend_data.consecutive_rising_count >= TrendAlertData::REVERSE_COUNT)
		{
			double recent_low = trend_data.get_min_price_recent(TrendAlertData::REVERSE_WINDOW);
			if (recent_low > 0 && current_price > recent_low)
			{
				trend_data.trend_state = TrendState::OSCILLATING;
				trend_data.state_enter_time = now;
				trend_data.consecutive_rising_count = 0;
				trend_data.consecutive_falling_count = 0;
				if (trend_data.can_alert(AlertType::REVERSAL_UP) && m_pMonitor != nullptr)
				{
					CString alert_msg;
					alert_msg.Format(L"%s (%s):\n下跌转震荡\n当前价格: %g",
						stock_data->info.displayName.c_str(),
						code.c_str(),
						current_price);
					m_pMonitor->ShowNotifyMessage(alert_msg);
					trend_data.record_alert(AlertType::REVERSAL_UP);
				}
			}
		}
	}
}

ITMPlugin* TMPluginGetInstance()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return &Stock::Instance();
}

// ========== T+0买卖点告警辅助函数 ==========

namespace T0Helper {
	struct MACDPoint {
		double dif;
		double dea;
		double bar;
		bool valid;
	};

	struct T0Signal {
		bool valid;
		bool isBuy;
		int strength;
		CString reason;
		double price;
		CString time;
	};

	// 计算分时MACD（通达信初始化方式）
	std::vector<MACDPoint> CalculateMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint)
	{
		std::vector<MACDPoint> result;
		int n = static_cast<int>(timelinePoint.size());
		if (n == 0)
			return result;

		result.resize(n);

		std::vector<double> closes(n);
		for (int i = 0; i < n; i++)
			closes[i] = timelinePoint[i].price;

		std::vector<double> ema12(n);
		ema12[0] = closes[0];
		for (int i = 1; i < n; i++)
			ema12[i] = closes[i] * 2.0 / 13.0 + ema12[i - 1] * 11.0 / 13.0;

		std::vector<double> ema26(n);
		ema26[0] = closes[0];
		for (int i = 1; i < n; i++)
			ema26[i] = closes[i] * 2.0 / 27.0 + ema26[i - 1] * 25.0 / 27.0;

		std::vector<double> dif(n);
		for (int i = 0; i < n; i++)
			dif[i] = ema12[i] - ema26[i];

		std::vector<double> dea(n);
		dea[0] = dif[0];
		for (int i = 1; i < n; i++)
			dea[i] = dif[i] * 2.0 / 11.0 + dea[i - 1] * 9.0 / 11.0;

		for (int i = 0; i < n; i++)
		{
			result[i].dif = dif[i];
			result[i].dea = dea[i];
			result[i].bar = (dif[i] - dea[i]) * 2.0;
			result[i].valid = true;
		}

		return result;
	}

	// 检测买点
	T0Signal DetectBuySignal(const std::vector<STOCK::TimelinePoint>& tp, const std::vector<MACDPoint>& macd)
	{
		T0Signal sig;
		sig.valid = false;
		sig.isBuy = true;
		sig.strength = 0;
		sig.price = 0;

		int n = static_cast<int>(tp.size());
		int m = static_cast<int>(macd.size());
		if (n < 10 || m < 10)
			return sig;

		int last = n - 1;
		sig.price = tp[last].price;
		sig.time = CString(tp[last].time.c_str());

		// ========== 量能基准：前三根成交量均值 ==========
		long long volAvg3 = 0;
		{
			int volCount = min(3, last);
			for (int i = last - volCount; i < last; i++)
				volAvg3 += tp[i].volume;
			if (volCount > 0) volAvg3 /= volCount;
		}
		bool isShrinking = (volAvg3 > 0 && tp[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
		bool isExpanding = (volAvg3 > 0 && tp[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

		int strength = 0;
		CString reasons;

		// 避坑：放量大跌不抄底
		{
			int lb = min(5, n - 1);
			bool heavyDecline = true;
			for (int i = last - lb + 1; i <= last; i++)
			{
				if (tp[i].price >= tp[i - 1].price) { heavyDecline = false; break; }
			}
			if (heavyDecline)
			{
				bool volUp = true;
				for (int i = last - lb + 2; i <= last; i++)
				{
					if (tp[i].volume < tp[i - 1].volume) { volUp = false; break; }
				}
				if (volUp) return sig;
			}
		}

		// 避坑：无量阴跌
		{
			int lb = min(10, n - 1);
			bool decline = true;
			for (int i = last - lb + 1; i <= last; i++)
			{
				if (tp[i].price >= tp[i - 1].price) { decline = false; break; }
			}
			bool lowVol = isShrinking;
			bool macdDown = macd[last].valid && macd[last].dif < 0 && macd[last].bar < 0;
			if (decline && lowVol && macdDown) return sig;
	}

	// 量柱信号（需量能确认）
		{
			int lb = min(10, n - 1);
			int greenCnt = 0;
			bool shrinking = true;
			long long prevGVol = 0;
			for (int i = last - lb + 1; i <= last; i++)
			{
				if (tp[i].price < tp[i - 1].price)
				{
					greenCnt++;
					if (prevGVol > 0 && tp[i].volume > prevGVol) shrinking = false;
					prevGVol = tp[i].volume;
				}
			}
			bool firstRed = (tp[last].price > tp[last - 1].price) && greenCnt >= 3;
			if (shrinking && greenCnt >= 3 && isShrinking) { strength++; reasons += _T("缩量下跌抛压耗尽 "); }
			// 首根红量柱需要放量确认，否则可能是假反弹
			if (firstRed && isExpanding) { strength++; reasons += _T("放量红柱资金承接 "); }
		}

		// MACD信号
		if (macd[last].valid)
		{
			if (macd[last].bar < 0)
			{
				int sc = 0;
				for (int i = last; i > max(0, last - 5); i--)
				{
					if (macd[i].valid && macd[i].bar < 0 && std::abs(macd[i].bar) <= std::abs(macd[i - 1].bar)) sc++;
				}
				if (sc >= 3) { strength++; reasons += _T("MACD绿柱缩短 "); }
			}
			// 底背离
			int lb = min(30, n - 1);
			int lo1 = -1, lo2 = -1;
			for (int i = last - lb + 1; i < last; i++)
			{
				if (i > 0 && i < n - 1 && tp[i].price < tp[i - 1].price && tp[i].price < tp[i + 1].price)
				{
					if (lo1 < 0) lo1 = i;
					else if (i > lo1 + 3 && lo2 < 0) lo2 = i;
				}
			}
			if (lo1 >= 0 && lo2 >= 0 && tp[lo2].price < tp[lo1].price &&
				macd[lo2].valid && macd[lo1].valid && macd[lo2].dif > macd[lo1].dif)
			{
				strength += 2; reasons += _T("MACD底背离 ");
			}
		}

		// 股价支撑（需量能确认）
		{
			double avgP = tp[last].averagePrice;
			double p = tp[last].price;
			// 回踩均价支撑：需要缩量回踩才有效
			if (avgP > 0 && std::abs(p - avgP) / avgP < 0.003 && isShrinking) { strength++; reasons += _T("缩量回踩均价支撑 "); }
			// 分时底部企稳：需要放量止跌才可靠
			if (last >= 2 && tp[last].price >= tp[last - 1].price && tp[last - 1].price < tp[last - 2].price && isExpanding)
			{
				strength++; reasons += _T("放量底部企稳 ");
			}
		}

		sig.strength = min(3, strength);
		sig.reason = reasons;
		sig.valid = (strength >= 2);
		return sig;
	}

	// 检测卖点
	T0Signal DetectSellSignal(const std::vector<STOCK::TimelinePoint>& tp, const std::vector<MACDPoint>& macd)
	{
		T0Signal sig;
		sig.valid = false;
		sig.isBuy = false;
		sig.strength = 0;
		sig.price = 0;

		int n = static_cast<int>(tp.size());
		int m = static_cast<int>(macd.size());
		if (n < 10 || m < 10)
			return sig;

		int last = n - 1;
		sig.price = tp[last].price;
		sig.time = CString(tp[last].time.c_str());

		// ========== 量能基准：前三根成交量均值 ==========
		long long volAvg3 = 0;
		{
			int volCount = min(3, last);
			for (int i = last - volCount; i < last; i++)
				volAvg3 += tp[i].volume;
			if (volCount > 0) volAvg3 /= volCount;
		}
		bool isShrinking = (volAvg3 > 0 && tp[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
		bool isExpanding = (volAvg3 > 0 && tp[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

		int strength = 0;
		CString reasons;

		// 避坑：放量大涨不卖出
		{
			int lb = min(5, n - 1);
			bool strongRise = true;
			for (int i = last - lb + 1; i <= last; i++)
			{
				if (tp[i].price <= tp[i - 1].price) { strongRise = false; break; }
			}
			if (strongRise)
			{
				bool volUp = true;
				for (int i = last - lb + 2; i <= last; i++)
				{
					if (tp[i].volume < tp[i - 1].volume) { volUp = false; break; }
				}
				if (volUp) return sig;
		}
	}

	// 量柱信号（需量能确认）
		{
			int lb = min(10, n - 1);
			// 检查近期是否有放量冲高（使用前三根均值判定放量）
			bool hadSpike = false;
			int spikeIdx = -1;
			for (int i = last - lb + 1; i <= last; i++)
			{
				long long localAvg3 = 0;
				int vc = min(3, i);
				for (int j = i - vc; j < i; j++)
					localAvg3 += tp[j].volume;
				if (vc > 0) localAvg3 /= vc;
				if (localAvg3 > 0 && tp[i].volume > localAvg3 * 3 / 2) { hadSpike = true; spikeIdx = i; break; }
			}
			// 放量后量能萎缩，且当前价格未明显回落（仍在高位）
			bool priceStillHigh = (hadSpike && spikeIdx >= 0 &&
				tp[last].price >= tp[spikeIdx].price * 0.995);
			bool shrinkingNow = false;
			if (hadSpike && last > spikeIdx + 1 && tp[last].volume < tp[last - 1].volume && isShrinking && priceStillHigh)
				shrinkingNow = true;

			if (hadSpike && shrinkingNow) { strength++; reasons += _T("放量冲高后缩量回落 "); }
		}

		// MACD信号
		if (macd[last].valid)
		{
			if (macd[last].bar > 0)
			{
				int sc = 0;
				for (int i = last; i > max(0, last - 5); i--)
				{
					if (macd[i].valid && macd[i].bar > 0 && macd[i].bar <= macd[i - 1].bar) sc++;
				}
				if (sc >= 3) { strength++; reasons += _T("MACD红柱缩短 "); }
			}
			// 死叉
			if (last >= 1 && macd[last].valid && macd[last - 1].valid)
			{
				if (macd[last - 1].dif > macd[last - 1].dea && macd[last].dif < macd[last].dea)
				{
					strength++; reasons += _T("MACD死叉 ");
				}
			}
			// 顶背离
			int lb = min(30, n - 1);
			int hi1 = -1, hi2 = -1;
			for (int i = last - lb + 1; i < last; i++)
			{
				if (i > 0 && i < n - 1 && tp[i].price > tp[i - 1].price && tp[i].price > tp[i + 1].price)
				{
					if (hi1 < 0) hi1 = i;
					else if (i > hi1 + 3 && hi2 < 0) hi2 = i;
				}
			}
			if (hi1 >= 0 && hi2 >= 0 && tp[hi2].price > tp[hi1].price &&
				macd[hi2].valid && macd[hi1].valid && macd[hi2].dif < macd[hi1].dif)
			{
				strength += 2; reasons += _T("MACD顶背离 ");
			}
		}

		// 股价压力（需量能确认）
		{
			double avgP = tp[last].averagePrice;
			double p = tp[last].price;
			// 遇均价压力：需要放量冲高遇阻才有效
			if (avgP > 0 && p > avgP && std::abs(p - avgP) / avgP < 0.005 && isExpanding) { strength++; reasons += _T("放量遇均价压力 "); }
			// 冲高滞涨：放量冲高后缩量回落
			if (last >= 2 && tp[last].price <= tp[last - 1].price && tp[last - 1].price > tp[last - 2].price && isShrinking)
			{
				strength++; reasons += _T("冲高缩量滞涨 ");
			}
		}

		sig.strength = min(3, strength);
		sig.reason = reasons;
		sig.valid = (strength >= 2);
		return sig;
	}
} // namespace T0Helper

void Stock::CheckT0AlertForStock(const std::wstring& code)
{
	std::lock_guard<std::mutex> lock(m_t0_alert_mutex);

	auto stock_data = g_data.GetStockData(code);
	if (!stock_data || !stock_data->info.is_ok)
		return;

	// 获取分时数据
	auto* timelineData = stock_data->getTimelineData();
	if (!timelineData || timelineData->data.size() < 10)
		return;

	const auto& tp = timelineData->data;

	// 计算MACD
	auto macd = T0Helper::CalculateMACD(tp);

	// 检测买卖信号
	auto buySig = T0Helper::DetectBuySignal(tp, macd);
	auto sellSig = T0Helper::DetectSellSignal(tp, macd);

	time_t now = time(nullptr);
	auto& state = m_t0_alert_state[code];

	// 买点告警
	if (buySig.valid && !state.last_buy_signal)
	{
		if (now - state.last_buy_alert_time >= T0AlertState::COOLDOWN_SECONDS)
		{
			CString alert_msg;
			CString strengthStr;
			switch (buySig.strength)
			{
			case 3: strengthStr = _T("强"); break;
			case 2: strengthStr = _T("中"); break;
			default: strengthStr = _T("弱"); break;
			}
			alert_msg.Format(_T("%s (%s):\n【T+0买入信号】%s\n当前价格: %.3f\n信号原因: %s"),
				stock_data->info.displayName.c_str(),
				code.c_str(),
				strengthStr.GetString(),
				buySig.price,
				buySig.reason.GetString());

			if (m_pMonitor != nullptr)
				m_pMonitor->ShowNotifyMessage(alert_msg);

			state.last_buy_alert_time = now;
		}
	}
	state.last_buy_signal = buySig.valid;

	// 卖点告警
	if (sellSig.valid && !state.last_sell_signal)
	{
		if (now - state.last_sell_alert_time >= T0AlertState::COOLDOWN_SECONDS)
		{
			CString alert_msg;
			CString strengthStr;
			switch (sellSig.strength)
			{
			case 3: strengthStr = _T("强"); break;
			case 2: strengthStr = _T("中"); break;
			default: strengthStr = _T("弱"); break;
			}
			alert_msg.Format(_T("%s (%s):\n【T+0卖出信号】%s\n当前价格: %.3f\n信号原因: %s"),
				stock_data->info.displayName.c_str(),
				code.c_str(),
				strengthStr.GetString(),
				sellSig.price,
				sellSig.reason.GetString());

			if (m_pMonitor != nullptr)
				m_pMonitor->ShowNotifyMessage(alert_msg);

			state.last_sell_alert_time = now;
		}
	}
	state.last_sell_signal = sellSig.valid;
}