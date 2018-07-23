#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

// TODO: Implement sine ourselves
#include <math.h>

#define internal		static
#define local_persist	static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int32_t int64;
typedef int64_t bool32;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float  real32;
typedef double real64;

#include "handmade.cpp"
#include "win32_handmade.h"


global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


internal debug_read_file_result
DEBUGPlatformReadEntireFile(char* FileName)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFile(FileName, GENERIC_READ, 
		FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize = {};
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 =
				SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32,
				MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			
			if (Result.Contents)
			{
				DWORD BytesRead = 0;
				if (ReadFile(FileHandle, Result.Contents,
					FileSize32, &BytesRead, 0)
					&& FileSize32 == BytesRead)
				{
					//
					Result.ContentsSize = FileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
					Result.ContentsSize = 0;
				}
			}
			else
			{
				//TODO: Logging
			}

		}
		else
		{
			//TODO: Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		//TODO: Logging
	}

	return(Result);
}

internal void 
DEBUGPlatformFreeFileMemory(void* Memory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

internal bool32
DEBUGPlatformWriteEntireFile(
	char* FileName, uint32 MemorySize, void* Memory)
{
	bool32 Result = false;

	HANDLE FileHandle = CreateFile(FileName, GENERIC_WRITE,
		0, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten = 0;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			//TODO: Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		//TODO: Logging
	}
	return(Result);
}


internal void
Wind32LoadXInput(void)
{
	// TODO: Check for windows8
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	if (!XInputLibrary)
	{
		// TODO: Diagnostic
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
	else
	{
		///TODO: Diagnostic
	}
}

internal void
Win32InitDSound(HWND Window, int32 SamplePerSecond, int32 BufferSize)
{
	// TODO: Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary)
	{
		// TODO: Get a DirectSound object!
		direct_sound_create *DirectSoundCreate =
			(direct_sound_create*)GetProcAddress(DSoundLibrary,
				"DirectSoundCreate");

		//TODO: Double-check that this works on XP - DirectSound8 or 7?
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplePerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDesciption = {};
				BufferDesciption.dwSize = sizeof(BufferDesciption);
				BufferDesciption.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// TODO: "Create" a primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesciption, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(Error))
					{
						OutputDebugStringA("Primary buffer format was set.\n");
					}
					else
					{
						//TODO: Diagnostic
					}
				}
				else
				{
					//TODO: Diagnostic
				}

			}
			else
			{
				//TODO: Diagnositc
			}
			// TODO: "Create" a seconday buffer
			DSBUFFERDESC BufferDesciption = {};
			BufferDesciption.dwSize = sizeof(BufferDesciption);
			BufferDesciption.dwFlags = 0;
			BufferDesciption.dwBufferBytes = BufferSize;
			BufferDesciption.lpwfxFormat = &WaveFormat;

			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDesciption, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error))
			{
				//TODO: Start it playing!
				OutputDebugStringA("Secondary buffer format was set.\n");
			}
		}
		else
		{
			// TODO: Diagnostic
		}

	}
	else
	{
		// TODO: Diagnostic
	}
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;

	return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	int BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height) * BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	Buffer->Pitch = Width * BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
	HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	StretchDIBits(DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
		DIB_RGB_COLORS, SRCCOPY);

}


internal LRESULT CALLBACK
Win32WindowProc(HWND    Window,
	UINT    Message,
	WPARAM  WParam,
	LPARAM  LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_SIZE:
	{

	} break;

	case WM_CLOSE:
	{
		GlobalRunning = false;
	} break;
	case WM_DESTROY:
	{
		GlobalRunning = false;
	} break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		uint32 VKCode = (uint32)WParam;
		bool WasDown = ((LParam & (1 << 30)) != 0);
		if (WasDown == true)
		{
			OutputDebugStringA("WasDown - true\n");
		}
		bool IsDown = ((LParam & (1 << 31)) == 0);

		if (WasDown != IsDown)
		{
			if (VKCode == 'W')
			{
			}
			else if (VKCode == 'A')
			{
			}
			else if (VKCode == 'S')
			{
			}
			else if (VKCode == 'D')
			{
			}
			else if (VKCode == 'Q')
			{
			}
			else if (VKCode == 'E')
			{
			}
			else if (VKCode == VK_UP)
			{
			}
			else if (VKCode == VK_DOWN)
			{
			}
			else if (VKCode == VK_RIGHT)
			{
			}
			else if (VKCode == VK_LEFT)
			{
			}
			else if (VKCode == VK_ESCAPE)
			{
				OutputDebugStringA("ESCAPE: ");
				if (IsDown)
				{
					OutputDebugStringA("IsDown");
				}
				if (WasDown)
				{
					OutputDebugStringA("WasDown");
				}
				OutputDebugStringA("\n");
			}
			else if (VKCode == VK_SPACE)
			{
			}
		}


		bool32 AltKeyWasDown = ((LParam & (1 << 29)) != 0);
		if ((VKCode == VK_F4) && AltKeyWasDown)
		{
			GlobalRunning = false;
		}
	} break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		Win32DisplayBufferInWindow(&GlobalBackuffer, DeviceContext,
			Dimension.Width, Dimension.Height);
		EndPaint(Window, &Paint);
	} break;
	default:
	{
		Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
	}
	return(Result);
}



internal void
Win32ClearBuffer(win32_sound_ouput *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0;
			ByteIndex < Region1Size;
			++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0;
			ByteIndex < Region2Size;
			++ByteIndex)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void
