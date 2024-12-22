#include "dx12.h"

int main()
{
	DX12Env dx12 = DX12Env::InitializeDX12();

	// Initialization constants for dispatch
	const uint32_t threadGroupSizeX	= 8;
	const uint32_t threadGroupSizeY	= 8;
	const uint32_t threadGroupSizeZ	= 1;
	const uint32_t threadGroupSize	= threadGroupSizeX * threadGroupSizeY * threadGroupSizeZ;

	const uint32_t dispatchSizeX = 4;
	const uint32_t dispatchSizeY = 4;
	const uint32_t dispatchSizeZ = 1;
	const uint32_t dispatchSize	 = dispatchSizeX * dispatchSizeY * dispatchSizeZ;

	const uint32_t totalSizeX = threadGroupSizeX * dispatchSizeX;
	const uint32_t totalSizeY = threadGroupSizeY * dispatchSizeY;
	const uint32_t totalSizeZ = threadGroupSizeZ * dispatchSizeZ;
	const uint32_t totalSize = threadGroupSize * dispatchSize;

	ShaderDefines defines;
	defines.AddDefine(L"THREAD_GROUP_SIZE_X", threadGroupSizeX);
	defines.AddDefine(L"THREAD_GROUP_SIZE_Y", threadGroupSizeY);
	defines.AddDefine(L"THREAD_GROUP_SIZE_Z", threadGroupSizeZ);
	defines.AddDefine(L"THREAD_GROUP_SIZE", threadGroupSize);
	defines.AddDefine(L"DISPATCH_SIZE_X", dispatchSizeX);
	defines.AddDefine(L"DISPATCH_SIZE_Y", dispatchSizeY);
	defines.AddDefine(L"DISPATCH_SIZE_Z", dispatchSizeZ);
	defines.AddDefine(L"DISPATCH_SIZE", dispatchSize);
	defines.AddDefine(L"TOTAL_SIZE_X", totalSizeX);
	defines.AddDefine(L"TOTAL_SIZE_Y", totalSizeY);
	defines.AddDefine(L"TOTAL_SIZE_Z", totalSizeZ);
	defines.AddDefine(L"TOTAL_SIZE", totalSize);
	
	ShaderCompilation shaderCompile = dx12.CompileShader(L"Shader.hlsl", L"main", defines);

	if (!shaderCompile.compileSuccess)
	{
		shaderCompile.PrintCompilationErrors();
		return -1;
	}

	Shader shader = shaderCompile.GetShader(dx12, shaderCompile);

	Buffer<float> uploadBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, Upload);
	Buffer<float> gpuBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, GPU);
	Buffer<float> downloadBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, Download);

	for (int i = 0; i < 2; i++)
	{
		{
			BufferView<float> uploadView = dx12.GetBufferView(uploadBuffer);
			for (int j = 0; j < totalSize * sizeof(float); j++)
			{
				uploadView[j * 4] = (float)(j * (i + 1));
				uploadView[j * 4 + 1] = 0;
				uploadView[j * 4 + 2] = 0;
				uploadView[j * 4 + 3] = 0;
			}
		}
		
		dx12.SetShader(shader);
		dx12.UploadBuffer(uploadBuffer, gpuBuffer);
		dx12.SetBuffer(0, gpuBuffer);
		dx12.DispatchShader(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
		dx12.DownloadBuffer(gpuBuffer, downloadBuffer);
		dx12.FlushQueue();

		BufferView<float> outputView = dx12.GetBufferView(downloadBuffer);

		for (int x = 0; x < 2; x++)
			printf("uav[%d] = %.3f, %.3f, %.3f, %.3f\n", x, outputView[x * 4 + 0], outputView[x * 4 + 1], outputView[x * 4 + 2], outputView[x * 4 + 3]);
		printf("\n");
	}
	
	return 0;
}
