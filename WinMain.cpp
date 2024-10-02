#include "Engine.h"

//クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	Engine::Initialize(kClientWidth, kClientHeight);

	while (Engine::ProcessMessage())
	{

	}

	Engine::Finalize();

	return 0;
}
