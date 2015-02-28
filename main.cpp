#include <windows.h>
#include <cstdint>
#include "quaternion.hpp"
#include "vector3d.hpp"
#include "polygon.hpp"

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int BytesPerPixel;
	int Pitch;
};

static bool Running;
static win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
	int Width;
	int Height;
};

win32_window_dimension
GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	return Result;
}

static void
Render(win32_offscreen_buffer* Buffer, const polygon& p)
{
	auto eps = 0.02;
	double x, y;
	auto nx = 2;
	auto ny = 2;
	auto Row = static_cast<uint8_t *>(Buffer->Memory);
	for (auto Y = 0; Y<Buffer->Height; ++Y)
	{
		auto Pixel = reinterpret_cast<uint32_t*>(Row);
		for (auto X = 0; X<Buffer->Width; ++X)
		{
			x = static_cast<double>(X) / Buffer->Width;
			y = static_cast<double>(Y) / Buffer->Height;
			vector3d v{ 2*nx*x-nx, 2*ny*y-ny, 0. };
			// Project vertices in camera space
			if (polyhedra_utilities::is_close_xy(p.vertices, v, eps))
				*Pixel++ = 0x000011EE; // 0xAARRGGBB
			else
				*Pixel++ = 0x00000000;
				//Pixel++;
			//std::stringstream ss;
			//ss << "pixel : " << x << "\t" << y << std::endl;
			//OutputDebugStringA(ss.str().c_str());
		}
		Row += Buffer->Pitch;
	}
}

static void
TranslateBackBuffer(win32_offscreen_buffer* Buffer, int /*XOffset*/, int /*YOffset*/)
{
	auto Row = static_cast<uint8_t*>(Buffer->Memory);
	uint32_t tempPixel;
	for (auto Y = 0; Y<Buffer->Height; ++Y)
	{
		auto Pixel = reinterpret_cast<uint32_t*>(Row);
		for (auto X = 0; X<Buffer->Width - 1; ++X)
		{
			tempPixel = *Pixel;
			*(++Pixel) = tempPixel;
		}
		Row += Buffer->Pitch;
	}
}

static void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	Buffer->BytesPerPixel = 4;
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(nullptr, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Buffer->Width*Buffer->BytesPerPixel;
}

static void
Win32DisplayBufferInWindow(HDC DeviceContext, int Width, int Height, win32_offscreen_buffer Buffer)
{
	StretchDIBits(DeviceContext,
		0, 0, Width, Height,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
UINT Message,
WPARAM WParam,
LPARAM LParam)
{
	LRESULT Result = 0;
	switch (Message)
	{
	case WM_SIZE:
	{
	} break;
	case WM_DESTROY:
	{
		OutputDebugStringA("WM_DESTROY\n");
	} break;
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		OutputDebugStringA("WM_CLOSE\n");
	} break;
	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		/*auto VKCode = WParam;
		auto WasDown = ((LParam & (1 << 30)) != 0);
		auto IsDown = ((LParam & (1 << 32)) == 0);*/
	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		auto DeviceContext = BeginPaint(Window, &Paint);
		auto Dimension = GetWindowDimension(Window);
		Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer);
		EndPaint(Window, &Paint);
	}
	default:
	{
		Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
	}
	return Result;
}

int CALLBACK
WinMain(HINSTANCE hInstance,
HINSTANCE /*hPrevInstance*/,
LPSTR /*lpCmdLine*/,
int /*nCmdShow*/)
{
	WNDCLASS WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1288, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = hInstance;
	WindowClass.lpszClassName = "MandelbrotViewerWindowClass";

	if (RegisterClass(&WindowClass))
	{
		auto Window =
			CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"3DMotor",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			nullptr,
			nullptr,
			hInstance,
			nullptr);

		vector3d u;
		u.x = 0.1;
		u.y = 1.0;
		u.z = 0.0;
		auto norm = sqrt(vector3d_utilities::snorm(u));
		u.x /= norm;
		u.y /= norm;
		u.z /= norm;
		auto angle = 0.05;
		auto polyhedra = polyhedra_utilities::generate_discrete_sphere(u, 6);
		auto versor = quaternion_utilities::versor(u, angle);
		if (Window)
		{
			Running = true;
			while (Running)
			{
				MSG Message;
				while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				auto DeviceContext = GetDC(Window);
				auto Dimension = GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer);
				ReleaseDC(Window, DeviceContext);
				vector3d_utilities::rotate(polyhedra.vertices, versor);
				Render(&GlobalBackBuffer, polyhedra);
			}
		}
		else
		{
			// TODO : Logging
		}
	}
	else
	{
		// TODO : Logging
	}

	return 0;
}