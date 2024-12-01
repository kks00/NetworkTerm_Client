#pragma once

#include "Global.h"

#define SERVERIPV4  "127.0.0.1"
#define SERVERIPV6  "::1"
#define SERVERPORT  9000

// 메시지 타입 정의
#define MESSAGE_INFO 1100

// TCP전송 메시지 타입 정의
#define SET_USER_NAME 1101
#define UPLOAD_IMAGE 1102
#define USER_LIST_DATA 1103
#define SEND_WHISP 1104
#define REMOVE_ALL 1105
#define REMOVE_WITHOUT_IMG 1106
#define SEND_CHAT 1106
#define RECV_MESSAGE 1107
#define NAME_ALREADY_EXISTS 1108

// UDP전송 메시지 타입 정의
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


// 고정 길이 전송시 사용할 데이터 구조체
struct MessageInfo {
	unsigned int payload_type; // 메시지 타입
	unsigned int payload_length; // 뒤따라올 페이로드의 길이
};

// 채팅 메시지 형식
struct CHAT_MSG
{
	COLORREF color; // 메시지 색상
	char buf[MSGSIZE]; // 메시지 데이터
};

// 선 그리기 메시지 형식
struct DRAWLINE_MSG
{
	int  type;
	int  color;
	int	 width;
	int	 line;
	int  x0, y0;
	int  x1, y1;
};

#define USERNAMESIZE 32 // 사용자 이름 최대길이
// 귓속말 전송 데이터 구조체 정의
struct SEND_WHISP_DATA {
	char sender_id[USERNAMESIZE];
	char message[MSGSIZE];
};


int recvn(SOCKET s, char* buf, int len, int flags);

int send_udp_payload(int message_type, char* payload_buf, int payload_size);
int send_tcp_payload(int message_type, char* payload_buf, int payload_size);

// 오류 출력 함수
void err_quit(char* msg);
void err_display(char* msg);
void SelectPenColor();