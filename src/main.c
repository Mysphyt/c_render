#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

// Rename static for different use case readability
#define global_variable static
#define local_persist static
#define internal_function static

// NOTE: Define stub functions for XInput in case there is an issue loading the xinput dll
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal_function void
Win32LoadXInput(void)
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
typedef struct
{
    // NOTE: Pixels are always 32--bits wide, memory order BB GG RR xx
    BITMAPINFO BitmapInfo;
    void *BitmapMemory;
    int BitmapHeight;
    int BitmapWidth;
    int BytesPerPixel;
    int Pitch;
} win32_backbuffer;

typedef struct
{
    int Width;
    int Height;
} win32_window_dimension;

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

typedef float real32;
typedef double real64;

#define PI 3.14159265359f

global_variable bool GlobalRunning;
global_variable win32_backbuffer GlobalBackbuffer;
global_variable int GlobalXOffset = 0;
global_variable int GlobalYOffset = 0;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

global_variable uint32 RunningSampleIndex = 0;

internal_function void
Win32InitDirectSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    LPDIRECTSOUND DirectSound;
    if (SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
        WAVEFORMATEX WaveFormat = {};
        WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
        WaveFormat.nChannels = 2;
        WaveFormat.nSamplesPerSec = SamplesPerSecond;
        WaveFormat.wBitsPerSample = 16;
        WaveFormat.cbSize = 0;
        WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
        WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
        HRESULT ErrorCode = DirectSound->lpVtbl->SetCooperativeLevel(DirectSound, Window, DSSCL_PRIORITY);
        if (SUCCEEDED(ErrorCode))
        {
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

            // NOTE: (Windows Incantation)
            // We don't send this Buffer any data -- this is just to call SetFormat
            // putting the sound card in a mode to play correctly
            LPDIRECTSOUNDBUFFER PrimaryBuffer;
            ErrorCode = IDirectSound_CreateSoundBuffer(DirectSound, &BufferDescription, &PrimaryBuffer, 0);
            if (SUCCEEDED(ErrorCode))
            {
                ErrorCode = PrimaryBuffer->lpVtbl->SetFormat(PrimaryBuffer, &WaveFormat);
                if (SUCCEEDED(ErrorCode))
                {
                }
                else
                {
                    OutputDebugStringA("Failed to set format for Primary DirectSound Buffer");
                }
            }
            else
            {
                OutputDebugStringA("Failed to create Primary DirectSound Buffer");
            }
        }
        else
        {
            OutputDebugStringA("Failed in call SetCooperativeLevel for Primary DirectSound Buffer");
        }

        // NOTE: (Windows Incantation)
        // This is the actual sound buffer, the PrimaryBuffer is just for setting the initial WaveFormat
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = 0;
        BufferDescription.dwBufferBytes = BufferSize;
        BufferDescription.lpwfxFormat = &WaveFormat;

        ErrorCode = IDirectSound_CreateSoundBuffer(DirectSound, &BufferDescription, &GlobalSecondaryBuffer, 0);
        if (SUCCEEDED(ErrorCode))
        {
        }
        else
        {
            OutputDebugStringA("Failed to create Secondary DirectSound Buffer");
        }
    }
}

internal_function win32_window_dimension
Win32GetWindowDimension(HWND Window)
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
RenderGradient(win32_backbuffer Buffer)
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
            uint8 Blue = Y + GlobalYOffset;  // (uint8)256 - ((float)(Y+YOffset) / BitmapWidth) * (float)256; //(X + XOffset);
            uint8 Green = X + GlobalXOffset; // ((float)(Y+YOffset) / BitmapHeight) * (float)256; //(Y + YOffset);
            uint8 Red = 0;                   //(XOffset - Y);

            // Set the pixel 32bit value (padding will be 00)
            *Pixel++ = ((Red << 16) | ((Green << 8) | Blue));
        }
        Row += Buffer.Pitch;
    }
}

// Win32 prefix on non-msdn functions
internal_function void
Win32ResizeDIBSection(win32_backbuffer *Buffer, int Width, int Height)
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

    // NOTE: Do I need to RESERVE and COMMIT or just COMMIT?
    Buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal_function void
Win32UpdateWindow(win32_backbuffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    /*
        Render the Backbuffer
    */
    // TODO: Aspect ratio conversion stretch modes
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,                 // Destination
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
    break;
    case WM_SYSKEYDOWN:
    {
    }
    break;
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
                GlobalYOffset++;
            }
            if (VKCode == 'A')
            {
                GlobalXOffset--;
            }
            if (VKCode == 'S')
            {
                GlobalYOffset--;
            }
            if (VKCode == 'D')
            {
                GlobalXOffset++;
            }
        }
    }
    break;
    case WM_KEYDOWN:
    {
    }
    break;
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

typedef struct
{
    int SamplesPerSecond;
    int BytesPerSample;
    int WaveVolume;
    int Hz;
    int WavePeriod;
    int HalfWavePeriod;
    int BufferSize;
} win32_sound_output;

