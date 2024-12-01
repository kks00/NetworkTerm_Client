#include "Global.h"

#define WM_DRAWIT   (WM_USER+1)            // 사용자 정의 윈도우 메시지

SOCKET        g_sock_tcp; // 클라이언트 TCP 소켓
SOCKET        g_sock_udp; // 클라이언트 UDP 소켓
SOCKADDR_IN g_serveraddr;
static char g_username[USERNAMESIZE];

static HINSTANCE     g_hInst; // 응용 프로그램 인스턴스 핸들
static HWND          g_hDrawWnd; // 그림을 그릴 윈도우
static HWND          g_hButtonSendMsg; // '메시지 전송' 버튼
static HWND          g_hEditStatus; // 받은 메시지 출력
static char          g_ipaddr[64]; // 서버 IP 주소
static u_short       g_port; // 서버 포트 번호
static BOOL          g_isIPv6; // IPv4 or IPv6 주소?
static HANDLE        g_hClientThread; // 스레드 핸들
static volatile BOOL g_bStart; // 통신 시작 여부
static HANDLE        g_hReadEvent, g_hWriteEvent; // 이벤트 핸들
static HANDLE        g_hUserList; // 접속중인 사용자 리스트
static HANDLE        g_hWhispText; // 귓속말 텍스트

static CHAT_MSG       g_chatmsg; // 채팅 메시지 저장
static DRAWLINE_MSG  g_drawmsg; // 수신한 선 그리기 메시지 저장
static DRAWLINE_MSG  g_localdrawmsg; // 최근 선택한 선 그리기 메시지 저장
static int g_drawline;
static int g_drawwidth;
static int g_drawcolor;


// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);
// UDP 수신 스레드
DWORD WINAPI TCPRecvThread(LPVOID arg);

DWORD WINAPI ChatSendThread(LPVOID arg);
// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...);


void AddListColorItems(HWND hDlg);
HBITMAP CreateColorBitmap(int width, int height, COLORREF color);
void UpdateCurrentColorOption(int selectedColorIndex);

void AddListControlItems(HWND hDlg);
void UpdateCurrentFigureOption(int selectedIndex);


// 메인 함수
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	// 디버깅 모드일 시 콘솔 생성
#ifdef DEBUG_MODE
	if (AllocConsole()) {
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);

		setbuf(stdout, NULL);
	}
#endif

	// 윈속 초기화
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

	// 이벤트 생성
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(g_hWriteEvent == NULL) return 1;

	// 변수 초기화(일부)
	g_localdrawmsg.type = DRAW_LINE;
	g_localdrawmsg.color = RGB(255, 0, 0);
	g_localdrawmsg.width = 10;

	// 대화상자 생성
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);	

	// 이벤트 제거
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}


