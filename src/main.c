#include <windows.h>
#include <stdbool.h>

// Rename static for different use cases
#define global_variable static
#define local_persist static
#define internal_function static

// TODO: global for now
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable HBITMAP BitmapHandle;

// WIN32 prefix on non-msdn functions

void WIN32ResizeDIBSection(HDC DeviceContext, int Width, int Height)
{
    // TODO: Maybe don't free first, free after, then free first if that fails 
    // TODO: Free DIBSection

    if(BitmapHandle)
    {
        DeleteObject(BitmapHandle);
    }

    //
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;
    BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biClrUsed = 0;
    BitmapInfo.bmiHeader.biClrImportant = 0;

    // Create back-buffer for the Window
    BitmapHandle = CreateDIBSection(
        DeviceContext, &BitmapInfo,
        DIB_RGB_COLORS,
        // Memory back from Windows to draw into
        &BitmapMemory, 
        0, 0);

}

void WIN32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
    const VOID *lpBits;
    const BITMAPINFO *lpBitsInfo;
    // Rectangle copy for scaling
    int StretchDIBits(
        DeviceContext,
        // (Destination) Bbt Part of the window that needs to be redwrawn
        X, Y, Width, Height,
        // (Source)
        X, Y, Width, Height,
        lpBits,
        lpBitsInfo,
        DIB_RGB_COLORS, // DIB_PAL_COLORS
        // Replace and copy the window directly
        SRCCOPY);
}

LRESULT CALLBACK
MainWinCallback(HWND Window,
                UINT Message,
                WPARAM WParam,
                LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
    {
    case WM_SIZE:
    {
        // User resized the window
        OutputDebugString("WM_SIZE\n");

        RECT ClientRect;
        GetClinetRect(Window, &ClientRect);

        int Height = ClientRect.bottom - ClientRect.top;
        int Width = ClientRect.right - ClientRect.left;

        WIN32ResizeDIBSection(Width, Height);
    }
    break;
    case WM_DESTROY:
    {
        // User deletes the window
        OutputDebugString("WM_DESTROY\n");
        Running = false;
    }
    break;
    case WM_CLOSE:
    {
        // User clicked close
        OutputDebugString("WM_CLOSE\n");
        Running = false;
    }
    break;
    case WM_ACTIVATEAPP:
    {
        // Window focus
        OutputDebugString("WM_ACTIVATEAPP\n");
    }
    break;
    case WM_PAINT:
    {
        // User paint request
        PAINTSTRUCT Paint;

        HDC DeviceContext = BeginPaint(Window, &Paint);

        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;

        WIN32UpdateWindow(Window, X, Y, Width, Height);
        EndPaint(Window, &Paint);
    }
    default:
    {
        Result = DefWindowProc(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    WNDCLASS WindowClass = {};

    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWinCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "CRenderWindowClass";

    if (RegisterClass(&WindowClass))
    {
        HWND WindowHandle =
            CreateWindowEx(
                0,                                // DWORD dwExStyle
                WindowClass.lpszClassName,        // LPCTSTR lpClassName
                "C Render",                       // LPCTSTR lpWindowName
                WS_OVERLAPPEDWINDOW | WS_VISIBLE, // DWORD dwStyle
                CW_USEDEFAULT,                    // int x
                CW_USEDEFAULT,                    // int y
                CW_USEDEFAULT,                    // int nWidth
                CW_USEDEFAULT,                    // int nHeight
                0,                                // HWND hWndParent
                0,                                // HMENU hMenu
                Instance,                         // HINSTANCE hInstance
                0                                 // LPVOID lpParam
            );
        if (WindowHandle)
        {
            Running = true;

            // Handle message queue
            while (Running)
            {
                MSG Message;
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
                if (MessageResult > 0)
                {
                    // Parsing for keyboard input
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }

    return (0);
}