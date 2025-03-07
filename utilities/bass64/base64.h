
#ifndef _BASE64UTILS_H_
#define _BASE64UTILS_H_

#include <stdint.h>
#include <string>

namespace utilities {

std::string Base64Encode(const std::string str_in);

std::string Base64Decode(const std::string str_in);

//判断是否为base64编码
//max_length：判断文本的最大长度
bool IsBase64Code(const std::string str, size_t max_length = static_cast<size_t>(-1));

}

#endif // #define _BASE64UTILS_H_