void draw_bitmap_image(char* filePath) {
	HBITMAP image_bitmap = (HBITMAP)LoadImageA(NULL, filePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

	HDC hdc = GetDC(g_hDrawWnd);
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, image_bitmap);

	BITMAP bitmap;
	GetObject(image_bitmap, sizeof(BITMAP), &bitmap);

	SetStretchBltMode(hdc, HALFTONE);

	RECT rect;
	if (GetClientRect(g_hDrawWnd, &rect)) {
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		StretchBlt(hdc, 0, 0, width, height,
			memDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
	}

	SelectObject(memDC, oldBitmap);
	DeleteDC(memDC);
}

void upload_image() {
	char filePath[MAX_PATH] = "";
	OPENFILENAME ofn = { 0 };

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = nullptr; // 다이얼로그의 소유자 핸들 (nullptr이면 기본값)
	ofn.lpstrFilter = "Bitmap Files (*.bmp)\0*.bmp\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; // 유효한 파일과 경로만 선택 가능
	ofn.lpstrDefExt = "bmp"; // 확장자가 없을 경우 기본 확장자 설정

	if (GetOpenFileName(&ofn)) {
		// 선택한 이미지 파일을 읽어서 TCP로 전송
		FILE* image_file = fopen(filePath, "rb");
		if (image_file) {
			fseek(image_file, 0, SEEK_END);
			size_t file_size = ftell(image_file);
			fseek(image_file, 0, SEEK_SET);

			char* buffer = (char*)malloc(file_size);
			if (buffer) {
				fread(buffer, sizeof(char), file_size, image_file);

				send_tcp_payload(UPLOAD_IMAGE, buffer, file_size);

				free(buffer);
			}
			fclose(image_file);
		}
	}
}

void remove_all() {

}

void send_whisp() {
	// 선택된 항목의 인덱스를 가져오기
	LRESULT selIndex = SendMessage((HWND)g_hUserList, LB_GETCURSEL, 0, 0);
	// 선택된 항목이 없는 경우 리턴
	if (selIndex == LB_ERR)
		return;

	SEND_WHISP_DATA data;

	// 선택된 항목의 텍스트를 가져오기
	SendMessage((HWND)g_hUserList, LB_GETTEXT, selIndex, (LPARAM)data.sender_id);

	// 입력한 메시지 가져오기
	SendMessage((HWND)g_hWhispText, WM_GETTEXT, (WPARAM)MSGSIZE, (LPARAM)data.message);

	// 입력한 내용이 없는 경우 리턴
	if (strlen(data.message) < 1)
		return;

	send_tcp_payload(SEND_WHISP, (char *)&data, sizeof(SEND_WHISP_DATA));
}


// Dialog 컨트롤 클릭 등 처리
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hButtonIsIPv6;
    static HWND hEditIPaddr;
    static HWND hEditPort;
    static HWND hButtonConnect;
    static HWND hEditMsg;
	static HWND hEditName;

	static HWND hListPenColor;	// 색 선택 리스트
    static HWND hBtnPenColor;   // 사용자 지정 색 선택 버튼
    static HWND hEditwidth;   // 글자 크기
    static HWND hPen1;			//펜 실선
    static HWND hPen2;			//펜 점선
    static HWND hhhighlighter; //형광펜
    static HWND heraser;		// 지우개
    static HWND hFigurelist;		// 도형 선택 버튼

    switch (uMsg) {
    case WM_INITDIALOG:
        // 컨트롤 핸들 얻기
        hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);
        hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
        hEditPort = GetDlgItem(hDlg, IDC_PORT);
        hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
        g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
        hEditMsg = GetDlgItem(hDlg, IDC_MSG);
        g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);
		hEditName = GetDlgItem(hDlg, IDC_USERNAME);
		
		hListPenColor = GetDlgItem(hDlg, IDC_LIST_COLOR); // 색 선택 리스트
        hBtnPenColor = GetDlgItem(hDlg, IDC_PENCOLOR); // 사용자 지정 색 선택 버튼
        hEditwidth = GetDlgItem(hDlg, IDC_EDIT1); // 크기
        hPen1 = GetDlgItem(hDlg, IDC_RADIOPEN); // 실선 선택 
        hPen2 = GetDlgItem(hDlg, IDC_RADIOPEN2); // 점선 선택 
        hhhighlighter = GetDlgItem(hDlg, IDC_RADIOHH); // 형광펜  컨트롤
        heraser = GetDlgItem(hDlg, IDC_RADIOERASER); // 지우개 컨트롤
        hFigurelist = GetDlgItem(hDlg, IDC_LIST2); // 도형 선택 컨트롤

		g_hUserList = GetDlgItem(hDlg, IDC_USERLIST); // 유저 리스트 컨트롤
		g_hWhispText = GetDlgItem(hDlg, IDC_WHISPTEXT);

        // 채팅, 사용자이름 최대 길이 제한
        SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage((HWND)g_hWhispText, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage(hEditName, EM_SETLIMITTEXT, USERNAMESIZE, 0);
        
		EnableWindow(g_hButtonSendMsg, FALSE);
        SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
        SetDlgItemInt(hDlg, IDC_PORT, SERVERPORT, FALSE);

		EnableWindow(GetDlgItem(hDlg, IDC_SENDWHISP), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_UPLOADIMG), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_REMOVEALL), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_REMOVEDRAW), FALSE);

        SendMessage(hPen1, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(hPen2, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(hhhighlighter, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(heraser, BM_SETCHECK, BST_UNCHECKED, 0);
        SetWindowText(GetDlgItem(hDlg, IDC_EDIT1), _T("10"));  // 기본값을 5로 설정
		AddListControlItems(hDlg);
		AddListColorItems(hDlg);
        EnableWindow(hBtnPenColor, FALSE);

        // 윈도우 클래스 등록
        WNDCLASS wndclass;
        wndclass.style = CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc = WndProc;
        wndclass.cbClsExtra = 0;
        wndclass.cbWndExtra = 0;
        wndclass.hInstance = g_hInst;
        wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wndclass.lpszMenuName = NULL;
        wndclass.lpszClassName = "MyWndClass";
        if (!RegisterClass(&wndclass)) return 1;

        // 자식 윈도우 생성
        g_hDrawWnd = CreateWindow("MyWndClass", "그림 그릴 윈도우", WS_CHILD,
            450, 190, 580, 580, hDlg, (HMENU)NULL, g_hInst, NULL);
        if (g_hDrawWnd == NULL) return 1;
        ShowWindow(g_hDrawWnd, SW_SHOW);
        UpdateWindow(g_hDrawWnd);

        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ISIPV6:
            g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
            if (g_isIPv6 == false)
                SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
            else
                SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV6);
            return TRUE;

        case IDC_CONNECT:
            GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
			GetDlgItemText(hDlg, IDC_USERNAME, g_username, sizeof(g_username));
			if (strlen(g_username) < 1) {
				MessageBoxA(NULL, "사용자 이름을 입력해주세요.", "오류", MB_OK | MB_ICONERROR);
				return TRUE;
			}

            g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
            g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);

            EnableWindow(hBtnPenColor, TRUE);

            // 소켓 통신 스레드 시작
            g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
            if (g_hClientThread == NULL) {
                MessageBox(hDlg, "클라이언트를 시작할 수 없습니다."
                    "\r\n프로그램을 종료합니다.", "실패!", MB_ICONERROR);
                EndDialog(hDlg, 0);
            }
            else {
                EnableWindow(hButtonConnect, FALSE);
                while (g_bStart == FALSE); // 서버 접속 성공 기다림

                EnableWindow(hButtonIsIPv6, FALSE);
                EnableWindow(hEditIPaddr, FALSE);
                EnableWindow(hEditPort, FALSE);
				EnableWindow(hEditPort, FALSE);
                EnableWindow(hEditName, FALSE);

				EnableWindow(GetDlgItem(hDlg, IDC_SENDWHISP), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_UPLOADIMG), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_REMOVEALL), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_REMOVEDRAW), TRUE);

				EnableWindow(g_hButtonSendMsg, TRUE);

                SetFocus(hEditMsg);
            }
            return TRUE;

        case IDC_SENDMSG:
            // 읽기 완료를 기다림
            WaitForSingleObject(g_hReadEvent, INFINITE);
            GetDlgItemText(hDlg, IDC_MSG, g_chatmsg.buf, MSGSIZE);
            // 쓰기 완료를 알림
            SetEvent(g_hWriteEvent);
            // 입력된 텍스트 전체를 선택 표시
            SendMessage(hEditMsg, EM_SETSEL, 0, -1);
            return TRUE;

        case IDC_PENCOLOR:
            SelectPenColor();
            return TRUE;


        case IDC_EDIT1: {
            TCHAR buffer[256];        // TCHAR 배열로 선언
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);  // IDC_EDIT1에 해당하는 HWND 얻기
            GetWindowText(hEdit, buffer, 256);          // Edit 컨트롤에서 텍스트 가져오기

            g_localdrawmsg.width = _ttoi(buffer);               // _ttoi를 사용하여 텍스트를 정수로 변환
            return TRUE;
        }

        case IDC_RADIOPEN:
			g_localdrawmsg.line = PS_SOLID; // 실선
            return TRUE;

        case IDC_RADIOPEN2:
			g_localdrawmsg.line = PS_DOT; // 점선
			g_localdrawmsg.width = 1;
			SetWindowText(GetDlgItem(hDlg, IDC_THICK), TEXT("1"));
            return TRUE;

        case IDC_RADIOHH:

            return TRUE;

        case IDC_RADIOERASER:
			g_localdrawmsg.type = DRAW_ERASER;
            return TRUE;
        case IDC_LIST2:
			//SelectFigureOption(hDlg, g_currentSelectFigureMode);

            return TRUE;

		case IDC_UPLOADIMG:
			upload_image();
			return TRUE;

		case IDC_REMOVEALL:
			remove_all();
			return TRUE;

		case IDC_SENDWHISP:
			send_whisp();
			return TRUE;

        case IDCANCEL:
            if (MessageBox(hDlg, "정말로 종료하시겠습니까?",
                "질문", MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                closesocket(g_sock_tcp);
				closesocket(g_sock_udp);
                EndDialog(hDlg, IDCANCEL);
            }
            return TRUE;
        }
        return FALSE;

	case WM_NOTIFY:
	{
		LPNMHDR pnmhdr = (LPNMHDR)lParam;

		// 도형 선택 변경 처리 (기존 코드)
		if (pnmhdr->idFrom == IDC_LIST2 && pnmhdr->code == LVN_ITEMCHANGED) {
			NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
			if (pnmv->uNewState & LVIS_SELECTED) {
				int selectedIndex = pnmv->iItem; // 선택된 항목 인덱스 가져오기
				UpdateCurrentFigureOption(selectedIndex); // 도형 변경
			}
		}

		// 색상 선택 변경 처리 (새로 추가된 코드)
		if (pnmhdr->idFrom == IDC_LIST_COLOR && pnmhdr->code == LVN_ITEMCHANGED) {
			NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
			if (pnmv->uNewState & LVIS_SELECTED) {
				int selectedColorIndex = pnmv->iItem; // 색상 항목 인덱스 가져오기
				UpdateCurrentColorOption(selectedColorIndex); // 색상 변경 함수 호출
			}
		}
	}
	return TRUE;



    case WM_CTLCOLORSTATIC:
        switch (GetDlgCtrlID((HWND)lParam)) {
        case IDC_STATUS:
        {
            HDC hdcStatic = (HDC)wParam;
            // 배경 색상 변경 
            SetBkColor(hdcStatic, RGB(120, 120, 120));  // RGB 값으로 색 지정

            // 텍스트 색상 변경 
            SetTextColor(hdcStatic, RGB(0, 255, 0));  // 텍스트 색을 검은색으로

            // 배경색을 설정하는 브러시 반환
            return (LRESULT)CreateSolidBrush(RGB(120, 120, 120));  // 배경색을 지정한 색으로 설정
        }
        }
        break;
    }

    return FALSE;
}


// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	// TCP 소켓 생성
	g_sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(g_sock_tcp == INVALID_SOCKET) err_quit("TCP socket()");

	// 소켓의 지역주소 재사용여부 설정
	bool toggle = true;
	setsockopt(g_sock_udp, SOL_SOCKET, SO_REUSEADDR, (char*)&toggle, sizeof(toggle));

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
	serveraddr.sin_port = htons(g_port);
	retval = connect(g_sock_tcp, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("TCP connect()");

	// 사용자 이름 전송
	send_tcp_payload(SET_USER_NAME, g_username, strlen(g_username) + 1);


	// UDP 소켓 생성
	g_sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_sock_udp == INVALID_SOCKET) err_quit("UDP socket()");

	ZeroMemory(&g_serveraddr, sizeof(g_serveraddr));
	g_serveraddr.sin_family = AF_INET;
	g_serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
	g_serveraddr.sin_port = htons(g_port);

	MessageBox(NULL, "서버에 접속했습니다.", "성공!", MB_ICONINFORMATION);

	// 읽기 & 쓰기 스레드 생성
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, TCPRecvThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, ChatSendThread, NULL, 0, NULL);
	if(hThread[0] == NULL || hThread[1] == NULL){
		MessageBox(NULL, "스레드를 시작할 수 없습니다."
			"\r\n프로그램을 종료합니다.",
			"실패!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// 스레드 종료 대기
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if(retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;

	MessageBox(NULL, "서버가 접속을 끊었습니다", "알림", MB_ICONINFORMATION);

	closesocket(g_sock_tcp);
	closesocket(g_sock_udp);

	ExitProcess(0); // 서버와 접속 끊기면 프로그램 종료
	return 0;
}



// TCP 데이터 수신
DWORD WINAPI TCPRecvThread(LPVOID arg)
{
	int retval;

	while (1) {
		MessageInfo message_info;
		memset(&message_info, 0, sizeof(MessageInfo));

		// 고정길이(8바이트)의 메세지 정보(메세지 타입, 페이로드 길이)를 수신
		retval = recv(g_sock_tcp, (char*)&message_info, sizeof(message_info), 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			return 0;
		}

		printf("[TCP/%s:%d] Type: %d Length: %d\n", inet_ntoa(g_serveraddr.sin_addr), ntohs(g_serveraddr.sin_port),
			message_info.payload_type, message_info.payload_length);

		// 페이로드 크기만큼 메모리 동적할당
		// malloc 사용시 크기가 큰 이미지파일 수신할 때 할당이 제대로 이루어지지 않아 Windows API 사용
		char* recv_buf = (char*)VirtualAlloc(NULL, 0xFFFFFFF, MEM_COMMIT, PAGE_READWRITE);
		if (!recv_buf) {
			err_display("TCPRecvThread malloc()");
			continue;
		}

		// 받은 페이로드 크기만큼 가변 길이 페이로드 받기
		retval = recvn(g_sock_tcp, (char*)recv_buf, message_info.payload_length, 0);
		if (retval == SOCKET_ERROR) {
			err_display("TCPRecvThread recv()");
			continue;
		}

		// 받은 메시지 출력
#ifdef LOG_PACKET_RAW
		printf("Payload: %s\n", byteArrayToHexString(recv_buf, message_info.payload_length).c_str());
#endif


		// 메시지 처리
		if (message_info.payload_type == CHATTING) {
			DisplayText("%s\r\n", recv_buf);
		}

		// 이미지 파일 수신 처리
		else if (message_info.payload_type == UPLOAD_IMAGE) {
			char file_name[MAX_PATH];

			// 수신한 이미지 파일 생성
			time_t now = time(NULL);
			struct tm* localTime = localtime(&now);
			strftime(file_name, sizeof(file_name), "%Y%m%d%H%M%S", localTime);
			strcat(file_name, ".bmp");

			printf("image file name: %s\n", file_name);

			FILE *image_file = fopen(file_name, "wb");
			if (image_file) {
				fwrite(recv_buf, sizeof(char), message_info.payload_length, image_file);
				fclose(image_file);
			}

			// 이미지 교체
			draw_bitmap_image(file_name);
		}

		// 유저 리스트 데이터를 수신했을 때 처리
		else if (message_info.payload_type == USER_LIST_DATA) {
			SendMessage((HWND)g_hUserList, LB_RESETCONTENT, 0, 0); // 리스트의 모든 항목 삭제

			char* item_text = strtok(recv_buf, "|");
			while (item_text != NULL) { // |를 기준으로 문자열 분리
				SendMessage((HWND)g_hUserList, LB_ADDSTRING, 0, (LPARAM)item_text); // 리스트에 데이터 추가
				item_text = strtok(NULL, "|");
			}
		}

		// 선 그리기 처리
		else if ((message_info.payload_type >= DRAW_LINE) && (message_info.payload_type <= DRAW_ARROW)) {
			memcpy(&g_drawmsg, recv_buf, sizeof(g_drawmsg));
			g_drawline = g_drawmsg.line;
			g_drawwidth = g_drawmsg.width;
			g_drawcolor = g_drawmsg.color;

			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(g_drawmsg.x0, g_drawmsg.y0),
				MAKELPARAM(g_drawmsg.x1, g_drawmsg.y1));
		}

		// 처리가 끝나면 할당해제
		VirtualFree(recv_buf, 0, MEM_RELEASE);
	}
	return 0;
}

// 채팅 전송 스레드
DWORD WINAPI ChatSendThread(LPVOID arg)
{
	int retval;

	// 서버와 데이터 통신
	while (1) {
		// 쓰기 완료 기다리기
		WaitForSingleObject(g_hWriteEvent, INFINITE);

		// 문자열 길이가 0이면 보내지 않음
		if (strlen(g_chatmsg.buf) == 0) {
			// '메시지 전송' 버튼 활성화
			EnableWindow(g_hButtonSendMsg, TRUE);
			// 읽기 완료 알리기
			SetEvent(g_hReadEvent);
			continue;
		}

		send_tcp_payload(CHATTING, g_chatmsg.buf, strlen(g_chatmsg.buf) + 1);

		// '메시지 전송' 버튼 활성화
		EnableWindow(g_hButtonSendMsg, TRUE);
		// 읽기 완료 알리기
		SetEvent(g_hReadEvent);
	}
	return 0;
}


// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HBITMAP hBitmap;
	static HDC hDCMem;
	static int x0, y0;
	static int x1, y1;
	static BOOL bDrawing = FALSE;

	// 로컬 설정 가져오기
	memcpy(&g_drawmsg, &g_localdrawmsg, sizeof(DRAWLINE_MSG));

	switch(uMsg){
	case WM_CREATE:
		hDC = GetDC(hWnd);

		// 화면을 저장할 비트맵 생성
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// 메모리 DC 생성C
		hDCMem = CreateCompatibleDC(hDC);

		// 비트맵 선택 후 메모리 DC 화면을 흰색으로 칠함
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		ReleaseDC(hWnd, hDC);
		return 0;

	case WM_LBUTTONDOWN:
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		bDrawing = TRUE;
		return 0;

	case WM_MOUSEMOVE:
		if (bDrawing && g_bStart && g_drawmsg.type == DRAW_LINE) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			x0 = x1;
			y0 = y1;
		}
		return 0;

	case WM_LBUTTONUP:
		x1 = LOWORD(lParam);
		y1 = HIWORD(lParam);

		// 메시지 타입에 따른 그리기 처리
		if (g_drawmsg.type == DRAW_STRAIGHTLINE) {
			// 직선 그리기
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_ELLIPSE) {

		}
		else if (g_drawmsg.type == DRAW_RECTANGLE) {
			// 사각형의 네 점 계산
			int x2 = x0;
			int y2 = y1;
			int x3 = x1;
			int y3 = y0;

			// 사각형의 네 점을 연결하는 직선 그리기
			// (x0, y0) -> (x1, y0)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y0;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// (x1, y0) -> (x1, y1)
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// (x1, y1) -> (x0, y1)
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0;
			g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// (x0, y1) -> (x0, y0)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0;
			g_drawmsg.y1 = y0;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_TRIANGLE) {
			// 정삼각형 그리기
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;	g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);	g_drawmsg.y1 = y0;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x1;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);	g_drawmsg.y1 = y0;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_RIGHTTRIANGLE) {
			// 직각 삼각형 그리기
			// 첫 번째 선: (x0, y0) -> (x1, y0) (수평선)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y0;  // y0로 동일하게 설정하여 수평선 만들기
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 두 번째 선: (x1, y0) -> (x1, y1) (수직선)
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y0;  // 수평선 끝 지점에서 시작
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;  // y1로 이동하여 수직선
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 세 번째 선: (x1, y1) -> (x0, y0) (대각선)
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0;
			g_drawmsg.y1 = y0;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_STAR) {
			//별 그리기
			// y 이동 비율
			double moveFactor = 0.3;

			// 첫 번째 삼각형 (아래 방향)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;  // 밑변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);
			g_drawmsg.y1 = y0;  // 왼쪽 변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);
			g_drawmsg.y1 = y0;  // 오른쪽 변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 두 번째 삼각형 (위 방향) - 30% 아래로 이동
			int offset_y = (int)((y1 - y0) * moveFactor);

			// 두 번째 삼각형을 30% 아래로 이동시킨 위치에서 그리기
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0 + offset_y;  // 이동된 y 값
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y0 + offset_y;  // 밑변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0 + offset_y;  // 이동된 y 값
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);
			g_drawmsg.y1 = y1 + offset_y;  // 왼쪽 변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y0 + offset_y;  // 이동된 y 값
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);
			g_drawmsg.y1 = y1 + offset_y;  // 오른쪽 변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

		}
		else if (g_drawmsg.type == DRAW_PARALLELOGRAM) {
			// 평행사변형 그리기
			// 평행사변형의 첫 번째 변 (왼쪽 아래에서 오른쪽 아래로)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;  // 아래쪽 변
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 평행사변형의 두 번째 변 (왼쪽 위에서 오른쪽 위로)
			// 첫 번째 변에 대해 일정한 오프셋을 적용하여 평행하게 이동시킴
			int offset = 30;  // 평행선의 오프셋
			g_drawmsg.x0 = x0 + offset;
			g_drawmsg.y0 = y0;  // 첫 번째 변의 y 좌표와 동일
			g_drawmsg.x1 = x1 + offset;
			g_drawmsg.y1 = y0;  // 두 번째 변도 y 좌표는 동일하게
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 평행사변형의 왼쪽 대각선 (왼쪽 아래에서 왼쪽 위로)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + offset;
			g_drawmsg.y1 = y0;  // 왼쪽 대각선
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 평행사변형의 오른쪽 대각선 (오른쪽 아래에서 오른쪽 위로)
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1 + offset;
			g_drawmsg.y1 = y0;  // 오른쪽 대각선
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_DIAMOND) {
			// 마름모 그리기
			// 마름모의 중심점 (x0, y0)과 대각선의 길이 (diagonal1, diagonal2)
			int diagonal1 = abs(x1 - x0);  // 첫 번째 대각선의 길이 (가로)
			int diagonal2 = abs(y1 - y0);  // 두 번째 대각선의 길이 (세로)

			// 대각선의 중심점을 기준으로 각 꼭짓점을 계산
			int halfDiagonal1 = diagonal1 / 2;
			int halfDiagonal2 = diagonal2 / 2;

			// 마름모의 네 점을 계산
			int point1_x = x0;
			int point1_y = y0 - halfDiagonal2;  // 위쪽 꼭짓점

			int point2_x = x0 + halfDiagonal1;
			int point2_y = y0;  // 오른쪽 꼭짓점

			int point3_x = x0;
			int point3_y = y0 + halfDiagonal2;  // 아래쪽 꼭짓점

			int point4_x = x0 - halfDiagonal1;
			int point4_y = y0;  // 왼쪽 꼭짓점

			// 네 꼭짓점을 이어서 마름모를 그립니다
			g_drawmsg.x0 = point1_x;
			g_drawmsg.y0 = point1_y;
			g_drawmsg.x1 = point2_x;
			g_drawmsg.y1 = point2_y;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = point2_x;
			g_drawmsg.y0 = point2_y;
			g_drawmsg.x1 = point3_x;
			g_drawmsg.y1 = point3_y;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = point3_x;
			g_drawmsg.y0 = point3_y;
			g_drawmsg.x1 = point4_x;
			g_drawmsg.y1 = point4_y;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			g_drawmsg.x0 = point4_x;
			g_drawmsg.y0 = point4_y;
			g_drawmsg.x1 = point1_x;
			g_drawmsg.y1 = point1_y;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}
		else if (g_drawmsg.type == DRAW_ARROW) {
			// 화살표 그리기
			// 화살표 몸통 (직선)
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 화살촉을 그리기 위해, 시작점과 끝점 간의 각도를 계산
			double angle = atan2(y1 - y0, x1 - x0);  // 화살표 방향 각도 계산 (radian 단위)
			int arrowheadLength = 30;  // 화살촉 길이

			// 화살촉 왼쪽 끝 점
			int x_left = x1 - arrowheadLength * cos(angle + 3.14159 / 6);  // 30도 왼쪽 회전
			int y_left = y1 - arrowheadLength * sin(angle + 3.14159 / 6);

			// 화살촉 오른쪽 끝 점
			int x_right = x1 - arrowheadLength * cos(angle - 3.14159 / 6);  // 30도 오른쪽 회전
			int y_right = y1 - arrowheadLength * sin(angle - 3.14159 / 6);

			// 왼쪽 화살촉 선
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x_left;
			g_drawmsg.y1 = y_left;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));

			// 오른쪽 화살촉 선
			g_drawmsg.x0 = x1;
			g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x_right;
			g_drawmsg.y1 = y_right;
			send_udp_payload(g_drawmsg.type, (char*)&g_drawmsg, sizeof(g_drawmsg));
		}

		bDrawing = FALSE;
		return 0;

	case WM_DRAWIT:
		hDC = GetDC(hWnd);

		hPen = CreatePen(g_drawline, g_drawwidth, g_drawcolor);

		// 화면에 그리기
		hOldPen = (HPEN)SelectObject(hDC, hPen);
		MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
		LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
		SelectObject(hDC, hOldPen);

		// 메모리 비트맵에 그리기
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);
		MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam), NULL);
		LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
		SelectObject(hDC, hOldPen);

		DeleteObject(hPen);
		ReleaseDC(hWnd, hDC);
		return 0;

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// 메모리 비트맵에 저장된 그림을 화면에 전송
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left,
			rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		return 0;

	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 에디트 컨트롤에 문자열 출력
