#include <windows.h>

#include "hooks.h"
#include "config.hpp"
#include "util.hpp"
#include "shared.hpp"

/* The handle of this instance of the DLL, set in DllMain.
 */
HINSTANCE dllHandle = NULL;

/* The entry point for the DLL. Stores the instance handle of the DLL for later use.
 */
BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved) {
	if (dwReason == DLL_PROCESS_ATTACH || dwReason == DLL_THREAD_ATTACH) {
		dllHandle = (HINSTANCE)hinstDLL;
	} else if (dwReason == DLL_PROCESS_DETACH || dwReason == DLL_THREAD_DETACH) {
		// Nothing to do for the moment.
	}
	return TRUE;
}

/* Initializes the DLL by storing the ID of the thread that will hook up the event hooks.
 * On successful execution, the thread ID is stored and the function returns NULL.
 * If there was already another thread ID (probably another taekwindow.exe process),
 * the function returns the thread ID of that thread and does nothing else.
 */
DWORD __declspec(dllexport) __stdcall init(DWORD threadId) {
	if (mainThreadId) {
		return mainThreadId;
	} else {
		mainThreadId = threadId;
		return NULL;
	}
}

/* Returns true if the given window can be moved and resized.
 * Prevents moving/resizing of maximized windows. Other cases may be added later.
 */
bool isDraggableWindow(HWND window) {
	WINDOWINFO info;
	info.cbSize = sizeof(WINDOWINFO);
	GetWindowInfo(window, &info);
	if (info.dwStyle & WS_MAXIMIZE) {
		return false;
	} else {
		return true;
	}
}

/* Sets the variables resizingX and resizingY to the proper values,
 * considering the screen-coordinate pointer location.
 */
void setResizingX(POINT const &pt) {
	resizingX = (pt.x - lastRect.left) * 3 / (lastRect.right - lastRect.left) - 1;
}
void setResizingY(POINT const &pt) {
	resizingY = (pt.y - lastRect.top) * 3 / (lastRect.bottom - lastRect.top) - 1;
}

/* Processes a button-down event.
 * Returns true if the event should not be passed on to the application, false otherwise.
 */
bool processButtonDown(MouseButton button, MOUSEHOOKSTRUCT *eventInfo) {
	if (currentState == dsNone) {
		// Nothing is yet going on. We possibly want to take action if the Modifier key is currently pressed.
		if (modifierDown) {
			if (button == mbLeft || button == mbRight) {
				// Yippee! A Modifier-drag event just started that we want to process.
				currentState = dsDragging;
				haveDragged = true;
				draggingButton = button;
				// Find the actual window being dragged: this is the top-level window that is the ultimate parent
				// of the window receiving the event. Seems to work for MDI's too.
				draggedWindow = GetAncestor(eventInfo->hwnd, GA_ROOT);
				if (isDraggableWindow(draggedWindow)) {
					// Window can be dragged.
					// Capture the mouse so it'll still get events even if the mouse leaves the window
					// (could happen while resizing).
					SetCapture(draggedWindow);
					GetWindowRect(draggedWindow, &lastRect);
					if (button == mbRight) {
						// Figure out in which area we're dragging to resize in the proper direction.
						setResizingX(eventInfo->pt);
						setResizingY(eventInfo->pt);
					}
				} else {
					// Modifier-dragging an invalid window. The user won't expect her actions to be passed
					// to that window, so we suppress all events until the mouse button is released.
					currentState = dsIgnoring;
				}
				// Either way, we eat the event.
				return true;
			} else {
				// Mouse-down event with a button we don't handle. Stay away from it.
				return false;
			}
		} else {
			// Modifier was up, so we keep our hands off this event.
			return false;
		}
	} else {
		// We're already dragging, and another button was pressed.
		// Naughty user shouldn't do this, but we pass the event anyway, because otherwise the application
		// might receive a mouse-up event without a preceding mouse-down event and get all confused.
		return false;
	}
}

/* Processes a mouse button release event.
 * Returns true if the event was processed and should not be passed to the application, false otherwise.
 */
