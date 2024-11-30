#pragma once

 // 디버깅 콘솔 생성여부
 #define DEBUG_MODE

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Comctl32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"
#include <tchar.h>
#include<math.h>
#include <math.h>

#include <commctrl.h> // ListView 관련 정의

#include <string>
using namespace std;

#include "Socket_Utils.h"

std::string byteArrayToHexString(const unsigned char* byteArray, size_t length);