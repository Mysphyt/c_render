#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <xinput.h>

// Rename static for different use case readability
#define global_variable static
#define local_persist static
#define internal_function static

#define win32_buffer struct _win32_backbuffer
#define win32_window_dimension struct _win32_window_dimension

global_variable bool GlobalRunning;
global_variable win32_buffer GlobalBackbuffer;

// NOTE: Define stub functions for XInput in case there is an issue loading the xinput dll
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal_function void
LoadXInput(void)
{
    // Try to load the XInput library
    HMODULE XInputLibrary = LoadLibrary("xinput_3.dll");
    if (XInputLibrary)
    {
        XInputGetState_ = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState_ = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

// TODO: global for now
struct _win32_backbuffer
{
    // NOTE: Pixels are always 32--bits wide, memory order BB GG RR xx
    BITMAPINFO BitmapInfo;
    void *BitmapMemory;
    int BitmapHeight;
    int BitmapWidth;
    int BytesPerPixel;
    int Pitch;
};

struct _win32_window_dimension
{
    int Width;
    int Height;
};

// Define shorthands for platform agnostic ints
// . (u)int(n)_t is the correct size regardless of platform
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

internal_function win32_window_dimension
WIN32GetWindowDimension(HWND Window)
{
    // TODO: aspect ratio scaling
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Height = ClientRect.bottom - ClientRect.top;
    Result.Width = ClientRect.right - ClientRect.left;

    return Result;
}

internal_function void
RenderGradient(win32_buffer Buffer, int XOffset, int YOffset)
{
    /*
        Updates 32 bit (RGBx) Pixel values in BitmapMemory to a gradient
        based on X and Y coordinates (position) and offset (time)
    */

    // Strides may not match pixel boundry
    uint8 *Row = (uint8 *)Buffer.BitmapMemory;
    // Loop through each bit in the Bitmap
    for (int Y = 0;
         Y < Buffer.BitmapHeight;
         ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        // Incriment each pixel to make sure pitch is aligned
        for (int X = 0;
             X < Buffer.BitmapWidth;
             ++X)
        {
            /*
                           0  1  2  3
                Memory:    BB GG RR XX
                Register:  XX RR GG BB
            */

            // Set RGB values
            uint8 Blue = Y;  // (uint8)256 - ((float)(Y+YOffset) / BitmapWidth) * (float)256; //(X + XOffset);
            uint8 Green = X; // ((float)(Y+YOffset) / BitmapHeight) * (float)256; //(Y + YOffset);
            uint8 Red = 0;   //(XOffset - Y);

            // Set the pixel 32bit value (padding will be 00)
            *Pixel++ = ((Red << 16) | ((Green << 8) | Blue));
        }
        Row += Buffer.Pitch;
    }
}

// WIN32 prefix on non-msdn functions
internal_function void
WIN32ResizeDIBSection(win32_buffer *Buffer, int Width, int Height)
{
    /*
        Re-allocates Bitmapmemory on resize event
    */
    if (Buffer->BitmapMemory)
    {
        // MEM_RELEASE instead of MEM_DECOMMIT because RELEASE actually frees as well, use DECOMMIT if you want them back later
        VirtualFree(Buffer->BitmapMemory, 0, MEM_RELEASE);
    }
    // Width and height in pixels
    Buffer->BitmapHeight = Height;
    Buffer->BitmapWidth = Width;

    Buffer->Pitch = Buffer->BitmapWidth * Buffer->BytesPerPixel;
    //
    Buffer->BitmapInfo.bmiHeader.biSize = sizeof(Buffer->BitmapInfo.bmiHeader);

    Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->BitmapWidth;
    // Negative BitmapHeight for top-down indexing
    // ...Meaning the first three bytes of the Bitmap are the top-left Pixel,
    //    not the bottom-left.
    Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->BitmapHeight;

    // 1 plane
    Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    // 32 bits per pixe (8x3-rgb + 8-padding), align pixels on 4-byte boundaries
    Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Width * Height) * Buffer->BytesPerPixel;

    Buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal_function void
WIN32UpdateWindow(win32_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    /*
        Render the Backbuffer
    */
    // TODO: Aspect ratio conversion stretch modes
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,               // Destination
        0, 0, Buffer->BitmapWidth, Buffer->BitmapHeight, // Source
        Buffer->BitmapMemory,
        &Buffer->BitmapInfo,
        // Use RGB colors
        DIB_RGB_COLORS,
        // Copy the bitmap directly
        SRCCOPY);
}

LRESULT CALLBACK
MainWinCallback(HWND Window,
                UINT Message,
                WPARAM WParam,
                LPARAM LParam)
{
    /*
        Window message callback function
    */
    LRESULT Result = 0;

    switch (Message)
    {
    case WM_SYSKEYUP:
    {
    }
    case WM_SYSKEYDOWN:
    {
    }
    case WM_KEYUP:
    {
        uint32 VKCode = WParam;
        // != 0 to catch 30th bit case as 1
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool IsDown = ((LParam & (1 << 31)) == 0);
        if (WasDown != IsDown)
        {
            if (VKCode == 'W')
            {

            }
            if (VKCode == 'A')
            {

            }
            if (VKCode == 'S')
            {

            }
            if (VKCode == 'D')
            {

            }
        }
    }
    case WM_KEYDOWN:
    {
    }
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
        GlobalRunning = false;
    }
    break;
    case WM_CLOSE:
    {
        // User clicked close
        OutputDebugString("WM_CLOSE\n");
        GlobalRunning = false;
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
    // Set the Backbuffer resolution
    GlobalBackbuffer.BytesPerPixel = 4;
    WIN32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WNDCLASS WindowClass = {};

    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWinCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "CRenderWindowClass";

    if (RegisterClass(&WindowClass))
    {
        HWND Window =
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
        if (Window)
        {
            //
            HDC DeviceContext = GetDC(Window);

            GlobalRunning = true;

            int XOffset = 0;
            int YOffset = 0;

            // Handle message queue
            while (GlobalRunning)
            {
                MSG Message;

                // PeekMessage does not block when there is no message
                // . PM_REMOVE, remove the message from the queue
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    // Parsing for keyboard input
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // Poll user input
                // . Should we poll this more frequently
                for (DWORD ControllerIndex = 0;
                     ControllerIndex < XUSER_MAX_COUNT;
                     ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // Controller is plugged in
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        // Parse button states
                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
                        bool RThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
                        bool LShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                    }
                    else
                    {
                        // Controller is not plugged in
                    }
                }

                RenderGradient(GlobalBackbuffer, XOffset, YOffset);

                win32_window_dimension Dim = WIN32GetWindowDimension(Window);
                WIN32UpdateWindow(&GlobalBackbuffer, DeviceContext, Dim.Width, Dim.Height);

                ++XOffset;
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
