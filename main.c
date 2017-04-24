#include <windows.h>
#include <fftw3.h>
#include <commctrl.h>
#include <mmreg.h>
#include <math.h>
#include <stdbool.h>
#include "resource.h"

/*
 * COMMENCE SPAGHETTI CODE (SRA) V1
 */

#define SAMPLES 2048
#define AVG 3


struct main_window {
	HWND window, list, target;
};

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TargetProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void BuildWindow(HINSTANCE hInst, int show, struct main_window *win);
void CALLBACK AudioProc(HWAVEIN hwi, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2);
int PopulateList(void);
void Disconnect(void) ;
void Connect(int id) ;

struct main_window win;
float buffd[AVG][SAMPLES];
int use = 0;
fftwf_plan plan;
float data[SAMPLES];
float dct[SAMPLES];
bool connected, ignore_close;
HWAVEIN device;
WAVEHDR buff;



int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_inst, LPSTR cmd_line, int cmd_show) {
	MSG msg;
	int timer_id;
	WNDCLASS wndclass;


	connected = ignore_close = false;
	plan = fftwf_plan_r2r_1d(SAMPLES, data, dct, FFTW_REDFT10, FFTW_ESTIMATE);

	ZeroMemory(&wndclass, sizeof wndclass);

	wndclass.lpszClassName = TEXT("SPECTARGET");
	wndclass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	wndclass.style         = CS_HREDRAW;
	wndclass.lpfnWndProc   = TargetProc;
	wndclass.hCursor       = LoadCursor(0, IDC_ARROW);
	RegisterClass(&wndclass);


	BuildWindow(instance, cmd_show, &win);
	PopulateList();
	timer_id = SetTimer(NULL, 0, 5000, NULL);
	while(GetMessage(&msg, NULL, 0, 0) > 0) {
		if(msg.message == WM_TIMER && msg.wParam == timer_id) {
			PopulateList();
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	KillTimer(NULL, (UINT_PTR) timer_id);
	ignore_close = true;
	waveInClose(device);
	fftwf_cleanup();

	return msg.wParam;
}

void CALLBACK AudioProc(HWAVEIN hwi, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
	int i;
	float v;

	switch(msg) {
		case WIM_CLOSE:
			if(!ignore_close) {
				MessageBox(win.window, TEXT("Audio input closed unexpectedly"), TEXT("Error"), MB_OK | MB_ICONWARNING);
			}
			else {
				ignore_close = false;
			}
			break;
		case WIM_DATA:
			if(!connected) break;
			fftwf_execute(plan);
			for(i = 0; i < SAMPLES; i++) {
				if(dct[i] == 0) {
					buffd[use][i] = 0;
				}
				else {
					v = 10 * log10f(dct[i] * dct[i]);
					buffd[use][i] = max(0, min(1, v / 60));
				}
			}
			use++;
			use = use < AVG ? use : 0;
			waveInAddBuffer(hwi, (LPWAVEHDR) param1, sizeof(WAVEHDR));
			fflush(stdout);
			RedrawWindow(win.target, NULL, NULL, RDW_INVALIDATE);
			break;
		default:
			break;
	}
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	int i;
	switch(msg) {
		case WM_COMMAND:
			if(HIWORD(wParam) == CBN_SELENDOK) {
				i = SendMessage(win.list, CB_GETCURSEL, 0, 0);
				if(i != CB_ERR) {
					Connect(i);
				}
			}
			break;
		case WM_SIZE:
			MoveWindow(win.target, 0, 24, LOWORD(lParam), max(HIWORD(lParam) - 24, 0), TRUE);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK TargetProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	HDC dc;
	PAINTSTRUCT paint;
	HBRUSH brush;
	int x, i;
	RECT rect;
	float s, a;

	switch(msg) {
		case WM_PAINT:
			dc = BeginPaint(hWnd, &paint);
			GetClientRect(hWnd, &rect);

			brush = CreateSolidBrush(RGB(0, 0, 0));
			FillRect(dc, &rect, brush);
			DeleteObject(brush);

			SelectObject(dc, GetStockObject(DC_PEN));

			for(x = rect.left/*, last = 0*/; x < rect.right; x++) {
				MoveToEx(dc, x, rect.bottom, NULL);
				s = x;
				s /= (rect.right - rect.left);
				s *= s;
				s *= SAMPLES;
				for(i = 0, a = 0; i < AVG; i++) {
					a += buffd[i][(int) s];
				}
				a /= AVG;

				SetDCPenColor(dc, RGB(a > 0.5 ? (a - 0.5) * 2 * (a - 0.5) * 2 * 150 : 0,
									  a > 0.5 ? (1 - a) * 2 * (1 - a) * 2 * 150 : 150, 0));

				LineTo(dc, x, (int) floorf(rect.bottom - (a * (rect.bottom - rect.top))));
			}

			EndPaint(hWnd, &paint);
			return 0;
		default:
			break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void BuildWindow(HINSTANCE hInst, int show, struct main_window *win) {
	HWND hwnd;
	HFONT hfont;
	WNDCLASSEX wcex;
	LPCWSTR classname = TEXT("FancyClassNameNobodyWillCareAbout");

	InitCommonControls();
	ZeroMemory(&wcex, sizeof wcex);
	wcex.cbSize = sizeof wcex;
	wcex.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
	wcex.lpszMenuName = 0;


	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWndProc;
	wcex.hInstance = hInst;
	wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = classname;

	if(!RegisterClassEx(&wcex)) {
		MessageBox(NULL, TEXT("Error registering window class."), TEXT("Error"), MB_ICONERROR | MB_OK);
	}

	hfont = CreateFont(-11, 0, 0, 0, 400, FALSE, FALSE, FALSE, 1, 400, 0, 0, 0, TEXT("Ms Shell Dlg"));
	hwnd = win->window = CreateWindowEx(WS_EX_LEFT, classname, TEXT("Fun"), WS_TILEDWINDOW | WS_VISIBLE, 0, 0, 285, 179,
										0, 0, hInst, 0);
	/*win->button = CreateWindowEx(0, WC_BUTTON, TEXT("Connect"), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 128,
								 0,
								 51, 23, hwnd, (HMENU) 0, hInst, 0);
	SendMessage(win->button, WM_SETFONT, (WPARAM) hfont, FALSE);*/
	win->list = CreateWindowEx(0, WC_COMBOBOX, NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS, 3,
							   1, 122, 49, hwnd, (HMENU) 0, hInst, 0);
	SendMessage(win->list, WM_SETFONT, (WPARAM) hfont, FALSE);
	win->target = CreateWindowEx(0, TEXT("SPECTARGET"), NULL, WS_CHILD | WS_VISIBLE, 0, 24, 278, 130, hwnd, (HMENU) 0,
								 hInst, 0);
	//SetWindowLongPtr(win->target, GWLP_WNDPROC, (LONG) TargetProc);
	//SendMessage(win->target, WM_SETFONT, (WPARAM)hfont, FALSE);

	ShowWindow(hwnd, show);
}

int PopulateList(void) {
	WAVEINCAPS caps;
	int i, top;

	SendMessage(win.list, CB_RESETCONTENT, 0, 0);
	for(top = waveInGetNumDevs(), i = 0; i < top; i++) {
		SendMessage(win.list, CB_ADDSTRING, 0,
					(LPARAM) (waveInGetDevCaps((UINT_PTR) i, &caps, sizeof caps) == MMSYSERR_NOERROR ?
							  caps.szPname : TEXT("UNKNOWN")));
	}

	if(connected && waveInGetID(device, (LPUINT) &i) == MMSYSERR_NOERROR) {
		SendMessage(win.list, CB_SETCURSEL, (WPARAM) i, 0);
	}

	return top;
}

void Connect(int id) {
	WAVEFORMATEXTENSIBLE format;

	if(connected) {
		Disconnect();
	}

	format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	format.Format.nChannels = 1;
	format.Format.nSamplesPerSec = 44100;
	format.Format.wBitsPerSample = 32;
	format.Format.cbSize = sizeof format - sizeof(WAVEFORMATEX);
	format.Format.nBlockAlign = (WORD) (format.Format.nChannels * format.Format.wBitsPerSample / 8);
	format.Format.nAvgBytesPerSec = format.Format.nBlockAlign * format.Format.nSamplesPerSec;
	format.Samples.wValidBitsPerSample = 32;
	format.dwChannelMask = 0;
	format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

	if(waveInOpen(&device, (UINT) id, (LPCWAVEFORMATEX) &format, (DWORD_PTR) AudioProc, 0, CALLBACK_FUNCTION) ==
	   MMSYSERR_NOERROR) {
		connected = true;
	}
	else {
		SendMessage(win.list, CB_SETCURSEL, (WPARAM) -1, 0);
		MessageBox(win.window, TEXT("Was unable to connect with audio device"), TEXT("Error"), MB_OK | MB_ICONERROR);
		return;
	}

	buff.lpData = (LPSTR) data;
	buff.dwBufferLength = sizeof data;
	buff.dwBytesRecorded = 0;
	buff.dwUser = 0;
	buff.dwFlags = 0;
	buff.dwLoops = 0;

	waveInPrepareHeader(device, &buff, sizeof buff);
	waveInAddBuffer(device, &buff, sizeof buff);
	waveInStart(device);
}

void Disconnect(void) {
	ignore_close = true;
	connected = false;
	waveInStop(device);
	waveInReset(device);
	waveInUnprepareHeader(device, &buff, sizeof buff);
	waveInClose(device);
}