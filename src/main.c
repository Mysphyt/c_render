#include <windows.h>

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
    }
    break;
    case WM_DESTROY:
    {
        // User deletes the window
        OutputDebugString("WM_DESTROY\n");
    }
    break;
    case WM_CLOSE:
    {
        // User clicked close
        OutputDebugString("WM_CLOSE\n");
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

        // Paint a white background
        PatBlt(DeviceContext, X, Y, Width, Height, WHITENESS);

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
    // MessageBox(0, "This is c_render.", "c_render", MB_OK|MB_ICONINFORMATION);

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
            // Handle message queue
            MSG Message;
            for(;;)
            {
                BOOL MessageResult = GetMessage(&Message, 0,0,0);
                if(MessageResult > 0)
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