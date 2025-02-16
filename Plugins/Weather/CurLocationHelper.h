#pragma once
#include <string>
#include "CityCode.h"

class CCurLocationHelper
{
public:
    CCurLocationHelper();
    ~CCurLocationHelper();

    struct Location
    {
        std::wstring province;
        std::wstring city;
        std::wstring conty;
    };

    static std::wstring GetCurrentCity();
    //将字符串中的xx省xx市xx县/市拆分
    static Location ParseCityName(std::wstring city_string);
    static int FindCityCodeItem(Location location);
    static int FindCityCodeItem(std::wstring city_name);

};

