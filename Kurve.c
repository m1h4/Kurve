#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>

ULONG font = 22;							// Height of default font
ULONG thread = 0;							// ID of sound thread
ULONG width, height;						// Width and height of display area
HBITMAP bitmap[3] = {NULL, NULL, NULL};		// Screen buffers
HDC context[3] = {NULL, NULL, NULL};		// Screen buffer handles
FLOAT state = -1, paused = 0;				// Current game FSM state variables
ULONG limit;								// Score to which the game is played

typedef struct
{
	FLOAT Position[2][2];					// Current and previous player positions
	FLOAT Orientation;						// Current player orientation
	FLOAT Life;								// Player FSM state variable
	ULONG Score;							// Current player score
	COLORREF Color;							// Player color
	ULONG Left;								// Key bound to player turn left
	ULONG Right;							// Key bound to player turn right
} PLAYER,*LPPLAYER;

// TODO Load some of these settings from a configuration file (key mappings & color mappings)
PLAYER player[] =
{
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0xFF, 0x00, 0x00), VK_LEFT,	VK_DOWN		},
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0x00, 0xFF, 0x00), '1',		'Q'			},
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0x00, 0x00, 0xFF), 'Y',		'X'			},
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0xFF, 0xFF, 0x00), 'G',		'H'			},
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0xFF, 0x00, 0xFF), VK_LBUTTON,	VK_RBUTTON	},
	{{{-1,-1}, {-1,-1}}, 0.0f, -2.0f, 0, RGB(0x00, 0xFF, 0xFF), VK_NEXT,	VK_PRIOR	},
};

ULONG WINAPI BeepThread(LPVOID context)
{
	ULONG i;
	MSG msg;

	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if(msg.message == WM_USER)	// Player died
		{
			for(i = 555; i >= 222; i -= 111)
				Beep(i, 22);
		}

		else if(msg.message == WM_USER+1)	// Player won
		{
			for(i = 444; i <= 555; i += 111)
				Beep(i, 66);

			Sleep(22);

			for(i = 222; i <= 555; i += 111)
				Beep(i, 44);

			Sleep(44);

			for(i = 555; i <= 777; i += 111)
				Beep(i, 88);
		}

		else if(msg.message == WM_USER+2)	// Player start
		{
			for(i = 333; i <= 666; i += 111)
				Beep(i, 22);

			Sleep(22);

			for(i = 333; i <= 666; i += 111)
				Beep(i, 22);

			Sleep(22);

			for(i = 333; i <= 666; i += 111)
				Beep(i, 22);
		}

		else if(msg.message == WM_USER+3)	// Paused
		{
			Beep(333, 33);

			Sleep(66);

			Beep(666, 66);
		}

		else if(msg.message == WM_USER+4)	// Unpaused
		{
			Beep(666, 33);

			Sleep(66);

			Beep(333, 66);
		}
	}

	return 0;
}

BOOL ResetPlayers(HWND hWnd)
{
	ULONG i;

	for(i = 0; i < _countof(player); ++i)
	{
		// Reset player position
		player[i].Position[1][0] = player[i].Position[0][0] = (FLOAT)(100 + rand() % (width - 2 * 100));
		player[i].Position[1][1] = player[i].Position[0][1] = (FLOAT)(100 + rand() % (height - 2 * 100));

		// Reset player orientation
		player[i].Orientation = (FLOAT)(rand()/(DOUBLE)(RAND_MAX)*2*M_PI);

		// Reset player FSM state
		if(player[i].Life == -1)
			player[i].Life = 0.0f;
	}

	return TRUE;
}

BOOL CheckHit(HDC hDC, PFLOAT position)
{
	// Check 5x5 area around player position
	if(	GetPixel(hDC, (INT)position[0]+2, (INT)position[1]-1) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]+2, (INT)position[1]) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]+2, (INT)position[1]+1) != RGB(0,0,0) ||
		
		GetPixel(hDC, (INT)position[0]-2, (INT)position[1]-1) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]-2, (INT)position[1]) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]-2, (INT)position[1]+1) != RGB(0,0,0) ||

		GetPixel(hDC, (INT)position[0]-1, (INT)position[1]+2) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0], (INT)position[1]+2) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]+1, (INT)position[1]+2) != RGB(0,0,0) ||
		
		GetPixel(hDC, (INT)position[0]-1, (INT)position[1]-2) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0], (INT)position[1]-2) != RGB(0,0,0) ||
		GetPixel(hDC, (INT)position[0]+1, (INT)position[1]-2) != RGB(0,0,0))
		return TRUE;

	return FALSE;
}

