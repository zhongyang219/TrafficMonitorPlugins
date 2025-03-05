#include "pch.h"
#include "CurLocationHelper.h"
#include "Common.h"
//#include "utilities/yyjson/yyjson.h"

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
    if (CCommon::GetURL(L"http://whois.pconline.com.cn/ip.jsp", city_data))
    {
        ////截取返回结果中花括号内的内容
        //size_t index1 = city_data.find(L'{');
        //size_t index2 = city_data.rfind(L'}');
        //if (index1 != std::wstring::npos && index2 != std::wstring::npos)
        //    city_data = city_data.substr(index1, index2 - index1 + 1);

        ////解析json内容
        //yyjson_doc* doc = yyjson_read(city_data.c_str(), city_data.size(), 0);
        //if (doc != nullptr)
        //{
        //    //获取Json根节点
        //    yyjson_val* root = yyjson_doc_get_root(doc);

        //    //获取城市
        //    yyjson_val* city = yyjson_obj_get(root, "cname");
        //    std::wstring str_city = CCommon::StrToUnicode(yyjson_get_str(city), true);

        //    return str_city;
        //}
        return CCommon::StrToUnicode(city_data.c_str(), false);
    }
    return std::wstring();
}

CCurLocationHelper::Location CCurLocationHelper::ParseCityName(std::wstring city_string)
{
    CCurLocationHelper::Location result;
    //解析省
    size_t index1 = city_string.find(L'省');
    if (index1 != std::wstring::npos)
    {
        result.province = city_string.substr(0, index1 + 1);
        city_string = city_string.substr(index1 + 1);
    }
    else
    {
        index1 = city_string.find(L"自治区");
        if (index1 != std::wstring::npos)
        {
            result.province = city_string.substr(0, index1 + 3);
            city_string = city_string.substr(index1 + 3);
        }
    }
    //解析市
    size_t index2 = city_string.find(L'市');
    if (index2 != std::wstring::npos)
    {
        result.city = city_string.substr(0, index2 + 1);
        city_string = city_string.substr(index2 + 1);
    }
    //解析县
    size_t index3 = city_string.find(L'县');
    if (index3 != std::wstring::npos)
    {
        result.conty = city_string.substr(0, index3 + 1);
    }
    else
    {
        index3 = city_string.find(L'市');
        if (index3 != std::wstring::npos)
        {
            result.conty = city_string.substr(0, index3 + 1);
        }
    }

    return result;
}

int CCurLocationHelper::FindCityCodeItem(Location location)
{
    int result;
    result = FindCityCodeItem(location.conty);
    if (result < 0)
        result = FindCityCodeItem(location.city);
    return result;
}

int CCurLocationHelper::FindCityCodeItem(std::wstring city_name)
{
    if (!city_name.empty())
    {
        auto iter = std::find_if(CityCode.begin(), CityCode.end(), [&](const CityCodeItem& city_code_item)
            {
                return city_code_item.name == city_name;
            });
        if (iter == CityCode.end())
        {
            city_name.pop_back();
            iter = std::find_if(CityCode.begin(), CityCode.end(), [&](const CityCodeItem& city_code_item)
                {
                    return city_code_item.name == city_name;
                });
        }
        return static_cast<int>(iter - CityCode.begin());
    }
    return -1;
}