bool processButtonUp(MouseButton button) {
	switch (currentState) {
		case dsNone:
			// Nothing going on, pass the event on.
			return false;
		case dsDragging:
			if (button == draggingButton) {
				// End of move or resize action.
				// Release the capture and eat the event.
				ReleaseCapture();
				currentState = dsNone;
				return true;
			} else {
				// Other button released during move event. (Naughty user!)
				return false;
			}
		case dsIgnoring:
			// Ignoring all events until the appropriate button was released.
			if (button == draggingButton) {
				// Well, guess what, it was!
				// But we still ignore this up event, of course.
				currentState = dsNone;
			}
			return true;
		default:
			// Should never be reached unless the currentState enum was tortured by inserting a pineapple into its butt.
			return false;
	}
}

/* The function for handling mouse events. This is the reason why we have to use a separate DLL;
 * see the SetWindowsHookEx documentation for details.
 */
LRESULT __declspec(dllexport) __stdcall mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
	bool processed = false; // Set to true if we don't want to pass the event to the application.
	if (nCode >= 0 && nCode == HC_ACTION) { // If nCode < 0, do nothing as per Microsoft's recommendations.
		MOUSEHOOKSTRUCT *eventInfo = (MOUSEHOOKSTRUCT*)lParam;
		switch (wParam) {
			case WM_LBUTTONDOWN:
				processed = processButtonDown(mbLeft, eventInfo);
				break;
			case WM_RBUTTONDOWN:
				processed = processButtonDown(mbRight, eventInfo);
				break;
			case WM_LBUTTONUP:
				processed = processButtonUp(mbLeft);
				break;
			case WM_RBUTTONUP:
				processed = processButtonUp(mbRight);
				break;
			case WM_MOUSEMOVE:
				int deltaX, deltaY;
				switch (currentState) {
					case dsDragging:
						// We are handling the moving or resizing of a window.
						// Find out the movement since the last known mouse position.
						deltaX = eventInfo->pt.x - lastMousePos.x, deltaY = eventInfo->pt.y - lastMousePos.y;
						if (draggingButton == mbLeft) {
							lastRect.left += deltaX;
							lastRect.top += deltaY;
							lastRect.right += deltaX;
							lastRect.bottom += deltaY;
						} else if (draggingButton == mbRight) {
							// Resize at the right corner/edge.
							switch (resizingX) {
								case -1:
									lastRect.left += deltaX; break;
								case 1:
									lastRect.right += deltaX; break;
								case 0:
									// We may have come close to a vertical border in the meantime.
									setResizingX(eventInfo->pt); break;
							}
							switch (resizingY) {
								case -1:
									lastRect.top += deltaY; break;
								case 1:
									lastRect.bottom += deltaY; break;
								case 0:
									// We may have come close to a horizontal border in the meantime.
									setResizingY(eventInfo->pt); break;
							}
						}
						// Now possibly the most important statement in the program: apply the movement/resizing to the window!
						SetWindowPos(draggedWindow, NULL,
							lastRect.left, lastRect.top, lastRect.right - lastRect.left, lastRect.bottom - lastRect.top,
							SWP_NOACTIVATE | SWP_NOOWNERZORDER);
						// No break here: fall through to the next case.
					case dsIgnoring:
						processed = true;
						break;
					case dsNone:
						break;
				}
				break;
		}
		lastMousePos = eventInfo->pt;
	}

	LRESULT res = CallNextHookEx((HHOOK)37, nCode, wParam, lParam); // first argument ignored
	if (processed)
		res = 1; // nonzero return value prevents passing the event to the application
	return res;
}

/* The function for handling keyboard events.
 * Or rather, the function to eat keyboard events that the application shouldn't receive.
 */
LRESULT __declspec(dllexport) __stdcall keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
#ifdef DEBUG
	if (wParam == 0x51) {
		// Q button pressed. Panic button for debugging.
		PostThreadMessage(mainThreadId, WM_QUIT, 0, 0);
		return 1;
	}
#endif
	if (nCode >= 0 && nCode == HC_ACTION) {
		if (wParam == modifier) {
			// Something happened to the modifier key.
			bool wasDown = modifierDown;
			modifierDown = !(lParam & 0x80000000);
			//MessageBoxA(NULL, modifierDown?"keydown":"keyup", lParam&0x4000, MB_OK);
			if (wasDown && !modifierDown) {
				// Modifier was released. Only pass the event on if there was no drag event.
				if (haveDragged) {
					return 1;
				}
				haveDragged = false;
			} else if (!wasDown && modifierDown) {
				// Modifier was pressed. There has been no drag event since.
				haveDragged = false;
			}
		}
	}
	return CallNextHookEx((HHOOK)37, nCode, wParam, lParam); // first argument ignored
}