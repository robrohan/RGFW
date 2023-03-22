/*
* Copyright (C) 2023 ColeagueRiley
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*
*
*/

/*
	Credits :
		EimaMei/Sacode : Much of the code for creating windows using winapi
*/

#define RGFW_TRANSPARENT_WINDOW		(1L<<0)
#define RGFW_NO_BOARDER		(1L<<1)
#define RGFW_NO_RESIZE		(1L<<1)

#define RGFW_keyPressed 2 /*!< a key has been pressed*/
#define RGFW_keyReleased 3 /*!< a key has been released*/
#define RGFW_mouseButtonPressed 4 /*!< a mouse button has been pressed (left,middle,right)*/
#define RGFW_mouseButtonReleased 5 /*!< a mouse button has been released (left,middle,right)*/
#define RGFW_mousePosChanged 6 /*!< the position of the mouse has been changed*/
#define RGFW_quit 33 /*!< the user clicked the quit button*/
#define RGFW_dnd 34 /*!< a file has been dropped into the window*/

#define RGFW_mouseLeft  1 /*!< left mouse button is pressed*/
#define RGFW_mouseMiddle  2 /*!< mouse-wheel-button is pressed*/
#define RGFW_mouseRight  3 /*!< right mouse button is pressed*/
#define RGFW_mouseScrollUp  4 /*!< mouse wheel is scrolling up*/
#define RGFW_mouseScrollDown  5 /*!< mouse wheel is scrolling down*/


