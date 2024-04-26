#include "native_window.h"
#include <windows.h>
// GET_X_LPARAM(), GET_Y_LPARAM()
#include <windowsx.h> 
#include <tchar.h>
#include <dwmapi.h>
#include <map>

#include "runtime/core/util.h"

#undef max

class NativeWindow_Windows;
// Maps "Windows" window handle to the wrapper class
std::map<HWND, NativeWindow_Windows*> g_Windows;

LRESULT WINAPI WndProc_(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static KeyCode keyMap(WPARAM wParam) {
	switch(wParam) {
		case VK_TAB: return KeyCode::KEY_TAB;
		case VK_LEFT: return KeyCode::KEY_LEFT;
		case VK_RIGHT: return KeyCode::KEY_RIGHT;
		case VK_UP: return KeyCode::KEY_UP;
		case VK_DOWN: return KeyCode::KEY_DOWN;
		case VK_PRIOR: return KeyCode::KEY_PAGE_UP;
		case VK_NEXT: return KeyCode::KEY_PAGE_DOWN;
		case VK_HOME: return KeyCode::KEY_HOME;
		case VK_END: return KeyCode::KEY_END;
		case VK_INSERT: return KeyCode::KEY_INSERT;
		case VK_DELETE: return KeyCode::KEY_DELETE;
		case VK_BACK: return KeyCode::KEY_BACKSPACE;
		case VK_SPACE: return KeyCode::KEY_SPACE;
		case VK_RETURN: return KeyCode::KEY_ENTER;
		case VK_ESCAPE: return KeyCode::KEY_ESCAPE;
		case VK_OEM_7: return KeyCode::KEY_APOSTROPHE;
		case VK_OEM_COMMA: return KeyCode::KEY_COMMA;
		case VK_OEM_MINUS: return KeyCode::KEY_MINUS;
		case VK_OEM_PERIOD: return KeyCode::KEY_PERIOD;
		case VK_OEM_2: return KeyCode::KEY_SLASH;
		case VK_OEM_1: return KeyCode::KEY_SEMICOLON;
		case VK_OEM_PLUS: return KeyCode::KEY_KP_ADD;
		case VK_OEM_4: return KeyCode::KEY_LEFT_BRACKET;
		case VK_OEM_5: return KeyCode::KEY_BACKSLASH;
		case VK_OEM_6: return KeyCode::KEY_RIGHT_BRACKET;
		case VK_OEM_3: return KeyCode::KEY_GRAVE_ACCENT;
		case VK_CAPITAL: return KeyCode::KEY_CAPS_LOCK;
		case VK_SCROLL: return KeyCode::KEY_SCROLL_LOCK;
		case VK_NUMLOCK: return KeyCode::KEY_NUM_LOCK;
		case VK_SNAPSHOT: return KeyCode::KEY_PRINT_SCREEN;
		case VK_PAUSE: return KeyCode::KEY_PAUSE;
		case VK_NUMPAD0: return KeyCode::KEY_KP_0;
		case VK_NUMPAD1: return KeyCode::KEY_KP_1;
		case VK_NUMPAD2: return KeyCode::KEY_KP_2;
		case VK_NUMPAD3: return KeyCode::KEY_KP_3;
		case VK_NUMPAD4: return KeyCode::KEY_KP_4;
		case VK_NUMPAD5: return KeyCode::KEY_KP_5;
		case VK_NUMPAD6: return KeyCode::KEY_KP_6;
		case VK_NUMPAD7: return KeyCode::KEY_KP_7;
		case VK_NUMPAD8: return KeyCode::KEY_KP_8;
		case VK_NUMPAD9: return KeyCode::KEY_KP_9;
		case VK_DECIMAL: return KeyCode::KEY_PERIOD;
		case VK_DIVIDE: return KeyCode::KEY_SLASH;
		case VK_MULTIPLY: return KeyCode::KEY_KP_MULTIPLY;
		case VK_SUBTRACT: return KeyCode::KEY_KP_SUBTRACT;
		case VK_ADD: return KeyCode::KEY_KP_ADD;
		case VK_SHIFT: return KeyCode::KEY_SHIFT;
		case VK_CONTROL: return KeyCode::KEY_CONTROL;
		case VK_MENU: return KeyCode::KEY_ALT;
		case VK_LSHIFT: return KeyCode::KEY_LEFT_SHIFT;
		case VK_LCONTROL: return KeyCode::KEY_LEFT_CONTROL;
		case VK_LMENU: return KeyCode::KEY_LEFT_ALT;
		case VK_LWIN: return KeyCode::KEY_LEFT_SUPER;
		case VK_RSHIFT: return KeyCode::KEY_RIGHT_SHIFT;
		case VK_RCONTROL: return KeyCode::KEY_RIGHT_CONTROL;
		case VK_RMENU: return KeyCode::KEY_RIGHT_ALT;
		case VK_RWIN: return KeyCode::KEY_RIGHT_SUPER;
		case '0': return KeyCode::KEY_0;
		case '1': return KeyCode::KEY_1;
		case '2': return KeyCode::KEY_2;
		case '3': return KeyCode::KEY_3;
		case '4': return KeyCode::KEY_4;
		case '5': return KeyCode::KEY_5;
		case '6': return KeyCode::KEY_6;
		case '7': return KeyCode::KEY_7;
		case '8': return KeyCode::KEY_8;
		case '9': return KeyCode::KEY_9;
		case 'A': return KeyCode::KEY_A;
		case 'B': return KeyCode::KEY_B;
		case 'C': return KeyCode::KEY_C;
		case 'D': return KeyCode::KEY_D;
		case 'E': return KeyCode::KEY_E;
		case 'F': return KeyCode::KEY_F;
		case 'G': return KeyCode::KEY_G;
		case 'H': return KeyCode::KEY_H;
		case 'I': return KeyCode::KEY_I;
		case 'J': return KeyCode::KEY_J;
		case 'K': return KeyCode::KEY_K;
		case 'L': return KeyCode::KEY_L;
		case 'M': return KeyCode::KEY_M;
		case 'N': return KeyCode::KEY_N;
		case 'O': return KeyCode::KEY_O;
		case 'P': return KeyCode::KEY_P;
		case 'Q': return KeyCode::KEY_Q;
		case 'R': return KeyCode::KEY_R;
		case 'S': return KeyCode::KEY_S;
		case 'T': return KeyCode::KEY_T;
		case 'U': return KeyCode::KEY_U;
		case 'V': return KeyCode::KEY_V;
		case 'W': return KeyCode::KEY_W;
		case 'X': return KeyCode::KEY_X;
		case 'Y': return KeyCode::KEY_Y;
		case 'Z': return KeyCode::KEY_Z;
		case VK_F1: return KeyCode::KEY_F1;
		case VK_F2: return KeyCode::KEY_F2;
		case VK_F3: return KeyCode::KEY_F3;
		case VK_F4: return KeyCode::KEY_F4;
		case VK_F5: return KeyCode::KEY_F5;
		case VK_F6: return KeyCode::KEY_F6;
		case VK_F7: return KeyCode::KEY_F7;
		case VK_F8: return KeyCode::KEY_F8;
		case VK_F9: return KeyCode::KEY_F9;
		case VK_F10: return KeyCode::KEY_F10;
		case VK_F11: return KeyCode::KEY_F11;
		case VK_F12: return KeyCode::KEY_F12;
		default: return KeyCode::KEY_NONE;
	}
}

class NativeWindow_Windows: public INativeWindow {
public:

	NativeWindow_Windows(const char* inWindowTitle, u32 inWidth, u32 inHeight) {
		bool bMaximize = false;
		const auto windowMaxWidth = GetSystemMetrics(SM_CXSCREEN);
		const auto windowMaxHeight = GetSystemMetrics(SM_CYSCREEN);
		const auto windowWidth = math::Clamp((int)inWidth, 100, windowMaxWidth);
		const auto windowHeight = math::Clamp((int)inHeight, 100, windowMaxHeight);
		const auto windowPosX = (windowMaxWidth - windowWidth) / 2;
		const auto windowPosY = (windowMaxHeight - windowHeight) / 2;

		m_WndCls = {
			sizeof(WNDCLASSEX), 
			CS_CLASSDC | CS_HREDRAW | CS_VREDRAW, 
			WndProc_, 
			0L, 
			0L, 
			GetModuleHandleW(NULL), 
			NULL, 
			NULL, 
			NULL, 
			NULL, 
			_T("PetEngine_Window_Class"), 
			NULL
		};
		::RegisterClassEx(&m_WndCls);

		auto inWindowTitleWide = util::ToWideString(std::string(inWindowTitle));
		m_hwnd = ::CreateWindowEx(
			0L, 
			m_WndCls.lpszClassName, 
			inWindowTitleWide.c_str(), 
			WS_OVERLAPPEDWINDOW, 
			windowPosX, 
			windowPosY, 
			windowWidth, 
			windowHeight, 
			NULL, 
			NULL, 
			m_WndCls.hInstance, 
			NULL
		);
		m_WindowSizePx = {(float)windowWidth, (float)windowHeight};
		// Cache cursors for referencing later
		m_Cursors[0] = NULL;
		for(auto i = 1; i != (int)MouseCursor::_Size; ++i) {
			m_Cursors[i] = ::LoadCursor(NULL, castCursor((MouseCursor)i));
		}
	}

	void Show() override {
		::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(m_hwnd);
		SetMouseCursor(MouseCursor::Arrow);
	}

	bool PollEvents() override {
		MSG msg;
		while(::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if(msg.message == WM_QUIT)
				return false;
		}
		return true;
	}

	void* GetNativeHandle() override {
		return (void*)m_hwnd;
	}

	void Destroy() override {
		::DestroyWindow(m_hwnd);
		::UnregisterClass(m_WndCls.lpszClassName, m_WndCls.hInstance);
		g_Windows.erase(m_hwnd);
	}

	float2 GetSize() override {
		return m_WindowSizePx;
	}

	void SetOnCursorMoveCallback(const std::function<void(float, float)>& inCallback) override {
		m_OnCursorMoveEvent = inCallback;
	}

	void SetOnMouseButtonCallback(const std::function<void(KeyCode, bool)>& inCallback) override {
		m_OnMouseButtonEvent = inCallback;
	}

	void SetOnKeyboardButtonCallback(const std::function<void(KeyCode, bool)>& inCallback) override {
		m_OnKeyboardButtonEvent = inCallback;
	}

	void SetOnWindowResizedCallback(const std::function<void(float2)>& inCallback) override {
		m_OnWindowResizeEvent = inCallback;
	}

	void SetOnMouseScrollCallback(const std::function<void(float)>& inCallback) override {
		m_OnMouseScrollEvent = inCallback;
	}

	void SetOnCharInputCallback(const std::function<void(wchar_t)>& inCallback) override {
		m_OnCharInputCallback = inCallback;
	}

	void SetMouseCursor(MouseCursor inCursorType) override {
		::SetCursor(m_Cursors[(int)inCursorType]);
	}

	LRESULT WINAPI HandleEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		switch(msg) {
			case WM_MOUSEMOVE: {
				if(!m_bMouseTracked) {
					TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE | TME_HOVER, m_hwnd, 0};
					::TrackMouseEvent(&tme);
					m_bMouseTracked = true;
				}
				if (m_OnCursorMoveEvent) {
					m_OnCursorMoveEvent((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
				}
				break;
			}
			case WM_MOUSELEAVE: {
				m_bMouseTracked = false;
				if(m_OnCursorMoveEvent) {
					m_OnCursorMoveEvent(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				}
				break;
			}
			case WM_MOUSEHOVER: {
				this->SetMouseCursor(MouseCursor::Arrow);
				break;
			}
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
			case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: {				
				int button = 0;
				if(msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
				if(msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
				if(msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
				if(msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				
				if(::GetCapture() == NULL) { ::SetCapture(m_hwnd); }
				if (m_OnMouseButtonEvent) { m_OnMouseButtonEvent(static_cast<KeyCode>(button + 1), true); }

				return 0;
			}
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_XBUTTONUP: {
				int button = 0;
				if(msg == WM_LBUTTONUP) { button = 0; }
				if(msg == WM_RBUTTONUP) { button = 1; }
				if(msg == WM_MBUTTONUP) { button = 2; }
				if(msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				
				if(::GetCapture() == m_hwnd) { ::ReleaseCapture(); }					
				if(m_OnMouseButtonEvent) { m_OnMouseButtonEvent(static_cast<KeyCode>(button + 1), false); }

				return 0;
			}
			case WM_MOUSEWHEEL: {
				if(m_OnMouseScrollEvent) { m_OnMouseScrollEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA); }
				return 0;
			}			
			case WM_MOUSEHWHEEL: {
				return 0;
			}			
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP: {
				const bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
				if(wParam < 256) {
					if (m_OnKeyboardButtonEvent) {
						m_OnKeyboardButtonEvent(keyMap((int)wParam), isDown);
					}
				}
				return 0;
			}
			case WM_SETFOCUS:
			case WM_KILLFOCUS: {
				return 0;
			}
			case WM_CHAR: {
				if(wParam > 0 && wParam < 0x10000) {				
					m_OnCharInputCallback((wchar_t)wParam);
				}
				return 0;
			}
			case WM_SETCURSOR: {
			/*	if(LOWORD(lParam) == HTCLIENT) {					
					LPTSTR win32_cursor = IDC_ARROW;
					::SetCursor(::LoadCursor(NULL, win32_cursor));
					return 1;
				}*/
				break;
			}
			case WM_DEVICECHANGE: { 
				return 0; 
			}
			case WM_SIZE: {
				m_WindowSizePx = float2((float)LOWORD(lParam), (float)HIWORD(lParam));
				if(!m_OnWindowResizeEvent) {
					return 0;
				}
				if(wParam != SIZE_MINIMIZED) {
					m_OnWindowResizeEvent(m_WindowSizePx);
				} else {
					m_OnWindowResizeEvent(float2(0.f));
				}
				return 0;
			}
			case WM_SYSCOMMAND: {
				if((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
					return 0;
				break;
			}			
			case WM_DESTROY: {
				::PostQuitMessage(0);
				return 0;
			}
		}
		return ::DefWindowProc(hWnd, msg, wParam, lParam);
	}

	LPTSTR castCursor(MouseCursor inCursor) {
		switch(inCursor) {
			case MouseCursor::No:		return IDC_NO;
			case MouseCursor::Arrow:	return IDC_ARROW;
			case MouseCursor::IBeam:	return IDC_IBEAM;
			case MouseCursor::Wait:		return IDC_WAIT;
			case MouseCursor::Cross:	return IDC_CROSS;
			case MouseCursor::UpArrow:	return IDC_UPARROW;
			case MouseCursor::SizeNWSE:	return IDC_SIZENWSE;
			case MouseCursor::SizeNESW:	return IDC_SIZENESW;
			case MouseCursor::SizeWE:	return IDC_SIZEWE;
			case MouseCursor::SizeNS:	return IDC_SIZENS;
			case MouseCursor::SizeALL:	return IDC_SIZEALL;
			case MouseCursor::Hand:		return IDC_HAND;
			case MouseCursor::Help:		return IDC_HELP;
			default:					return IDC_ARROW;
		}
	}

	void GetClipboardText(std::wstring& outString) const override {
		outString.clear();

		if(!::OpenClipboard(NULL)) {
			return;
		}
		HANDLE wbuf_handle = ::GetClipboardData(CF_UNICODETEXT);
		if(wbuf_handle == NULL) {
			::CloseClipboard();
			return;
		}
		if(const WCHAR* wbuf_global = (const WCHAR*)::GlobalLock(wbuf_handle)) {
			outString = wbuf_global;
		}
		::GlobalUnlock(wbuf_handle);
		::CloseClipboard();
	}

	void SetClipboardText(const std::wstring& inString) const override {
		if(!::OpenClipboard(NULL)) {
			return;
		}
		// Add space for \0
		const auto textSizeBytes = inString.size() * sizeof(WCHAR) + sizeof(WCHAR);
		HGLOBAL bufferHandle = ::GlobalAlloc(GMEM_MOVEABLE, textSizeBytes);

		if(bufferHandle == NULL) {
			::CloseClipboard();
			return;
		}
		WCHAR* buffer = (WCHAR*)::GlobalLock(bufferHandle);

		if (buffer) {
			memcpy(buffer, inString.data(), textSizeBytes);
			buffer[inString.size()] = L'\0';
		}		
		::GlobalUnlock(bufferHandle);
		::EmptyClipboard();

		if(::SetClipboardData(CF_UNICODETEXT, bufferHandle) == NULL) {
			::GlobalFree(bufferHandle);
		}
		::CloseClipboard();
	}

private:

	HWND								m_hwnd = nullptr;
	WNDCLASSEX							m_WndCls;
	bool								m_bMouseTracked = false;

	HCURSOR								m_Cursors[(int)MouseCursor::_Size];

	float2								m_WindowSizePx;

	std::function<void(float, float)>	m_OnCursorMoveEvent;
	std::function<void(KeyCode, bool)>	m_OnMouseButtonEvent;
	std::function<void(KeyCode, bool)>	m_OnKeyboardButtonEvent;
	std::function<void(float)>			m_OnMouseScrollEvent;
	std::function<void(float2)>			m_OnWindowResizeEvent;
	std::function<void(wchar_t)>		m_OnCharInputCallback;

};

INativeWindow* INativeWindow::createWindow(std::string_view inWindowTitle, u32 inWidth, u32 inHeight) {
	auto window = new NativeWindow_Windows{inWindowTitle.data(), inWidth, inHeight};
	g_Windows.emplace((HWND)window->GetNativeHandle(), window);
	return window;
}

LRESULT WINAPI WndProc_(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	auto it = g_Windows.find(hWnd);
	if(it == g_Windows.end()) {
		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}
	auto* window = it->second;
	return window->HandleEvent(hWnd, msg, wParam, lParam);
}