VOID DrawPlayer(HDC hDC, PFLOAT position, COLORREF color)
{
	HGDIOBJ hOldPen = SelectObject(hDC, (HGDIOBJ)CreatePen(0, 5, color));

	MoveToEx(hDC, (INT)position[0], (INT)position[1], NULL);
	LineTo(hDC, (INT)position[0], (INT)position[1]);

	DeleteObject(SelectObject(hDC, hOldPen));
}

BOOL WinAnimate(HWND hWnd)
{
	ULONG i,j;
	RECT rect;

	// Do not animate if paused
	if(paused == 2)
		return TRUE;
	else if(paused > 0)
	{
		paused -= 0.02f;

		if(paused < 0.0f)
			paused = 0.0f;

		return TRUE;
	}

	if(state == -1)
	{
		// Just check for game join/disconnect keys
		ULONG ready = 0;

		for(i = 0; i < _countof(player); ++i)
		{
			if(HIWORD(GetAsyncKeyState(player[i].Left)))
				player[i].Life = -1.0f;

			if(HIWORD(GetAsyncKeyState(player[i].Right)))
				player[i].Life = -2.0f;

			ready += player[i].Life == -1.0f;
		}

		if(HIWORD(GetAsyncKeyState(VK_SPACE)) && ready > 1)
		{
			limit = 10 * (ready - 1);

			ResetPlayers(hWnd);
			state = 0.0f;

			PostThreadMessage(thread, WM_USER+2, 0, 0);
		}

		return TRUE;
	}

	if(state < 1.0f)
	{
		// State for player blinking, just advance time

		state += 0.02f;

		if(state > 1.0f)
			state = 1.0f;

		return TRUE;
	}

	if(state >= 2.0f && state < 3.0f)
	{
		// State for winner animation

		state += 0.02f;

		if(state > 3.0f)
			state = 3.0f;

		return TRUE;
	}

	if(state == 3.0f)
	{
		if(HIWORD(GetAsyncKeyState(VK_SPACE)))
		{
			// We are at the game over screen so switch to the intro screen and reset player scores

			for(i = 0; i < _countof(player); ++i)
			{
				player[i].Score = 0;
				player[i].Life = -2;
			}

			state = -1;
		}

		return TRUE;
	}

	if(HIWORD(GetAsyncKeyState(VK_SPACE)))
	{
		for(i = 0; i < _countof(player); ++i)
			if(player[i].Life >= 0.0f)
				break;

		// Check if all players are dead
		if(i == _countof(player))
		{
			ULONG max = 0;
			ULONG maxs = 1;

			for(i = 1; i < _countof(player); ++i)
			{
				if(player[i].Score > player[max].Score)
				{
					max = i;
					maxs = 1;
				}
				else if(player[i].Score == player[max].Score)
					++maxs;
			}

			if(player[max].Score >= limit && maxs > 1)
				limit = player[max].Score + 1;

			// A player won the game, display the winner screen
			if(player[max].Score >= limit)
			{
				limit = max;
				state = 2.0f;

				PostThreadMessage(thread, WM_USER+1, 0, 0);

				return TRUE;
			}

			// Else just reset the game for a nother round

			ResetPlayers(hWnd);
			state = 0.0f;

			PostThreadMessage(thread, WM_USER+2, 0, 0);

			return TRUE;
		}
	}

	// Update players according to input
	for(i = 0; i < _countof(player); ++i)
	{
		player[i].Position[1][0] = player[i].Position[0][0];
		player[i].Position[1][1] = player[i].Position[0][1];

		if(player[i].Life < 0.0f)
			continue;

		if(HIWORD(GetAsyncKeyState(player[i].Left)))
		{
			player[i].Orientation -= 0.05f;

			while(player[i].Orientation < 0.0f)
				player[i].Orientation += (FLOAT)(2*M_PI);
		}

		if(HIWORD(GetAsyncKeyState(player[i].Right)))
		{
			player[i].Orientation += 0.05f;

			while(player[i].Orientation > 2*M_PI)
				player[i].Orientation -= (FLOAT)(2*M_PI);
		}

		player[i].Life += 0.01f;
		if(player[i].Life > 1.55f)
			player[i].Life = 0.0f;
	
		player[i].Position[0][0] += cosf(player[i].Orientation) * 3.0f;
		player[i].Position[0][1] += sinf(player[i].Orientation) * 3.0f;
	}

	GetClientRect(hWnd,&rect);

	// Draw the players
	for(i = 0; i < _countof(player); ++i)
	{
		if(player[i].Life < 0.0f)
			continue;

		rect.left = (LONG)min(player[i].Position[0][0],player[i].Position[1][0]) - 3;
		rect.top = (LONG)min(player[i].Position[0][1],player[i].Position[1][1]) - 3;
		rect.right = (LONG)max(player[i].Position[0][0],player[i].Position[1][0]) + 3;
		rect.bottom = (LONG)max(player[i].Position[0][1],player[i].Position[1][1]) + 3;

		BitBlt(context[2],rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,context[0],rect.left,rect.top,SRCCOPY);
		BitBlt(context[1],rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,0,rect.left,rect.top,WHITENESS);
		DrawPlayer(context[1],player[i].Position[1],RGB(0,0,0));
		BitBlt(context[2],rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,context[1],rect.left,rect.top,SRCAND);
	
		// Check for collisions
		if(CheckHit(context[2],player[i].Position[0]))
		{
			player[i].Life = -1;

			PostThreadMessage(thread, WM_USER, 0, 0);

			for(j = 0; j < _countof(player); ++j)
				//if(player[j].Life >= 0.0f)
				if(j != i && (player[j].Position[1][0] != player[j].Position[0][0] || player[j].Position[1][1] != player[j].Position[0][1]))
					++player[j].Score;
		}
	}

	// Check if only one player is now left alive and end his suffering
	j = -1;

	for(i = 0; i < _countof(player); ++i)
		if(player[i].Life >= 0.0f)
			j = j == -1 ? i : -2;

	if(j != -1 && j != -2)
		player[j].Life = -1;

	return TRUE;
}

