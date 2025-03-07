#include <string>
#include "base64.h"
#include <algorithm>
#include <cassert>
#include <set>

namespace utilities {

static const char base64EncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char base64DecodeTable[256] = {
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\x3e','\xff','\xff','\xff','\x3f',
    '\x34','\x35','\x36','\x37','\x38','\x39','\x3a','\x3b','\x3c','\x3d','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\x00','\x01','\x02','\x03','\x04','\x05','\x06','\x07','\x08','\x09','\x0a','\x0b','\x0c','\x0d','\x0e',
    '\x0f','\x10','\x11','\x12','\x13','\x14','\x15','\x16','\x17','\x18','\x19','\xff','\xff','\xff','\xff','\xff',
    '\xff','\x1a','\x1b','\x1c','\x1d','\x1e','\x1f','\x20','\x21','\x22','\x23','\x24','\x25','\x26','\x27','\x28',
    '\x29','\x2a','\x2b','\x2c','\x2d','\x2e','\x2f','\x30','\x31','\x32','\x33','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
    '\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff','\xff',
};

//3个字节转换成4个字节
static std::string ThreeBytesToFourBytes(const char* bytes, int len)
{
    std::string res;
    res.resize(4);
    res[0] = base64EncodeTable[(bytes[0] >> 2) & 0x3f];
    if (len == 3)
    {
        res[1] = base64EncodeTable[((bytes[0] << 4) & 0x30) | ((bytes[1] >> 4) & 0x0f)];
        res[2] = base64EncodeTable[((bytes[1] << 2) & 0x3c) | ((bytes[2] >> 6) & 0x03)];
        res[3] = base64EncodeTable[bytes[2] & 0x3f];
    }
    else if (len == 2)
    {
        res[1] = base64EncodeTable[((bytes[0] << 4) & 0x30) | ((bytes[1] >> 4) & 0x0f)];
        res[2] = base64EncodeTable[(bytes[1] << 2) & 0x3c];
        res[3] = '=';
    }
    else
    {
        res[1] = base64EncodeTable[(bytes[0] << 4) & 0x30];
        res[2] = '=';
        res[3] = '=';
    }
    return res;
}

static std::string FourBytesToThreeBytes(const char* bytes, int len)
{
    int equalNum = 0;
    if (bytes[len - 1] == '=')
        equalNum++;
    if (bytes[len - 2] == '=')
        equalNum++;
    len -= equalNum;
    std::string res;
    res.resize(3, '\0');
    res[0] = (base64DecodeTable[bytes[0]] << 2) | (base64DecodeTable[bytes[1]] >> 4);
    if (len > 2)
    {
        res[1] = ((base64DecodeTable[bytes[1]] << 4) & 0xf0) | (base64DecodeTable[bytes[2]] >> 2);
        if (len > 3)
        {
            res[2] = ((base64DecodeTable[bytes[2]] << 6) & 0xc0) | (base64DecodeTable[bytes[3]]);
        }
    }
    return res;
}

std::string Base64Encode(const std::string str_in)
{
    std::string res;
    for (size_t i = 0; i < str_in.size(); i += 3)
    {
        //取3个字节并转换成4个字节
        int bytes_count = std::min(3, static_cast<int>(str_in.size() - i));
        std::string fourBytes = ThreeBytesToFourBytes(str_in.data() + i, bytes_count);
        res.append(fourBytes);
    }
    return res;
}

std::string Base64Decode(const std::string str_in)
{
    std::string res;
    for (size_t i = 0; i < str_in.size(); i += 4)
    {
        int bytes_count = std::min(4, static_cast<int>(str_in.size() - i));
        std::string threeBytes = FourBytesToThreeBytes(str_in.data() + i, bytes_count);
        res.append(threeBytes);
    }
    return res;
}


bool IsBase64Code(const std::string str, size_t max_length)
{
    std::set<char> base64EncodeSet;
    int len = _countof(base64DecodeTable);
    for (int i = 0; i < len; i++)
        base64EncodeSet.insert(base64EncodeTable[i]);

    for (size_t i = 0; i < str.size() && i <= max_length; i++)
    {
        if (i == str.size() - 1 && str[i] == '=')		//遍历到最后一个字符是'='，返回true
            return true;
        if (i == str.size() - 2 && str[i] == '=' && str[i + 1] == '=')	//遍历到最倒数第2个字符，后面两个字符都是'='，返回true
            return true;
        if (base64EncodeSet.find(str[i]) == base64EncodeSet.end())		//如果有一个字符不在base64编码表中，则返回false
            return false;
    }
    return true;
}

}
