#include "Socket_Utils.h"

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// UDP 데이터 전송 함수
int send_udp_payload(int message_type, char* payload_buf, int payload_size) {
	MessageInfo message_info;
	message_info.payload_type = message_type;
	message_info.payload_length = payload_size;

	// 먼저 고정길이(8바이트)의 메세지 정보(타입, 페이로드 길이)를 전송
	int retval = sendto(g_sock_udp, (char*)&message_info, sizeof(message_info), 0, (sockaddr*)&g_serveraddr, sizeof(g_serveraddr));
	if (retval == SOCKET_ERROR) {
		err_display("[send_udp_payload] send messageinfo");
		return SOCKET_ERROR;
	}
	printf("[%s] sent messageinfo: %d\n", __func__, retval);

	// 페이로드 전송
	retval = sendto(g_sock_udp, payload_buf, message_info.payload_length, 0, (sockaddr*)&g_serveraddr, sizeof(g_serveraddr));
	if (retval == SOCKET_ERROR) {
		err_display("[send_udp_payload] send payload");
		return SOCKET_ERROR;
	}
	printf("[%s] sent payload: %d\n", __func__, retval);

	return retval;
}

// TCP 데이터 전송 함수
int send_tcp_payload(int message_type, char* payload_buf, int payload_size) {
	MessageInfo message_info;
	message_info.payload_length = payload_size;
	message_info.payload_type = message_type;

	// 먼저 고정길이(8바이트)의 메세지 정보(타입, 페이로드 길이)를 전송
	int retval = send(g_sock_tcp, (char*)&message_info, sizeof(message_info), 0);
	if (retval == SOCKET_ERROR) {
		err_display("[send_tcp_payload] send messageinfo");
		return SOCKET_ERROR;
	}
	printf("[%s] sent messageinfo: %d\n", __func__, retval);

	// 페이로드 전송
	retval = send(g_sock_tcp, payload_buf, message_info.payload_length, 0);
	if (retval == SOCKET_ERROR) {
		err_display("[send_tcp_payload] send payload");
		return SOCKET_ERROR;
	}
	printf("[%s] sent payload: %d\n", __func__, retval);

	return retval;
}

// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}