BOOL WinPaint(HWND hWnd)
{
	ULONG i;

	if(state == -1)
	{
		ULONG ready = 0;
		RECT rect = {0, height/2, width, 0};
		RECT text = {5, 5, 0, 0};

		BitBlt(context[0], 0, 0, width, height, 0, 0, 0, BLACKNESS);

		for(i = 0; i < _countof(player); ++i)
		{
			if(player[i].Life == -2.0f)
				continue;

			SetTextColor(context[0], player[i].Color);
			DrawText(context[0], TEXT("Ready"), -1, &text, DT_NOCLIP);

			text.top += font + 2;

			++ready;
		}

		if(ready >= 2)
		{
			rect.right = width - 5;
			rect.bottom = height - 5;

			SetTextColor(context[0], RGB(255, 255, 255));
			DrawText(context[0], TEXT("press 'Space' to start"), -1, &rect, DT_NOCLIP|DT_RIGHT|DT_BOTTOM|DT_SINGLELINE);
		}
		else if(ready == 0)
		{
			SetTextColor(context[0], RGB(255, 255, 255));
			DrawText(context[0], TEXT("Kurve\nby\nMarko Mihovilic"), -1, &rect, DT_NOCLIP|DT_CENTER);
		}

		return TRUE;
	}

	if(state >= 2.0f)
	{
		RECT rect = {width, height/2, 0, 0};
		BitBlt(context[0], 0, 0, width, height, 0, 0, 0, BLACKNESS);

		if((ULONG)((state - 2.0f) * 30.0f) % 2 == 0)
			SetTextColor(context[0],player[limit].Color);
		else
			SetTextColor(context[0],RGB(255, 255, 255));

		DrawText(context[0], TEXT("Winner"), -1, &rect, DT_NOCLIP|DT_SINGLELINE|DT_CENTER);

		rect.right = width - 5;
		rect.bottom = height - 5;

		SetTextColor(context[0], RGB(255, 255, 255));
		DrawText(context[0], TEXT("press 'Space' to continue"), -1, &rect, DT_NOCLIP|DT_RIGHT|DT_BOTTOM|DT_SINGLELINE);

		return TRUE;
	}

	if(state < 1.0f)
	{
		BitBlt(context[0], 0, 0, width, height, 0, 0, 0, BLACKNESS);

		for(i = 0; i < _countof(player); ++i)
		{
			if(player[i].Life >= 0.0f && (ULONG)(state * 20.0f) % 2 == 0)
			{
				HGDIOBJ oldpen;

				DrawPlayer(context[0], player[i].Position[0], player[i].Color);

				// Draw inital angle indicator
				oldpen = SelectObject(context[0], CreatePen(0, 2, player[i].Color));
				MoveToEx(context[0], (INT)player[i].Position[0][0], (INT)player[i].Position[0][1], NULL);
				LineTo(context[0], (INT)(player[i].Position[0][0] + 10.0f * cosf(player[i].Orientation)), (INT)(player[i].Position[0][1] + 10.0f * sinf(player[i].Orientation)));
				DeleteObject(SelectObject(context[0], oldpen));
			}
		}

		return TRUE;
	}

	for(i = 0; i < _countof(player); ++i)
		if(player[i].Life >= 0.0f && player[i].Life < 1.5f)
			DrawPlayer(context[0], player[i].Position[0], player[i].Color);

	for(i = 0; i < _countof(player); ++i)
		if(player[i].Life >= 0.0f)
			break;

	// Check if all players are dead
	if(i == _countof(player))
	{
		RECT rect = {5, 5, width - 5, height - 5};

		SetTextColor(context[0], RGB(255, 255, 255));
		DrawText(context[0], TEXT("press 'Space' to continue"), -1, &rect, DT_NOCLIP|DT_RIGHT|DT_BOTTOM|DT_SINGLELINE);
	}

	return TRUE;
}