void DisplayText(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditStatus);
	SendMessage(g_hEditStatus, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

void AddListColorItems(HWND hDlg) {
	HWND hList = GetDlgItem(hDlg, IDC_LIST_COLOR);

	// 1. 이미지 리스트 생성 (32x32 크기)
	HIMAGELIST hImageList = ImageList_Create(32, 32, ILC_COLOR32 | ILC_MASK, 10, 10);

	// 2. 색상 배열 정의
	COLORREF colors[] = {
		RGB(255, 0, 0),   // 빨강
		RGB(0, 255, 0),   // 초록
		RGB(0, 0, 255),   // 파랑
		RGB(255, 255, 0), // 노랑
		RGB(0, 255, 255), // 청록
		RGB(255, 0, 255), // 자홍
		RGB(128, 0, 0),   // 어두운 빨강
		RGB(0, 128, 0),   // 어두운 초록
		RGB(0, 0, 128),   // 어두운 파랑
		RGB(128, 128, 128) // 회색
	};

	// 3. 각 색상에 대한 비트맵 생성 및 이미지 리스트에 추가
	for (int i = 0; i < _countof(colors); i++) {
		HBITMAP hBitmap = CreateColorBitmap(32, 32, colors[i]);
		ImageList_Add(hImageList, hBitmap, NULL);
		DeleteObject(hBitmap); // 비트맵 해제
	}

	// 4. 이미지 리스트를 리스트 컨트롤에 설정
	ListView_SetImageList(hList, hImageList, LVSIL_NORMAL);

	// 5. 보기 스타일을 아이콘 뷰로 설정
	ListView_SetView(hList, LV_VIEW_ICON);

	// 6. 아이템 크기 설정 (간격을 수동으로 설정)
	SendMessage(hList, LVM_SETICONSPACING, 0, MAKELPARAM(40, 40));  // 수평, 수직 간격 설정

	// 7. 리스트 항목 추가
	LVITEM lvItem = { 0 };
	lvItem.mask = LVIF_TEXT | LVIF_IMAGE;

	for (int i = 0; i < _countof(colors); i++) {
		lvItem.iItem = i;
		lvItem.iImage = i; // 이미지 리스트에서 색상 비트맵의 인덱스
		lvItem.pszText = _T(""); // 텍스트는 표시하지 않음
		ListView_InsertItem(hList, &lvItem);
	}
}



// 32x32 크기의 색상 비트맵 생성 함수
HBITMAP CreateColorBitmap(int width, int height, COLORREF color) {
	HBITMAP hBitmap = CreateCompatibleBitmap(GetDC(NULL), width, height);
	HDC hDC = CreateCompatibleDC(NULL);
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hDC, hBitmap);

	// 지정된 색으로 채우기
	HBRUSH hBrush = CreateSolidBrush(color);
	RECT rect = { 0, 0, width, height };
	FillRect(hDC, &rect, hBrush);

	// 리소스 해제
	DeleteObject(hBrush);
	SelectObject(hDC, hOldBitmap);
	DeleteDC(hDC);

	return hBitmap;
}

