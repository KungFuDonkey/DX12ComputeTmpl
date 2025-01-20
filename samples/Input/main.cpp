#include "dx12.hpp"

SETUP_DX12;

int main()
{
	DX12Env dx12 = DX12Env::InitializeDX12();

	dx12.CreateScreen(640, 480);

	Shader shader = dx12.CompileShader(L"Shader.hlsl", L"main");

	while (true)
	{
		dx12.Render();
		dx12.FlushQueue();
	}
}