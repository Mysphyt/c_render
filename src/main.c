#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

// Rename static for different use cases
#define global_variable static
#define local_persist static
#define internal_function static
#define win32_buffer struct _win32_backbuffer

global_variable bool Running;
global_variable win32_buffer GlobalBackbuffer;

// TODO: global for now
struct _win32_backbuffer {
    BITMAPINFO BitmapInfo;
    void *BitmapMemory;
    int BitmapHeight;
    int BitmapWidth;
    int BytesPerPixel;
    int Pitch;
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

void
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
                Memory:    BB GG RR xx
                Register:  xx RR GG BB

            */

            // Set RGB values
            uint8 Blue = Y; // (uint8)256 - ((float)(Y+YOffset) / BitmapWidth) * (float)256; //(X + XOffset);
            uint8 Green = X; // ((float)(Y+YOffset) / BitmapHeight) * (float)256; //(Y + YOffset);
            uint8 Red = 0; //(XOffset - Y);

            // Set the pixel 32bit value (padding will be 00)
            *Pixel++ = ((Red << 16) | ((Green << 8) | Blue));
        }
        Row += Buffer.Pitch;
    }
}

// WIN32 prefix on non-msdn functions
void 
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

    Buffer->Pitch = Buffer->BitmapWidth*Buffer->BytesPerPixel;
    //
    Buffer->BitmapInfo.bmiHeader.biSize = sizeof(Buffer->BitmapInfo.bmiHeader);

    Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->BitmapWidth;
    // Negative BitmapHeight for top-down indexing
    Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->BitmapHeight;

    // 1 plane
    Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    // 32 bits per pixe (8x3-rgb + 8-padding), align pixels on 4-byte boundaries
    Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int BytesPerPixel = 4;
    int BitmapMemorySize = (Width * Height) * BytesPerPixel;

    //
    Buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

}

void 
WIN32UpdateWindow(win32_buffer Buffer, HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
    /*
        Render BitmapMemory
    */
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;

    // Update the desplay by drawing to a destination rectangle
    // . Write the Bitmap to the Window
    StretchDIBits(
        DeviceContext,
        0, 0, Buffer.BitmapWidth, Buffer.BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        Buffer.BitmapMemory,
        &Buffer.BitmapInfo,
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
    case WM_SIZE:
    {
        // User resized the window
        OutputDebugString("WM_SIZE\n");

        RECT ClientRect;
        GetClientRect(Window, &ClientRect);

        int Height = ClientRect.bottom - ClientRect.top;
        int Width = ClientRect.right - ClientRect.left;

        WIN32ResizeDIBSection(&GlobalBackbuffer, Width, Height);
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

        RECT ClientRect;
        GetClientRect(Window, &ClientRect);

        HDC DeviceContext = BeginPaint(Window, &Paint);

        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;

        WIN32UpdateWindow(GlobalBackbuffer, DeviceContext, &ClientRect, X, Y, Width, Height);

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
    WNDCLASS WindowClass;

    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWinCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "CRenderWindowClass";

    GlobalBackbuffer.BytesPerPixel = 4;

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
            Running = true;

            int XOffset = 0;
            int YOffset = 0;

            // Handle message queue
            while (Running)
            {
                MSG Message;

                // PeekMessage does not block when there is no message
                // . PM_REMOVE, remove the message from the queue
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        Running = false;
                    }

                    // Parsing for keyboard input
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                RenderGradient(GlobalBackbuffer, XOffset, YOffset);

                RECT ClientRect;
                GetClientRect(Window, &ClientRect);

                HDC DeviceContext = GetDC(Window);

                int WindowWidth = ClientRect.right - ClientRect.left;
                int WindowHeight = ClientRect.bottom - ClientRect.top;
                WIN32UpdateWindow(GlobalBackbuffer, DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);

                ReleaseDC(Window, DeviceContext);

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