void UpdateCurrentColorOption(int selectedColorIndex)
{
	// 색상 인덱스에 맞게 g_localdrawmsg.color 값을 설정
	switch (selectedColorIndex)
	{
	case 0: // 빨강
		g_localdrawmsg.color = RGB(255, 0, 0);
		break;
	case 1: // 초록
		g_localdrawmsg.color = RGB(0, 255, 0);
		break;
	case 2: // 파랑
		g_localdrawmsg.color = RGB(0, 0, 255);
		break;
	case 3: // 노랑
		g_localdrawmsg.color = RGB(255, 255, 0);
		break;
	case 4: // 청록
		g_localdrawmsg.color = RGB(0, 255, 255);
		break;
	case 5: // 자홍
		g_localdrawmsg.color = RGB(255, 0, 255);
		break;
	case 6: // 어두운 빨강
		g_localdrawmsg.color = RGB(128, 0, 0);
		break;
	case 7: // 어두운 초록
		g_localdrawmsg.color = RGB(0, 128, 0);
		break;
	case 8: // 어두운 파랑
		g_localdrawmsg.color = RGB(0, 0, 128);
		break;
	case 9: // 회색
		g_localdrawmsg.color = RGB(128, 128, 128);
		break;
	default:
		g_localdrawmsg.color = RGB(255, 255, 255); // 기본색 (흰색)
		break;
	}

	// 디버그용 메시지 박스 출력 (선택된 색상 확인)
	TCHAR buffer[50];
	_stprintf_s(buffer, _T("선택된 색상: (%d, %d, %d)"), GetRValue(g_localdrawmsg.color), GetGValue(g_localdrawmsg.color), GetBValue(g_localdrawmsg.color));
	MessageBox(NULL, buffer, _T("색상 선택"), MB_OK);
}






