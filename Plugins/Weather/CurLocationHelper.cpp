#include "pch.h"
#include "CurLocationHelper.h"
#include "Common.h"
#include "DataManager.h"

CCurLocationHelper::CCurLocationHelper()
{
}

CCurLocationHelper::~CCurLocationHelper()
{
}

std::wstring CCurLocationHelper::GetCurrentCity()
{
    std::string city_data;
    //获取城市信息
    if (CCommon::GetURL(L"https://whois.pconline.com.cn/ip.jsp", city_data))
    {
        return CCommon::StrToUnicode(city_data.c_str(), false);
    }
    return std::wstring();
}


int CCurLocationHelper::FindCityCodeItem(std::wstring city_name)
{
    if (!city_name.empty())
    {
        for (size_t i{}; i < g_data.CityList().size(); i++)
        {
            const auto& city_info{ g_data.CityList()[i] };
            //解析城市列表中城市的省和市
            std::wstring cur_province;
            std::wstring cur_city;
            size_t index = city_info.name.find(L' ');
            if (index == std::wstring::npos)
                continue;
            cur_province = city_info.name.substr(0, index);
            cur_city = city_info.name.substr(index + 1);


            if (city_name.find(cur_province) != std::wstring::npos && city_name.find(cur_city) != std::wstring::npos)
                return static_cast<int>(i);
        }
    }
    return -1;
}
