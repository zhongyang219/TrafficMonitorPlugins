
#ifndef _BASE64UTILS_H_
#define _BASE64UTILS_H_

#include <stdint.h>
#include <string>

namespace utilities {

std::string Base64Encode(const std::string str_in);

std::string Base64Decode(const std::string str_in);

//ÅĞ¶ÏÊÇ·ñÎªbase64±àÂë
bool IsBase64Code(const std::string str);

}

#endif // #define _BASE64UTILS_H_