#ifdef __cplusplus
extern "C" {
#endif

typedef struct RGFW_Event {
    int type; /*!< which event has been sent?*/
    int button; /*!< which mouse button has been clicked (0) left (1) middle (2) right*/
    int x, y;

    int ledState; /*!< 0 : numlock, 1 : caps lock, 3 : small lock*/

    int keyCode; /* keycode of event*/

	#ifdef _WIN32
	char keyName[16]; /* key name of event*/
	#else
	char* keyName;
	#endif

    char** droppedFiles; /*!< dropped files*/
} RGFW_Event;

typedef struct RGFW_window {
    void* display;
    void* window;
    void* glWin;

	char* name; /* window's name*/
	int x, y, w, h; /* window size, x, y*/

	int srcX, srcY, srcW, srcH;
	char* srcName;

	RGFW_Event event;
} RGFW_window;

RGFW_window RGFW_createWindow(char* name, int x, int y, int w, int h, unsigned long args);
RGFW_window* RGFW_createWindowPointer(char* name, int x, int y, int w, int h, unsigned long args);
RGFW_Event RGFW_checkEvents(RGFW_window* window);

int RGFW_isPressedI(RGFW_window* window, int key);
int RGFW_isPressedS(RGFW_window* window, char* key);

void RGFW_closeWindow(RGFW_window* window);

void RGFW_makeCurrent(RGFW_window* window);

void RGFW_clear(RGFW_window* w, int r, int g, int b, int a);

void RGFW_setDrawBuffer(int buffer);

#ifdef RGFW_IMPLEMENTATION

#ifdef __linux__

#include <GL/glx.h>
#include <X11/XKBlib.h>
#include <stdlib.h>

RGFW_window* RGFW_createWindowPointer(char* name, int x, int y, int w, int h, unsigned long args){
	int singleBufferAttributes[] = {4, 8, 8, 9, 8, 10, 8, None};

	int doubleBufferAttributes[] = {4, 5, 8, 8, 9, 8, 10, 8, None};

    RGFW_window* nWin = (RGFW_window*)malloc(sizeof(RGFW_window));

	nWin->srcX = nWin->x = x;
	nWin->srcY = nWin->y = y;
	nWin->srcW = nWin->w = w;
	nWin->srcH = nWin->h = h;

	nWin->srcName = nWin->name = name;

    XInitThreads(); /* init X11 threading*/

    /* init the display*/
	nWin->display = XOpenDisplay(0);
	if ((Display *)nWin->display == NULL)
		return nWin;

    long event_mask =  KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask; /* X11 events accepted*/

    XVisualInfo *vi = glXChooseVisual((Display *)nWin->display, DefaultScreen((Display *)nWin->display), doubleBufferAttributes);

    if (vi == NULL) /* switch to single buffer if double buffer fails*/
        vi = glXChooseVisual((Display *)nWin->display, DefaultScreen((Display *)nWin->display), singleBufferAttributes);

    if (RGFW_TRANSPARENT_WINDOW & args)
        XMatchVisualInfo((Display *)nWin->display, DefaultScreen((Display *)nWin->display), 32, TrueColor, vi); /* for RGBA backgrounds*/

    Colormap cmap = XCreateColormap((Display *)nWin->display, RootWindow((Display *)nWin->display, vi->screen),
                                    vi->visual, AllocNone); /* make the cmap from the visual*/

    nWin->glWin = (void*)glXCreateContext((Display *)nWin->display, vi, 0, 1); /* create the GLX context with the visual*/

    /* make X window attrubutes*/
    XSetWindowAttributes swa;

    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = event_mask;

    /* create the window*/
    nWin->window = (void*)XCreateWindow((Display *)nWin->display, RootWindow((Display *)nWin->display, vi->screen), x, y, w, h,
                        0, vi->depth, InputOutput, vi->visual,
                        (1L << 1) | (1L << 13) | CWBorderPixel | CWEventMask, &swa);

    if (RGFW_NO_RESIZE & args){ /* make it so the user can't resize the window*/
        XSizeHints *sh = XAllocSizeHints();
        sh->flags = (1L << 4) | (1L << 5);
        sh->min_width = sh->max_width = w;
        sh->min_height = sh->max_height = h;

        XSetWMSizeHints((Display *)nWin->display, (GLXDrawable)nWin->window, sh, ((Atom)40));
    }


    if (RGFW_NO_BOARDER & args){
        /* Atom vars for no-border*/
        Atom window_type = XInternAtom((Display *)nWin->display, "_NET_WM_WINDOW_TYPE", False);
        Atom value = XInternAtom((Display *)nWin->display, "_NET_WM_WINDOW_TYPE_DOCK", False);

        XChangeProperty((Display *)nWin->display, (GLXDrawable)nWin->window, window_type, ((Atom)4), 32, PropModeReplace, (unsigned char *)&value, 1); /* toggle border*/
    }

    XSelectInput((Display *)nWin->display, (GLXDrawable)nWin->window, swa.event_mask); /* tell X11 what events we want*/

    /* make it so the user can't close the window until the program does*/
    Atom wm_delete = XInternAtom((Display *)nWin->display, "WM_DELETE_WINDOW", 1);
    XSetWMProtocols((Display *)nWin->display, (GLXDrawable)nWin->window, &wm_delete, 1);

    /* connect the context to the window*/
    glXMakeCurrent((Display *)nWin->display, (GLXDrawable)nWin->window, (GLXContext)nWin->glWin);

    /* set the background*/
    XStoreName((Display *)nWin->display, (GLXDrawable) nWin->window, name); /* set the name*/

    glEnable(0x0BE2);			 /* Enable blending.*/
    glBlendFunc(0x0302, 0x0303); /* Set blending function*/

    XMapWindow((Display *)nWin->display, (GLXDrawable)nWin->window);						  /* draw the window*/
    XMoveWindow((Display *)nWin->display, (GLXDrawable)nWin->window, x, y); /* move the window to it's proper cords*/

    return nWin;
}

RGFW_Event RGFW_checkEvents(RGFW_window* win){
    RGFW_Event event;
	XEvent E; /* raw X11 event*/

	/* if there is no unread qued events, get a new one*/
	if (XEventsQueued((Display *)win->display, QueuedAlready) + XEventsQueued((Display *)win->display, QueuedAfterReading))
		XNextEvent((Display *)win->display, &E);

	/* vars for getting the mouse x/y with query*/
	int x, y, i;
	unsigned m, m2;
	Window root, child;

	XWindowAttributes a;

	if (E.type == KeyPress || E.type == KeyRelease){
		/* set event key data*/
		event.keyCode = XkbKeycodeToKeysym((Display *)win->display, E.xkey.keycode, 0, E.xkey.state & ShiftMask ? 1 : 0); /* get keysym*/
		event.keyName = XKeysymToString(event.keyCode); /* convert to string */
		/*translateString(event.keyName, 0);*/
	}

	switch (E.type) {
		case KeyPress:
			event.type = RGFW_keyPressed;
			break;

		case KeyRelease:
			/* check if it's a real key release*/
			if (XEventsQueued((Display *)win->display, QueuedAfterReading)) { /* get next event if there is one*/
				XEvent NE;
				XPeekEvent((Display *)win->display, &NE);

				if (E.xkey.time == NE.xkey.time && E.xkey.keycode == NE.xkey.keycode) /* check if the current and next are both the same*/
					break;
			}

			event.type = RGFW_keyReleased;
			break;

		case ButtonPress:
			event.button = event.button;
			event.type = RGFW_mouseButtonPressed;
			break;

		case ButtonRelease:
			event.type = RGFW_mouseButtonReleased;
			event.button = event.button;
			break;

		case MotionNotify:
			/* if the x/y changes, fetch the x/y with query*/
			if (XQueryPointer((Display *)win->display, (GLXDrawable)win->window, &root, &child, &x, &y, &x, &y, &m)){
				event.x = x;
				event.y = y;
			}
			event.type = RGFW_mousePosChanged;
			break;

		case ClientMessage:
			/* if the client closed the window*/
			if (E.xclient.data.l[0] == (long int)XInternAtom((Display *)win->display, "WM_DELETE_WINDOW", 1)){
				event.type = RGFW_quit;
			}
			else
				XFlush((Display *)win->display);
			break;

		default:
			XFlush((Display *)win->display);
			event.type = 0;
			break;
	}

	/* if the key has a weird name, change it*/
	XKeyboardState keystate;
	XGetKeyboardControl((Display *)win->display, &keystate);
	event.ledState = keystate.led_mask;

	if ((win->srcX != win->x) || (win->srcY != win->y)){
		XMoveWindow((Display *)win->display, (GLXDrawable)win->window, win->x, win->y);
		win->srcX = win->x;
		win->srcY = win->y;
	}

	else if ((win->srcW != win->w) || (win->srcH != win->h)){
		XResizeWindow((Display *)win->display, (GLXDrawable)win->window, (GLXDrawable)win->w, win->h);
		win->srcW = win->w;
		win->srcH = win->h;
	} else {
		/* make sure the window attrubutes are up-to-date*/
		XGetWindowAttributes((Display *)win->display, (GLXDrawable)win->window, &a);

		win->srcX = win->x = a.x;
		win->srcY = win->y = a.y;
		win->srcW = win->w = a.width;
		win->srcH =win->h = a.height;
	}

	if (win->srcName != win->name){
		XStoreName((Display *)win->display, (Window)win->window, win->name);
		win->srcName = win->name;
	}

	win->event = event;

	return event;
}

void RGFW_closeWindow(RGFW_window* win){
	XDestroyWindow((Display *)win->display, (GLXDrawable)win->window); /* close the window*/
	XCloseDisplay((Display *)win->display);	   /* kill the display*/
}

int RGFW_isPressedI(RGFW_window* w, int key){
	char keyboard[32];
	XQueryKeymap((Display *)w->display, keyboard); /* query the keymap */

	KeyCode kc2 = XKeysymToKeycode((Display *)w->display, key); /* convert the key to a keycode */
	return !!(keyboard[kc2 >> 3] & (1 << (kc2 & 7)));				/* check if the key is pressed */
}

int RGFW_isPressedS(RGFW_window* w, char* key){ return RGFW_isPressedI(w, XStringToKeysym(key)); }
#endif

#ifdef _WIN32
#include <ole2.h>
#include <GL/gl.h>
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407

RGFW_window* RGFW_createWindowPointer(char* name, int x, int y, int w, int h, unsigned long args){
    RGFW_window* nWin = (RGFW_window*)malloc(sizeof(RGFW_window));
    int         pf;
	WNDCLASS    wc;

	nWin->srcX = nWin->x = x;
	nWin->srcY = nWin->y = y;
	nWin->srcW = nWin->w = w;
	nWin->srcH = nWin->h = h;

	nWin->srcName = nWin->name = name;

	HINSTANCE inh = GetModuleHandle(NULL);

	WNDCLASSA Class = {0}; /* Setup the Window class. */
	Class.lpszClassName = name;
	Class.hInstance = inh;
	Class.hCursor = LoadCursor(NULL, IDC_ARROW);
	Class.lpfnWndProc = DefWindowProc;
	RegisterClassA(&Class);

	RegisterClass(&wc);

	LPDROPTARGET target;
	RegisterDragDrop((HWND)nWin->display, target);

	DWORD window_style = WS_MAXIMIZEBOX | WS_MINIMIZEBOX | window_style;

	if (!(RGFW_NO_BOARDER & args))
		window_style |= WS_CAPTION | WS_SYSMENU | WS_BORDER;
	else
		window_style |= WS_POPUP | WS_VISIBLE;

	if (!(RGFW_NO_RESIZE & args))
		window_style |= WS_SIZEBOX;
	if (RGFW_TRANSPARENT_WINDOW & args)
		SetWindowLong((HWND)nWin->display, GWL_EXSTYLE, GetWindowLong((HWND)nWin->display, GWL_EXSTYLE) | WS_EX_LAYERED);

    nWin->display = CreateWindowA(name, name, window_style, x, y, w, h, NULL, NULL, inh, NULL);


    nWin->window = GetDC((HWND)nWin->display);

    /* there is no guarantee that the contents of the stack that become
       the pfd are zeroed, therefore _make sure_ to clear these bits. */
    PIXELFORMATDESCRIPTOR pfd = {0}; /* Setup OpenGL. */
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;

    pf = ChoosePixelFormat((HDC)nWin->window, &pfd);

    SetPixelFormat((HDC)nWin->window, pf, &pfd);

    DescribePixelFormat((HDC)nWin->window, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	if (RGFW_TRANSPARENT_WINDOW & args)
		SetWindowLong((HWND)nWin->display, GWL_EXSTYLE, GetWindowLong((HWND)nWin->display, GWL_EXSTYLE) | WS_EX_LAYERED);

    ReleaseDC((HWND)nWin->display, (HDC)nWin->window);

    nWin->window = GetDC((HWND)nWin->display);
    nWin->glWin = wglCreateContext((HDC)nWin->window);
    wglMakeCurrent((HDC)nWin->window, (HGLRC)nWin->glWin);
    ShowWindow((HWND)nWin->display, SW_SHOWNORMAL);

    return nWin;
}

RGFW_Event RGFW_checkEvents(RGFW_window* win){
	MSG msg = {};

	int setButton = 0;

	while (PeekMessage(&msg, (HWND)win->display, 0u, 0u, PM_REMOVE)) {
		switch (msg.message) {
			case WM_CLOSE:
			case WM_QUIT:
				win->event.type = RGFW_quit;
				break;

			case WM_KEYUP:
				win->event.keyCode = msg.wParam;
				GetKeyNameTextA(msg.lParam, win->event.keyName, 16);
				win->event.type = RGFW_keyReleased;
				break;

			case WM_KEYDOWN:
				win->event.keyCode = msg.wParam;
				GetKeyNameTextA(msg.lParam, win->event.keyName, 16);
				win->event.type = RGFW_keyPressed;
				break;

			case WM_MOUSEMOVE:
				win->event.x = msg.pt.x - win->x;
				win->event.x = msg.pt.x - win->x;
				win->event.type = RGFW_mousePosChanged;
				break;

			case WM_LBUTTONDOWN:
				win->event.button = RGFW_mouseLeft;
				win->event.type = RGFW_mouseButtonPressed;
				break;
			case WM_RBUTTONDOWN:
				win->event.button = RGFW_mouseRight;
				win->event.type = RGFW_mouseButtonPressed;
				break;
			case WM_MBUTTONDOWN:
				win->event.button = RGFW_mouseMiddle;
				win->event.type = RGFW_mouseButtonPressed;
				break;

			case WM_MOUSEWHEEL:
				if (msg.wParam > 0)
					win->event.button = RGFW_mouseScrollUp;
				else
					win->event.button = RGFW_mouseScrollDown;

				win->event.type = RGFW_mouseButtonPressed;
				break;

			case WM_LBUTTONUP:
				win->event.button = RGFW_mouseLeft;
				win->event.type = RGFW_mouseButtonReleased;
				break;
			case WM_RBUTTONUP:
				win->event.button = RGFW_mouseRight;
				win->event.type = RGFW_mouseButtonReleased;
				break;
			case WM_MBUTTONUP:
				win->event.button = RGFW_mouseMiddle;
				win->event.type = RGFW_mouseButtonReleased;
				break;

			case WM_DROPFILES:
				win->event.type = RGFW_dnd;
				break;
			default: break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if ((win->srcX != win->x) || (win->srcY != win->y) || (win->srcW != win->w) || (win->srcH != win->h)){
		SetWindowPos((HWND)win->display, (HWND)win->display, win->x, win->y, win->w, win->h, 0);
		win->srcX = win->x;
		win->srcY = win->y;
		win->srcW = win->w;
		win->srcH = win->h;
	} else {
		/* make sure the window attrubutes are up-to-date*/
		RECT a;

		if (GetWindowRect((HWND)win->display, &a)){
			win->srcX = win->x = a.left;
			win->srcY = win->y = a.top;
			win->srcW = win->w = a.right - a.left;
			win->srcH = win->h = a.bottom - a.top;
		}
	}

	if (win->srcName != win->name){
		SetWindowTextA((HWND)win->display, win->name);
		win->srcName = win->name;
	}

	win->event.keyCode = 0;

	if ((GetKeyState(VK_CAPITAL) & 0x0001)!=0)
		win->event.keyCode |= 1;
	if ((GetKeyState(VK_NUMLOCK) & 0x0001)!=0)
		win->event.keyCode |= 2;
	if ((GetKeyState(VK_SCROLL) & 0x0001)!=0)
		win->event.keyCode |= 3;


	if (!IsWindow((HWND)win->display))
		win->event.type = RGFW_quit;

	return win->event;
}

int RGFW_isPressedI(RGFW_window* window, int key){
	return (GetAsyncKeyState(key) & 0x8000);
}

int RGFW_isPressedS(RGFW_window* window, char* key){
	int vKey = VkKeyScan(key[0]);

	if (sizeof(key)/sizeof(char) > 1){
		if (strcmp(key, "Super_L") == 0) vKey = VK_LWIN;
		else if (strcmp(key, "Super_R") == 0) vKey = VK_RWIN;
		else if (strcmp(key, "Space") == 0) vKey = VK_SPACE;
		else if (strcmp(key, "Return") == 0) vKey = VK_RETURN;
		else if (strcmp(key, "Caps_Lock") == 0) vKey = VK_CAPITAL;
		else if (strcmp(key, "Tab") == 0) vKey = VK_TAB;
		else if (strcmp(key, "Right") == 0) vKey = VK_RIGHT;
		else if (strcmp(key, "Left") == 0) vKey = VK_LEFT;
		else if (strcmp(key, "Up") == 0) vKey = VK_UP;
		else if (strcmp(key, "Down") == 0) vKey = VK_DOWN;
		else if (strcmp(key, "Shift") == 0)
			return RGFW_isPressedI(window, VK_RSHIFT) || RGFW_isPressedI(window, VK_LSHIFT);
		else if (strcmp(key, "Shift") == 0)
			return RGFW_isPressedI(window, VK_RMENU) || RGFW_isPressedI(window, VK_LMENU);
		else if (strcmp(key, "Control") == 0)
			return RGFW_isPressedI(window, VK_RCONTROL) || RGFW_isPressedI(window, VK_LCONTROL);
	}

	return RGFW_isPressedI(window, vKey);
}

void RGFW_closeWindow(RGFW_window* win) {
	wglDeleteContext((HGLRC)win->glWin); /* delete opengl context */
	DeleteDC((HDC)win->window); /* delete window */
	DestroyWindow((HWND)win->display); /* delete display */
}

#endif

#ifdef __APPLE__
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407
#include <OpenGL/gl.h>
#include <Carbon/Carbon.h>
RGFW_window* RGFW_createWindowPointer(char* name, int x, int y, int w, int h, unsigned long args){
	RGFW_window* nWin = malloc(sizeof(RGFW_window));

	nWin->srcX = nWin->x = x;
	nWin->srcY = nWin->y = y;
	nWin->srcW = nWin->w = w;
	nWin->srcH = nWin->h = h;

	return nWin;
}

#endif

RGFW_window RGFW_createWindow(char* name, int x, int y, int w, int h, unsigned long args){ return *RGFW_createWindowPointer(name, x, y, w, h, args); }

void RGFW_makeCurrent(RGFW_window* w){
    #ifdef __linux__
		glXMakeCurrent((Display *)w->display, (GLXDrawable)w->window, (GLXContext)w->glWin);
	#endif
	#ifdef _WIN32
		wglMakeCurrent((HDC)w->window, (HGLRC)w->glWin);
	#endif	
}

void RGFW_setDrawBuffer(int buffer){
    if (buffer != GL_FRONT && buffer != GL_BACK && buffer != GL_LEFT && buffer != GL_RIGHT)
        return;

    /* Set the draw buffer using the specified value*/
    glDrawBuffer(buffer);
}

void RGFW_clear(RGFW_window* w, int r, int g, int b, int a){
	RGFW_makeCurrent(w);

    glFlush(); /* flush the window*/

    int currentDrawBuffer;

    glGetIntegerv(GL_DRAW_BUFFER, &currentDrawBuffer);
    RGFW_setDrawBuffer((currentDrawBuffer == GL_BACK) ? GL_FRONT : GL_BACK); /* Set draw buffer to be the other buffer

    /* clear the window*/
    glClearColor(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    glClear(0x00004000);
}
#endif /*RGFW_IMPLEMENTATION*/

#ifdef __cplusplus
}
#endif