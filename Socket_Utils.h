#pragma once

#include "Global.h"

#define SERVERIPV4  "127.0.0.1"
#define SERVERIPV6  "::1"
#define SERVERPORT  9000

struct MessageInfo {
	unsigned int payload_type;
	unsigned int payload_length;
};

// 메세지 타입 정의
#define MESSAGE_INFO 1100
#define SET_USER_NAME 1101

#define CHATTING			  1000          // 메시지 타입: 채팅
#define DRAW_LINE             1001			// 메시지 타입: 선
#define DRAW_STRAIGHTLINE     1002			// 메시지 타입: 직선
#define DRAW_ELLIPSE          1003			// 메시지 타입: 타원
#define DRAW_RECTANGLE        1004			// 메시지 타입: 사각형
#define DRAW_TRIANGLE         1005 			// 메시지 타입: 삼각형
#define DRAW_RIGHTTRIANGLE    1006 			// 메시지 타입: 직각 삼각형
#define DRAW_STAR             1007 			// 메시지 타입: 별
#define DRAW_PARALLELOGRAM    1008 			// 메시지 타입: 평행사변형
#define DRAW_DIAMOND          1009			// 메시지 타입: 마름모
#define DRAW_ARROW            1010			// 메시지 타입: 화살표
#define DRAW_ERASER           1011			// 메시지 타입: 지우개


#define BUFSIZE     256                    // 전송 메시지 전체 크기
#define MSGSIZE     (BUFSIZE-sizeof(int))  // 채팅 메시지 최대 길이

// 채팅 메시지 형식
struct CHAT_MSG
{
	char buf[MSGSIZE];
};

// 선 그리기 메시지 형식
// sizeof(DRAWLINE_MSG) == 256
struct DRAWLINE_MSG
{
	int  type;
	int  color;
	int	 width;
	int	 line;
	int  x0, y0;
	int  x1, y1;
};

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags);
// 오류 출력 함수
void err_quit(char* msg);
void err_display(char* msg);
void SelectPenColor();