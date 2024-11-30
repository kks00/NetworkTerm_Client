#include "Global.h"

// 바이트 배열을 입력받아 16진수로 1바이트씩 띄워서 문자열로 반환하는 함수
std::string byteArrayToHexString(const unsigned char* byteArray, size_t length) {
    const char* hexChars = "0123456789ABCDEF";
    std::string result;

    for (size_t i = 0; i < length; ++i) {
        if (i > 0) {
            result += " "; // 1바이트 간 공백 추가
        }
        result += hexChars[(byteArray[i] >> 4) & 0x0F]; // 상위 4비트
        result += hexChars[byteArray[i] & 0x0F];       // 하위 4비트
    }

    return result;
}