void SelectPenColor() {
	// 색상 대화 상자 열기
	CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
	static COLORREF customColors[16] = { 0 }; // 사용자 정의 색상
	cc.hwndOwner = g_hDrawWnd;
	cc.lpCustColors = customColors;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	cc.rgbResult = g_localdrawmsg.color;
	if (ChooseColor(&cc)) {
		g_localdrawmsg.color = cc.rgbResult;
	}
}


/*
void AddFigureOption(HWND hDlg) {
	// 항목 추가
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("지우개"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("선"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("타원"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("사각형"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("삼각형"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("직선"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("오각형"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("별"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("사다리꼴"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("밤톨"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("평행사변형"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("마름모"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_ADDSTRING, 0, (LPARAM)_T("화살표"));
	SendDlgItemMessage(hDlg, IDC_FIGURE, CB_SETCURSEL, 0, 0);
}

*/

void AddListControlItems(HWND hDlg) {
	HWND hList = GetDlgItem(hDlg, IDC_LIST2); // 리스트 컨트롤 핸들 가져오기

	// 1. 이미지 리스트 생성
	HIMAGELIST hImageList = ImageList_Create(32, 32, ILC_COLOR32 | ILC_MASK, 10, 10);

	// 아이콘 파일 로드 및 추가
	const TCHAR* iconPaths[] = {
		_T(".\\icons\\선.ico"),
		_T(".\\icons\\직선.ico"),
		_T(".\\icons\\타원.ico"),
		_T(".\\icons\\사각형.ico"),
		_T(".\\icons\\삼각형.ico"),
		_T(".\\icons\\직각 삼각형.ico"),
		_T(".\\icons\\별.ico"),
		_T(".\\icons\\평행 사변형.ico"),
		_T(".\\icons\\마름모.ico"),
		_T(".\\icons\\화살표.ico")
	};

	for (int i = 0; i < _countof(iconPaths); i++) {
		HICON hIcon = (HICON)LoadImage(NULL, iconPaths[i], IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
		if (hIcon) {
			ImageList_AddIcon(hImageList, hIcon);
			DestroyIcon(hIcon); // 메모리 릭 방지
		}
		else {
			MessageBox(hDlg, _T("아이콘 로드 실패"), _T("오류"), MB_OK | MB_ICONERROR);
		}
	}

	// 2. 이미지 리스트를 리스트 컨트롤에 설정
	ListView_SetImageList(hList, hImageList, LVSIL_NORMAL);

	// 3. 보기 스타일을 아이콘 뷰로 설정
	ListView_SetView(hList, LV_VIEW_ICON);

	// 4. 아이템 추가
	const TCHAR* itemTexts[] = {
		_T("선"),
		_T("직선"),
		_T("타원"),
		_T("사각형"),
		_T("삼각형"),
		_T("직각 삼각형"),
		_T("별"),
		_T("평행사변형"),
		_T("마름모"),
		_T("화살표")
	};

	LVITEM lvItem = { 0 };
	lvItem.mask = LVIF_TEXT | LVIF_IMAGE;

	for (int i = 0; i < _countof(itemTexts); i++) {
		lvItem.iItem = i; // 항목 인덱스
		lvItem.iImage = i; // 이미지 리스트의 아이콘 인덱스
		lvItem.pszText = (LPTSTR)itemTexts[i]; // 텍스트
		ListView_InsertItem(hList, &lvItem);
	}
}



// 선택된 항목에 따라 옵션 업데이트

void UpdateCurrentFigureOption(int selectedIndex)
{
	switch (selectedIndex)
	{
	case 0: // 선
		g_localdrawmsg.type = DRAW_LINE;
		break;
	case 1: // 직선
		g_localdrawmsg.type = DRAW_STRAIGHTLINE;
		break;
	case 2: // 타원
		g_localdrawmsg.type = DRAW_ELLIPSE;
		break;
	case 3: // 사각형
		g_localdrawmsg.type = DRAW_RECTANGLE;
		break;
	case 4: // 삼각형
		g_localdrawmsg.type = DRAW_TRIANGLE;
		break;
	case 5: // 직각 삼각형
		g_localdrawmsg.type = DRAW_RIGHTTRIANGLE;
		break;
	case 6: // 별
		g_localdrawmsg.type = DRAW_STAR;
		break;
	case 7: // 평행사변형
		g_localdrawmsg.type = DRAW_PARALLELOGRAM;
		break;
	case 8: // 마름모
		g_localdrawmsg.type = DRAW_DIAMOND;
		break;
	case 9: // 화살표
		g_localdrawmsg.type = DRAW_ARROW;
		break;
	default:
		g_localdrawmsg.type = -1; // 잘못된 선택
		break;
	}

	// 디버그용 메시지 박스 출력 (선택 확인)
	TCHAR buffer[50];
	_stprintf_s(buffer, _T("선택된 옵션: %d"), g_localdrawmsg.type);
	MessageBox(NULL, buffer, _T("도형 선택"), MB_OK);
}

