#include "Variant.h"

namespace utilities
{
	CVariant::CVariant(int value)
	{
		m_value_int = value;
		m_type = eType::INT;
	}

	CVariant::CVariant(size_t value)
	{
		m_value_int = static_cast<int>(value);
		m_type = eType::UINT;
	}

	CVariant::CVariant(double value)
	{
		m_value_double = value;
		m_type = eType::DOUBLE;
	}

	CVariant::CVariant(const wchar_t* value)
	{
		m_value_string = value;
		m_type = eType::STRING;
	}

	CVariant::CVariant(const std::wstring& value)
	{
		m_value_string = value.c_str();
		m_type = eType::STRING;
	}

	CVariant::~CVariant()
	{
	}

	std::wstring CVariant::ToString() const
	{
		std::wstring str;
		switch (m_type)
		{
		case CVariant::eType::INT:
			str = std::to_wstring(m_value_int);
			break;
		case eType::UINT:
			str = std::to_wstring(static_cast<unsigned int>(m_value_int));
			break;
		case CVariant::eType::DOUBLE:
			str = std::to_wstring(m_value_double);
			break;
		case CVariant::eType::STRING:
			str = m_value_string;
			break;
		default:
			break;
		}
		return str;
	}
}
