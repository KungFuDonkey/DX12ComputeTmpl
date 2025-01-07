#include "dx12.hpp"

SETUP_DX12;

struct ConstantInput
{
	float divValue;
};

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
	
	Shader shader = dx12.CompileShader(L"Shader.hlsl", L"main", defines);

	Buffer<ConstantInput> constantBuffer = dx12.CreateBuffer<ConstantInput>(1, GPUConstant | CPUWrite);
	Buffer<float> gpuBuffer = dx12.CreateBuffer<float>(totalSize * 4, CPURead | CPUWrite);

	for (int i = 0; i < 2; i++)
	{
		WriteView<float> gpuBufferView = dx12.GetWriteView(gpuBuffer);
		for (int j = 0; j < totalSize; j++)
		{
			gpuBufferView[j * 4] = (float)(j * (i + 1));
			gpuBufferView[j * 4 + 1] = 0;
			gpuBufferView[j * 4 + 2] = 0;
			gpuBufferView[j * 4 + 3] = 0;
		}
		gpuBufferView.Close();

		WriteView<ConstantInput> constantView = dx12.GetWriteView(constantBuffer);
		constantView[0].divValue = 5.0f;
		constantView.Close();

		// initialize shader
		dx12.SetShader(shader);

		// upload buffers
		dx12.UploadBuffer(gpuBuffer);
		dx12.UploadBuffer(constantBuffer);

		// set buffer inputs
		dx12.SetBuffer(0, constantBuffer);
		dx12.SetBuffer(1, gpuBuffer);

		// dispatch the shader
		dx12.DispatchShader(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);

		// add readback
		dx12.ReadbackBuffer(gpuBuffer);

		// execute all commands
		if (!dx12.FlushQueue())
		{
			return -1;
		}

		ReadView<float> outputView = dx12.GetReadView(gpuBuffer);

		for (int x = 0; x < 2; x++)
		{
			spdlog::info("uav[{0:d}] = {1:.3f}, {2:.3f}, {3:.3f}, {4:.3f}", x, outputView[x * 4 + 0], outputView[x * 4 + 1], outputView[x * 4 + 2], outputView[x * 4 + 3]);
		}
			
		spdlog::info("");
	}
	
	return 0;
}
