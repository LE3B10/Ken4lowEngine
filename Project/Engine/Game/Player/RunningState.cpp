#include "RunningState.h"

#include "Input.h"

void RunningState::Initialize()
{
	OutputDebugStringA("Entering Running State\n");
}

void RunningState::Update()
{
	OutputDebugStringA("Updating Running State\n");
}

void RunningState::HandleInput(Input* input)
{
	OutputDebugStringA("Transition to Running State\n");
}

void RunningState::Draw()
{
	OutputDebugStringA("Rendering Running State\n");
}

void RunningState::Exit()
{
	OutputDebugStringA("Exiting Running State\n");
}
