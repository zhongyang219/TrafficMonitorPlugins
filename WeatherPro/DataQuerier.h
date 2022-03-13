#pragma once

#include "pch.h"
#include <vector>

struct SCityInfo
{
    CString CityNO;
    CString CityName;
    CString CityAdministrativeOwnership;
};

using CityInfoList = std::vector<SCityInfo>;

namespace query
{
    CityInfoList QueryCityInfo(const CString &q_name);
}
