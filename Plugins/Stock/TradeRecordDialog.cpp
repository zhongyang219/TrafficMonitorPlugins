#include "pch.h"
#include "TradeRecordDialog.h"
#include "Common.h"
#include "DataManager.h"

IMPLEMENT_DYNAMIC(CTradeRecordDialog, CDialogEx)

CTradeRecordDialog::CTradeRecordDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TRADE_RECORD_DIALOG, pParent)
	, m_time(_T(""))
	, m_price(0.0)
	, m_stockCode(_T(""))
	, m_tradeType(0)
	, m_amount(_T(""))
	, m_fee(0.0)
	, m_total(0.0)
{
}

CTradeRecordDialog::~CTradeRecordDialog()
{
}

void CTradeRecordDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TRADE_TYPE_BUY, m_btnBuy);
	DDX_Control(pDX, IDC_TRADE_TYPE_SELL, m_btnSell);
	DDX_Control(pDX, IDC_TRADE_TIME_EDIT, m_editTime);
	DDX_Control(pDX, IDC_TRADE_PRICE_EDIT, m_editPrice);
	DDX_Control(pDX, IDC_TRADE_AMOUNT_EDIT, m_editAmount);
	DDX_Control(pDX, IDC_TRADE_AMOUNT_STATIC, m_staticAmount);
	DDX_Control(pDX, IDC_TRADE_FEE_STATIC, m_staticFee);
	DDX_Control(pDX, IDC_TRADE_TOTAL_STATIC, m_staticTotal);
}

void CTradeRecordDialog::SetTradeInfo(const CString& time, double price, const CString& stockCode, const CString& stockName)
{
	m_time = time;
	m_price = price;
	m_stockCode = stockCode;
	m_stockName = stockName;
}

BOOL CTradeRecordDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_btnBuy.SetCheck(1);
	m_tradeType = 0;

	m_editTime.SetWindowText(m_time);
	m_editPrice.SetWindowText(CCommon::FormatFloat(m_price));

	UpdateCalculations();

	return TRUE;
}

void CTradeRecordDialog::UpdateCalculations()
{
	double amount = 0.0;
	if (!m_amount.IsEmpty())
	{
		amount = _ttof(m_amount);
	}

	double totalAmount = m_price * amount;

	const double FEE_RATE = 0.00005;
	m_fee = totalAmount * FEE_RATE;
	if (m_fee < 0.5 && m_fee > 0)
	{
		m_fee = 0.5;
	}

	if (m_tradeType == 0)
	{
		m_total = -(totalAmount + m_fee);
	}
	else
	{
		m_total = totalAmount - m_fee;
	}

	CString amountStr;
	amountStr.Format(_T("%.2f"), totalAmount);
	m_staticAmount.SetWindowText(amountStr);

	CString feeStr;
	feeStr.Format(_T("%.2f"), m_fee);
	m_staticFee.SetWindowText(feeStr);

	CString totalStr;
	totalStr.Format(_T("%.2f"), m_total);
	m_staticTotal.SetWindowText(totalStr);
}

BEGIN_MESSAGE_MAP(CTradeRecordDialog, CDialogEx)
	ON_BN_CLICKED(IDC_TRADE_TYPE_BUY, &CTradeRecordDialog::OnBnClickedTradeTypeBuy)
	ON_BN_CLICKED(IDC_TRADE_TYPE_SELL, &CTradeRecordDialog::OnBnClickedTradeTypeSell)
	ON_EN_CHANGE(IDC_TRADE_AMOUNT_EDIT, &CTradeRecordDialog::OnEnChangeTradeAmountEdit)
	ON_BN_CLICKED(IDOK, &CTradeRecordDialog::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CTradeRecordDialog::OnBnClickedCancel)
END_MESSAGE_MAP()

void CTradeRecordDialog::OnBnClickedTradeTypeBuy()
{
	m_tradeType = 0;
	m_btnBuy.SetCheck(1);
	m_btnSell.SetCheck(0);
	UpdateCalculations();
}

void CTradeRecordDialog::OnBnClickedTradeTypeSell()
{
	m_tradeType = 1;
	m_btnBuy.SetCheck(0);
	m_btnSell.SetCheck(1);
	UpdateCalculations();
}

void CTradeRecordDialog::OnEnChangeTradeAmountEdit()
{
	m_editAmount.GetWindowText(m_amount);
	UpdateCalculations();
}

void CTradeRecordDialog::OnBnClickedOk()
{
	m_editAmount.GetWindowText(m_amount);

	double amount = _ttof(m_amount);
	if (amount <= 0)
	{
		AfxMessageBox(_T("请输入有效的数量"));
		return;
	}

	CString timeStr;
	m_editTime.GetWindowText(timeStr);
	CString priceStr;
	m_editPrice.GetWindowText(priceStr);

	// 构造完整日期时间: yyyy-MM-dd HH:mm:ss
	CTime now = CTime::GetCurrentTime();
	CString fullTime;
	fullTime.Format(_T("%04d-%02d-%02d %s"), now.GetYear(), now.GetMonth(), now.GetDay(), (LPCTSTR)timeStr);

	double totalAmount = m_price * amount;

	// 保存到sqlite数据库
	bool ok = g_data.SaveTradeRecord(
		(LPCWSTR)m_stockCode,
		(LPCWSTR)m_stockName,
		m_tradeType,
		(LPCWSTR)fullTime,
		m_price,
		amount,
		totalAmount,
		m_fee,
		m_total
	);

	if (!ok)
	{
		AfxMessageBox(_T("保存交易记录失败"));
		return;
	}

	CDialogEx::OnOK();
}

void CTradeRecordDialog::OnBnClickedCancel()
{
	CDialogEx::OnCancel();
}