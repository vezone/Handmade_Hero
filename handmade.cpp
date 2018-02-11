#include "handmade.h"


internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
	local_persist real32 tSine;
	int16 ToneVolume = 300;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0;
		SampleIndex < SoundBuffer->SampleCount;
		++SampleIndex)
	{
		//TODO: draw this out for people
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		tSine += 2.0f*Pi32*1.0f / WavePeriod;
	}
}

internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	//TODO: Let's see what the optimizer does

	uint8 *Row = (uint8*)Buffer->Memory;
	for (int Y = 0;
		Y < Buffer->Height;
		++Y)
	{
		uint32 *Pixel = (uint32*)Row;
		for (int X = 0;
			X < Buffer->Width;
			++X)
		{
			uint8 Blue =  (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
			//uint8 Red = (0);
			*Pixel++ = ((Green << 8) | Blue);
		}

		Row += Buffer->Pitch;
	}
}

internal void
GameUpdateAndRender(game_memory *Memory, game_input *Input, 
	game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer)
{
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state *GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		char* FileName = __FILE__; //"test.bmp";
		debug_read_file_result FileResult = DEBUGPlatformReadEntireFile(FileName);

		if (FileResult.Contents)
		{
			DEBUGPlatformWriteEntireFile("output_test.txt", FileResult.ContentsSize, FileResult.Contents);
			DEBUGPlatformFreeFileMemory(FileResult.Contents);
		}

		GameState->ToneHz = 256;

		//TODO: this is may be more appropriate to do in the platfom layer
		Memory->IsInitialized = true;
	}

	game_controller_input *Input0 = &Input->Controllers[0];
	if (Input0->IsAnalog)
	{
		GameState->BlueOffset +=  (int)(4.0f*(Input0->EndX));
		GameState->ToneHz = 256 + (int)(128.0f *(Input0->EndY));
	}
	else
	{
		//TODO: handler
	}

	if (Input0->Down.EndedDown)
	{
		GameState->GreenOffset += 1;
	}

	++GameState->BlueOffset;
	//++GameState->GreenOffset;

	GameOutputSound(SoundBuffer, GameState->ToneHz);
	RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}