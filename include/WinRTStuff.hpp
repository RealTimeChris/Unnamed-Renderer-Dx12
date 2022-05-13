// WinRTStuff.hpp (Header Only)
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#ifndef WINRT_STUFF
	#define WINRT_STUFF
#endif

#ifndef URDX12_INLINE
	#define URDX12_INLINE [[msvc::forceinline]] inline
#endif

#ifndef WINRT_LEAN_AND_MEAN
	#define WINRT_LEAN_AND_MEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.UI.Xaml.Hosting.DesktopWindowXamlSource.h>

namespace win_rt_stuff {

	URDX12_INLINE void fail_bail(const wchar_t* errorMessage, const wchar_t* errorTitle) {
		MessageBox(NULL, errorMessage, errorTitle, NULL);
		exit(-1);
	}

	class window_class {
	  protected:
		URDX12_INLINE static LRESULT CALLBACK window_procedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		const wchar_t* className;

	  public:
		URDX12_INLINE window_class() : className{ L"WindowClass" } {
			WNDCLASSEX windowClassDescription{};
			windowClassDescription.cbSize		 = sizeof(WNDCLASSEX);
			windowClassDescription.hbrBackground = reinterpret_cast<HBRUSH>(LTGRAY_BRUSH);
			windowClassDescription.hCursor		 = LoadCursor(NULL, IDC_ARROW);
			windowClassDescription.hInstance	 = GetModuleHandle(NULL);
			windowClassDescription.lpfnWndProc	 = this->window_procedure;
			windowClassDescription.lpszClassName = this->className;
			windowClassDescription.style		 = CS_HREDRAW | CS_VREDRAW;

			ATOM result{ 0 };

			result = RegisterClassEx(&windowClassDescription);

			if (result == 0) {
				fail_bail(L"RegisterClassEx() failed.", L"WinRTStuff::WindowClass Error");
			}
		}

		URDX12_INLINE const wchar_t* report_class_name() {
			return this->className;
		}

		URDX12_INLINE ~window_class() {
			UnregisterClass(this->className, GetModuleHandle(NULL));
		}
	};

	class render_window {
	  protected:
		HWND windowHandle{ NULL };

	  public:
		URDX12_INLINE render_window(const wchar_t* windowClassName, unsigned __int32 viewPixelWidth, unsigned __int32 viewPixelHeight, const wchar_t* windowTitle = L"RenderWindow") {
			int totalWidth{ ( int )viewPixelWidth + 16 }, totalHeight{ ( int )viewPixelHeight + 39 };

			this->windowHandle = CreateWindowEx(NULL, windowClassName, windowTitle, WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX, 0, 0, totalWidth, totalHeight, NULL, NULL,
				GetModuleHandle(NULL), NULL);

			if (this->windowHandle == NULL) {
				fail_bail(L"CreateWindowEx() failed.", L"WinRTStuff::RenderWindow Error");
			}

			BOOL result{ 0 };

			result = SetWindowPos(this->windowHandle, HWND_TOP, -7, 0, totalWidth, totalHeight, SWP_NOSENDCHANGING);

			if (result == 0) {
				fail_bail(L"SetWindowPos() failed.", L"WinRTStuff::RenderWindow Error");
			}
		}

		URDX12_INLINE HWND report_window_handle() {
			return this->windowHandle;
		}

		URDX12_INLINE ~render_window() {
			if (( bool )IsWindowVisible(this->windowHandle) == true) {
				DestroyWindow(this->windowHandle);
			}
		}
	};

}
