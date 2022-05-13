// WinRTStuff.hpp (Header Only)
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#ifndef WINRT_STUFF
	#define WINRT_STUFF
#endif

#ifndef WINRT_LEAN_AND_MEAN
	#define WINRT_LEAN_AND_MEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.UI.Xaml.Hosting.DesktopWindowXamlSource.h>

namespace WinRTStuff {

	void FailBail(const wchar_t* ErrorMessage, const wchar_t* ErrorTitle) {
		MessageBox(NULL, ErrorMessage, ErrorTitle, NULL);
		exit(-1);
	}

	class WindowClass {
	  protected:
		static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		const wchar_t* ClassName;

	  public:
		WindowClass() : ClassName{ L"WindowClass" } {
			WNDCLASSEX WindowClassDescription{};
			WindowClassDescription.cbSize = sizeof(WNDCLASSEX);
			WindowClassDescription.hbrBackground = reinterpret_cast<HBRUSH>(LTGRAY_BRUSH);
			WindowClassDescription.hCursor = LoadCursor(NULL, IDC_ARROW);
			WindowClassDescription.hInstance = GetModuleHandle(NULL);
			WindowClassDescription.lpfnWndProc = this->WindowProcedure;
			WindowClassDescription.lpszClassName = this->ClassName;
			WindowClassDescription.style = CS_HREDRAW | CS_VREDRAW;

			ATOM Result{ 0 };

			Result = RegisterClassEx(&WindowClassDescription);

			if (Result == 0) {
				FailBail(L"RegisterClassEx() failed.", L"WinRTStuff::WindowClass Error");
			}
		}

		const wchar_t* ReportClassName() {
			return this->ClassName;
		}

		~WindowClass() {
			UnregisterClass(this->ClassName, GetModuleHandle(NULL));
		}
	};

	class RenderWindow {
	  protected:
		HWND WindowHandle{ NULL };

	  public:
		RenderWindow(
			const wchar_t* WindowClassName, unsigned __int32 ViewPixelWidth, unsigned __int32 ViewPixelHeight, const wchar_t* WindowTitle = L"RenderWindow") {
			int TotalWidth{ ( int )ViewPixelWidth + 16 }, TotalHeight{ ( int )ViewPixelHeight + 39 };

			this->WindowHandle = CreateWindowEx(NULL, WindowClassName, WindowTitle, WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX, 0, 0, TotalWidth, TotalHeight,
				NULL, NULL, GetModuleHandle(NULL), NULL);

			if (this->WindowHandle == NULL) {
				FailBail(L"CreateWindowEx() failed.", L"WinRTStuff::RenderWindow Error");
			}

			BOOL Result{ 0 };

			Result = SetWindowPos(this->WindowHandle, HWND_TOP, -7, 0, TotalWidth, TotalHeight, SWP_NOSENDCHANGING);

			if (Result == 0) {
				FailBail(L"SetWindowPos() failed.", L"WinRTStuff::RenderWindow Error");
			}
		}

		HWND ReportWindowHandle() {
			return this->WindowHandle;
		}

		~RenderWindow() {
			if (( bool )IsWindowVisible(this->WindowHandle) == true) {
				DestroyWindow(this->WindowHandle);
			}
		}
	};

}