Win32FillSoundBuffer(win32_sound_ouput *SoundOutput,
	DWORD ByteToLock, DWORD BytesToWrite,
	game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		// TODO: assert that Region1Size/Region2Size is valid

		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;

		for (DWORD SampleIndex = 0;
			SampleIndex < Region1SampleCount;
			++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;

		for (DWORD SampleIndex = 0;
			SampleIndex < Region2SampleCount;
			++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}

}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit,
	game_button_state *NewState)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CommandLine,
	int       ShowCode)
{
	LARGE_INTEGER PerCounterFrequencyResult;
	QueryPerformanceFrequency(&PerCounterFrequencyResult);
	int64 PerCounterFrequency = PerCounterFrequencyResult.QuadPart;
	
	Wind32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = Win32WindowProc;
	WindowClass.hInstance = Instance;
	// WindowClass.hIcon
	WindowClass.lpszClassName = "HandmadeHeroWindowsClass";

	if (RegisterClass(&WindowClass))
	{
		LPCWSTR;
		HWND Window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0);

		if (Window)
		{
			HDC DeviceContext = GetDC(Window);

			// NOTE: Graphics Test
			int XOffset = 0;
			int YOffset = 0;

			win32_sound_ouput SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.BytesPerSample = sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			// TODO(Max): Pool with bitmap VirtualAlloc
			int16 *Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
				MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if INTERNAL_BUILD == 1
			LPVOID BaseAddress = 0;//(LPVOID)Gigabytes(500);
#else
			LPVOID BaseAddress = 0;
#endif
			
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes((size_t)64);
			GameMemory.TransientStorageSize = Gigabytes((size_t)1);
			
			uint64 TotalSize = GameMemory.PermanentStorageSize +
				GameMemory.TransientStorageSize;
			
			GameMemory.PermanentStorage =
				VirtualAlloc(BaseAddress, (size_t)TotalSize,
					MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8*)GameMemory.PermanentStorage +
				GameMemory.PermanentStorageSize);
			

			game_input Input[2] = {};
			game_input *OldInput = &Input[0];
			game_input *NewInput = &Input[1];

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{

				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
				uint64 LastCycleCount = __rdtsc();

				while (GlobalRunning)
				{
					MSG Message;
					while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
					{
						if (Message.message == WM_QUIT)
						{
							GlobalRunning = false;
						}

						TranslateMessage(&Message);
						DispatchMessage(&Message);
					}

					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > ArrayCount(NewInput->Controllers))
					{
						MaxControllerCount = ArrayCount(NewInput->Controllers);
					}

					for (DWORD ControllerIndex = 0;
						ControllerIndex < MaxControllerCount;
						++ControllerIndex)
					{

						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];


						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							// This conteroller is plugged in
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

							NewController->IsAnalog = true;
							NewController->StartX = OldController->EndX;
							NewController->StartY = OldController->EndY;

							real32 X;
							if (Pad->sThumbLX < 0)
							{
								X = (real32)Pad->sThumbLX / 32768.0f;
							}
							else
							{
								X = (real32)Pad->sThumbLX / 32767.0f;
							}
							NewController->MinX = NewController->MaxX = NewController->EndX = X;

							real32 Y;
							if (Pad->sThumbLX < 0)
							{
								Y = (real32)Pad->sThumbLX / 32768.0f;
							}
							else
							{
								Y = (real32)Pad->sThumbLX / 32767.0f;
							}

							NewController->MinX = NewController->MaxX = NewController->EndX = Y;

							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->Down, XINPUT_GAMEPAD_A,
								&NewController->Down);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->Right, XINPUT_GAMEPAD_B,
								&NewController->Right);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->Left, XINPUT_GAMEPAD_X,
								&NewController->Left);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->Up, XINPUT_GAMEPAD_Y,
								&NewController->Up);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
								&NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
								&OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
								&NewController->RightShoulder);


							// boolp Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							// bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						}
						else
						{
							// The controller is not available
						}
					}

					DWORD PlayCursor = 0;
					DWORD WriteCursor = 0;
					DWORD ByteToLock = 0;
					DWORD TargetCursor = 0;
					DWORD BytesToWrite = 0;
					bool32 SoundIsValid = false;
					if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor,
						&WriteCursor)))
					{
						ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample)
							% SoundOutput.SecondaryBufferSize);
						TargetCursor = ((PlayCursor
							+ (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample))
							% SoundOutput.SecondaryBufferSize);

						//TODO: We need a more accurate check than ByteToLock == PlayCursor
						if (ByteToLock > TargetCursor)
						{
							BytesToWrite = (SoundOutput.SecondaryBufferSize) - ByteToLock;
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						SoundIsValid = true;
					}

					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples = Samples;

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackuffer.Memory;
					Buffer.Width = GlobalBackuffer.Width;
					Buffer.Height = GlobalBackuffer.Height;
					Buffer.Pitch = GlobalBackuffer.Pitch;

					GameUpdateAndRender(&GameMemory, NewInput,
						&Buffer, &SoundBuffer);


					if (SoundIsValid)
					{
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(&GlobalBackuffer, DeviceContext,
						Dimension.Width,
						Dimension.Height);

					uint64 EndCycleCount = __rdtsc();

					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);

					//TODO: Display the value here
					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					int32 MSPerFrame = (int32)((1000 * CounterElapsed) / PerCounterFrequency);
					int32 FPS = (int32)(PerCounterFrequency / CounterElapsed);
					int32 MCPF = (int32)CyclesElapsed / (1000 * 1000);

					char BufferTime[256];
					wsprintf(BufferTime, "%dms/f, %df/s %dmc/f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(BufferTime);

					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
				}
			}
			else 
			{
				//No Memory or Samples
			}
		}
		
		else
		{
			//
		}
	}
	else
	{
		//
	}
	return(0);
}