LRESULT WINAPI WinProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ULONG i;
	PAINTSTRUCT paint;
	HDC hDC;

    switch(uMsg)
	{
        case WM_KEYDOWN:
			switch(wParam)
			{
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;

				case VK_PAUSE:
					if(paused == 2)
					{
						paused = 1;
						PostThreadMessage(thread, WM_USER+4, 0, 0);
					}
					else if(paused == 0 && state > 0.0f && state <= 1.0f)
					{
						paused = 2;
						PostThreadMessage(thread, WM_USER+3, 0, 0);
					}
					break;
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_TIMER:
			WinAnimate(hWnd);
			WinPaint(hWnd);

			InvalidateRect(hWnd, NULL, FALSE);
			break;

		case WM_PAINT:
			hDC = BeginPaint(hWnd, &paint);

			BitBlt(context[1], 0, 0, width, height, context[0], 0, 0, SRCCOPY);

			if(state >= 0.0f)
			{
				TCHAR score[32];
				RECT rect;

				rect.left = 5;
				rect.top = 5;

				for(i = 0; i < _countof(player); ++i)
				{
					if(player[i].Life == -2.0f)
						continue;

					SetTextColor(context[1], player[i].Color);

					wsprintf(score, TEXT("%d"), player[i].Score);
					DrawText(context[1], score, -1, &rect, DT_NOCLIP);

					rect.top += font + 2;
					
					if((player[i].Life > 1.5f || player[i].Life == -1) && state < 2.0f)
						DrawPlayer(context[1], player[i].Position[0], player[i].Color);
				}
			}

			// Display battery status
			{
				SYSTEM_POWER_STATUS power;

				GetSystemPowerStatus(&power);

				// Check if on battery
				if(power.ACLineStatus == 0 && power.BatteryLifePercent != 255)
				{
					TCHAR status[32];
					RECT rect = {5, 5, width - 5, 5};

					SetTextColor(context[1], RGB(100, 100, 100));
					wsprintf(status, TEXT("Battery %d%%"), power.BatteryLifePercent);
					DrawText(context[1], status, -1, &rect, DT_NOCLIP|DT_RIGHT);
				}
			}

			// Display pause message
			if(paused)
			{
				RECT rect = {0, 0, width, height};
				SetTextColor(context[1], (ULONG)(paused * 10) % 2 ? RGB(0, 0, 0) : RGB(255, 255, 255));
				DrawText(context[1], TEXT("PAUSED"), -1, &rect, DT_CENTER|DT_NOCLIP|DT_SINGLELINE|DT_VCENTER);
			}

			BitBlt(hDC, 0, 0, width, height, context[1], 0, 0, SRCCOPY);

			EndPaint(hWnd, &paint);
			break;

		case WM_SETCURSOR:
			SetCursor(NULL);
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	WNDCLASSEX wndClass;
	HWND hWnd;
	HDC hDC;
	MSG msg;

	srand(GetTickCount());

	ZeroMemory(&wndClass,sizeof(wndClass));

	wndClass.lpszClassName	= TEXT("KURVE_WINDOW_CLASS");
	wndClass.cbSize			= sizeof(wndClass);
	wndClass.style			= CS_CLASSDC;
	wndClass.lpfnWndProc	= WinProcedure;
	wndClass.hInstance		= hInstance;
	//wndClass.hIcon		= LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_MAIN));
    //wndClass.hIconSm		= LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_MAIN));
	//wndClass.hCursor		= LoadCursor(NULL,IDC_ARROW);

	if(!RegisterClassEx(&wndClass))
	{
		MessageBox(NULL, TEXT("Failed to register window class."), TEXT("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	
	hWnd = CreateWindowEx(0, wndClass.lpszClassName, TEXT("Kurve"), WS_POPUP, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	if(!hWnd)
	{
		MessageBox(NULL, TEXT("Failed to create main game window."), TEXT("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	hDC = GetDC(hWnd);

	context[0] = CreateCompatibleDC(hDC);
	context[1] = CreateCompatibleDC(hDC);
	context[2] = CreateCompatibleDC(hDC);
	if(!context[0] || !context[1] || !context[2])
	{
		MessageBox(hWnd, TEXT("Failed to create offscreen device contexts."), TEXT("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	bitmap[0] = CreateCompatibleBitmap(hDC, width, height);
	bitmap[1] = CreateCompatibleBitmap(hDC, width, height);
	bitmap[2] = CreateCompatibleBitmap(hDC, width, height);
	if(!bitmap[0] || !bitmap[1] || !bitmap[2])
	{
		MessageBox(NULL, TEXT("Failed to create offscreen buffers."), TEXT("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	SelectObject(context[0], bitmap[0]);
	SelectObject(context[1], bitmap[1]);
	SelectObject(context[2], bitmap[2]);
	
	ReleaseDC(hWnd,hDC);

	SetBkMode(context[0], TRANSPARENT);
	SetBkMode(context[1], TRANSPARENT);
	SetBkMode(context[2], TRANSPARENT);

	DeleteObject(SelectObject(context[0], CreateFont(font, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, 0)));
	DeleteObject(SelectObject(context[1], CreateFont(font, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, 0)));
	DeleteObject(SelectObject(context[1], CreateFont(font, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, 0)));

	//DeleteObject(SelectObject(context[0], GetStockObject(DEFAULT_GUI_FONT)));
	//DeleteObject(SelectObject(context[1], GetStockObject(DEFAULT_GUI_FONT)));
	//DeleteObject(SelectObject(context[2], GetStockObject(DEFAULT_GUI_FONT)));

	if(lstrcmpiA(lpCmdLine, "-nosound"))
		CloseHandle(CreateThread(NULL, 0, BeepThread, NULL, 0, &thread));

	ShowWindow(hWnd, SW_SHOW);
	SetTimer(hWnd, 0, 1000/60, NULL);

	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	PostThreadMessage(thread, WM_QUIT, 0, 0);

	DestroyWindow(hWnd);

	DeleteObject(bitmap[0]);
	DeleteObject(bitmap[1]);
	DeleteObject(bitmap[2]);

	DeleteDC(context[0]);
	DeleteDC(context[1]);
	DeleteDC(context[2]);

	UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);

	return (INT)msg.wParam;
}