// 7 april 2015
#include "uipriv_windows.h"

// TODOs
// - [12:24] <ZeroOne> There's flickering between tabs
// - with CTLCOLOR handler: [12:24] <ZeroOne> And setting the button text blanked out the entire GUI until I ran my mouse over the elements / [12:25] <ZeroOne> https://dl.dropboxusercontent.com/u/15144168/GUI%20stuff.png / [12:41] <ZeroOne> https://dl.dropboxusercontent.com/u/15144168/stack.png here have another screenshot
// 	- I get this too
// - without CTLCOLOR handler: [12:33] <ZeroOne> If I hide the stack, then show it, it looks like it's drawing duplicate buttons underneath

/*
all container windows (including the message-only window, hence this is not in container_windows.c) have to call the sharedWndProc() to ensure messages go in the right place and control colors are handled properly
*/

/*
all controls that have events receive the events themselves through subclasses
to do this, all container windows (including the message-only window; see http://support.microsoft.com/default.aspx?scid=KB;EN-US;Q104069) forward WM_COMMAND to each control with this function, WM_NOTIFY with forwardNotify, etc.
*/
static LRESULT forwardCommand(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND control = (HWND) lParam;

	// don't generate an event if the control (if there is one) is unparented (a child of the initial parent window)
	if (control != NULL && IsChild(initialParent, control) == 0)
		return SendMessageW(control, msgCOMMAND, wParam, lParam);
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static LRESULT forwardNotify(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NMHDR *nmhdr = (NMHDR *) lParam;
	HWND control = nmhdr->hwndFrom;

	// don't generate an event if the control (if there is one) is unparented (a child of the initial parent window)
	if (control != NULL && IsChild(initialParent, control) == 0)
		return SendMessageW(control, msgNOTIFY, wParam, lParam);
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static void paintControlBackground(HWND hwnd, HDC dc)
{
	HWND parent;
	RECT r;
	POINT pOrig;
	DWORD le;

	parent = hwnd;
	for (;;) {
		parent = GetParent(parent);
		if (parent == NULL)
			logLastError("error getting parent control of control in paintControlBackground()");
		// wine sends these messages early, yay...
		if (parent == initialParent)
			return;
		// skip groupboxes; they're (supposed to be) transparent
		if (windowClassOf(parent, L"button", NULL) != 0)
			break;
	}
	if (GetWindowRect(hwnd, &r) == 0)
		logLastError("error getting control's window rect in paintControlBackground()");
	// the above is a window rect in screen coordinates; convert to client rect
	SetLastError(0);
	if (MapWindowRect(NULL, parent, &r) == 0) {
		le = GetLastError();
		SetLastError(le);		// just to be safe
		if (le != 0)
			logLastError("error getting client origin of control in paintControlBackground()");
	}
	if (SetWindowOrgEx(dc, r.left, r.top, &pOrig) == 0)
		logLastError("error moving window origin in paintControlBackground()");
	SendMessageW(parent, WM_PRINTCLIENT, (WPARAM) dc, PRF_CLIENT);
	if (SetWindowOrgEx(dc, pOrig.x, pOrig.y, NULL) == 0)
		logLastError("error resetting window origin in paintControlBackground()");
}

BOOL sharedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	switch (uMsg) {
	case WM_COMMAND:
		*lResult = forwardCommand(hwnd, uMsg, wParam, lParam);
		return TRUE;
	case WM_NOTIFY:
		*lResult = forwardNotify(hwnd, uMsg, wParam, lParam);
		return TRUE;
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
/*TODO		// read-only TextFields and Textboxes are exempt
		// this is because read-only edit controls count under WM_CTLCOLORSTATIC
		if (windowClassOf((HWND) lParam, L"edit", NULL) == 0)
			if (textfieldReadOnly((HWND) lParam))
				return FALSE;
*/		if (SetBkMode((HDC) wParam, TRANSPARENT) == 0)
			logLastError("error setting transparent background mode to controls in sharedWndProc()");
		paintControlBackground((HWND) lParam, (HDC) wParam);
		*lResult = (LRESULT) hollowBrush;
		return TRUE;
	}
	return FALSE;
}

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing and https://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
// this X value is really only for buttons but I don't see a better one :/
#define winXPadding 4
#define winYPadding 4

void resize(uiControl *control, HWND parent, RECT r, RECT margin)
{
	uiSizing d;
	uiSizingSys sys;
	HDC dc;
	HFONT prevfont;
	TEXTMETRICW tm;
	SIZE size;

	size.cx = 0;
	size.cy = 0;
	ZeroMemory(&tm, sizeof (TEXTMETRICW));
	dc = GetDC(parent);
	if (dc == NULL)
		logLastError("error getting DC in resize()");
	prevfont = (HFONT) SelectObject(dc, hMessageFont);
	if (prevfont == NULL)
		logLastError("error loading control font into device context in resize()");
	if (GetTextMetricsW(dc, &tm) == 0)
		logLastError("error getting text metrics in resize()");
	if (GetTextExtentPoint32W(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &size) == 0)
		logLastError("error getting text extent point in resize()");
	sys.baseX = (int) ((size.cx / 26 + 1) / 2);
	sys.baseY = (int) tm.tmHeight;
	sys.internalLeading = tm.tmInternalLeading;
	if (SelectObject(dc, prevfont) != hMessageFont)
		logLastError("error restoring previous font into device context in resize()");
	if (ReleaseDC(parent, dc) == 0)
		logLastError("error releasing DC in resize()");
	r.left += uiDlgUnitsToX(margin.left, sys.baseX);
	r.top += uiDlgUnitsToY(margin.top, sys.baseY);
	r.right -= uiDlgUnitsToX(margin.right, sys.baseX);
	r.bottom -= uiDlgUnitsToY(margin.bottom, sys.baseY);
	d.xPadding = uiDlgUnitsToX(winXPadding, sys.baseX);
	d.yPadding = uiDlgUnitsToY(winYPadding, sys.baseY);
	d.sys = &sys;
	uiControlResize(control, r.left, r.top, r.right - r.left, r.bottom - r.top, &d);
}

void updateParent(uintptr_t h)
{
	HWND hwnd;

	if (h == 0)		// no parent
		return;
	hwnd = (HWND) h;
	SendMessageW(hwnd, msgUpdateChild, 0, 0);
}
