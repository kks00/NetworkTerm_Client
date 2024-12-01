#pragma once

 // 디버깅 콘솔 생성여부
 // #define DEBUG_MODE
// #define LOG_PACKET_RAW

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
#include <time.h>

#include <commctrl.h> // ListView 관련 정의
#include <richedit.h>

#include <fstream>

#include <string>
using namespace std;

#include "Socket_Utils.h"

extern SOCKET        g_sock_tcp; // 클라이언트 TCP 소켓
extern SOCKET        g_sock_udp; // 클라이언트 UDP 소켓
extern SOCKADDR_IN g_serveraddr;

std::string byteArrayToHexString(const char* byteArray, size_t length);