void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    HRESULT ErrorCode;

    ErrorCode = GlobalSecondaryBuffer->lpVtbl->Lock(
        GlobalSecondaryBuffer,
        ByteToLock,
        BytesToWrite,
        &Region1, &Region1Size,
        &Region2, &Region2Size, 0);

    int16 *SampleOut;
    if (SUCCEEDED(ErrorCode))
    {
        // TODO: Assert Region(1|2)Size is valid
        SampleOut = (int16 *)Region1;

        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region1SampleCount;
             ++SampleIndex)
        {
            real32 t = 2.0f * PI * ((real32)RunningSampleIndex / (real32)SoundOutput->WavePeriod);
            real32 SineValue = sinf(t);
            int16 SampleValue = (int16)(SineValue * SoundOutput->WaveVolume); // ((RunningSampleIndex++ / HalfWavePeriod) % 2) ? WaveVolume: -WaveVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++RunningSampleIndex;
        }

        SampleOut = (int16 *)Region2;

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region2SampleCount;
             ++SampleIndex)
        {
            real32 t = 2.0f * PI * ((real32)RunningSampleIndex / (real32)SoundOutput->WavePeriod);
            real32 SineValue = sinf(t);
            int16 SampleValue = (int16)(SineValue * SoundOutput->WaveVolume); // ((RunningSampleIndex++ / HalfWavePeriod) % 2) ? WaveVolume: -WaveVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++RunningSampleIndex;
        }

        GlobalSecondaryBuffer->lpVtbl->Unlock(
            GlobalSecondaryBuffer,
            Region1, Region1Size,
            Region2, Region2Size);
    }
    else
    {
        OutputDebugString("Failed to Lock DirectSound SecondaryBuffer");
    }
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    // Set the Backbuffer resolution
    GlobalBackbuffer.BytesPerPixel = 4;
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    Win32LoadXInput();

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
            // Process is running
            GlobalRunning = true;

            HDC DeviceContext = GetDC(Window);

            win32_sound_output SineWave;

            SineWave.SamplesPerSecond = 48000;
            SineWave.BytesPerSample = sizeof(int16) * 2; // 32bit samples, 16 bit chunks to form square waves
            SineWave.WaveVolume = 3000;
            SineWave.Hz = 256;
            SineWave.WavePeriod = SineWave.SamplesPerSecond / SineWave.Hz;
            SineWave.HalfWavePeriod = SineWave.WavePeriod / 2;
            SineWave.BufferSize = SineWave.SamplesPerSecond * SineWave.BytesPerSample;

            // Init sound 2 second buffer
            Win32InitDirectSound(Window, SineWave.SamplesPerSecond, SineWave.BufferSize);

            Win32FillSoundBuffer(&SineWave, 0, SineWave.BufferSize);
            IDirectSoundBuffer_Play(GlobalSecondaryBuffer, 0, 0, DSBPLAY_LOOPING);

            // Square wave data
            /*
            int SquareWaveVolume = 16000;
            int Hz = 256; // C
            int SquareWavePeriod = SamplesPerSecond / Hz;
            int HalfSquareWavePeriod = SquareWavePeriod / 2;
            */
            // uint to wrap back to 0, goes up forever
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

                // Poll Controller input

                /*
                // NOTE: XInputGetState has a performance bug if no controller is plugged in

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
                */

                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(GlobalSecondaryBuffer, &PlayCursor, &WriteCursor)))
                {
                    // Where in the buffer is the RunningSampleIndex (to lock)
                    DWORD ByteToLock = (RunningSampleIndex * SineWave.BytesPerSample) % SineWave.BufferSize;
                    DWORD BytesToWrite;

                    /*
                    Square Wave

                        1 = Region 1
                        2 = Region 2

                        - ByteToLock > PlayCursor:

                        |222*------------*11111111111|
                            ^Play        ^ByteToLock


                        - ByteToLock < PlayCursor:

                        |---*111111111111111*--------|
                            ^ByteToLock     ^Play

                        - First chunk maps directly to the buffer
                            Region1Size = BytesToWrite
                            Region2Size = 0
                           _   _   _   _   _
                        |_| |_| |_| |_| |_| |_
                        ^        ^
                        |        |
                        |        |
                        |11111111| Buffer

                        - As DirectSound plays, lock and fill the already
                            played portion with the next section of the wave
                           _   _   _   _   _
                        |_| |_| |_| |_| |_| |_
                             ^   ^^    ^
                             |   ||2222|
                             |   |
                        |----|111| Buffer
                           _   _   _   _   _
                        |_| |_| |_| |_| |_| |_
                             ^   ^
                             |   |
                             |   |
                        |2222|111| Buffer

                    */
                    if (ByteToLock == PlayCursor)
                    {
                        BytesToWrite = 0;
                    }
                    else if (ByteToLock > PlayCursor)
                    {
                        // Write to the end of the buffer and then to the PlayCursor
                        BytesToWrite = (SineWave.BufferSize - ByteToLock);
                        BytesToWrite += PlayCursor;
                    }
                    else // ByteToLock < PlayCursor
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    Win32FillSoundBuffer(&SineWave, ByteToLock, BytesToWrite);
                }

                RenderGradient(GlobalBackbuffer);

                win32_window_dimension Dim = Win32GetWindowDimension(Window);
                Win32UpdateWindow(&GlobalBackbuffer, DeviceContext, Dim.Width, Dim.Height);
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