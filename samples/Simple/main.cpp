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

	Buffer<ConstantInput> constantUploadBuffer = dx12.CreateBuffer<ConstantInput>(1, Upload);
	Buffer<ConstantInput> constantBuffer = dx12.CreateBuffer<ConstantInput>(1, GPUConstant);
	Buffer<float> uploadBuffer = dx12.CreateBuffer<float>(totalSize * 4, Upload);
	Buffer<float> gpuBuffer = dx12.CreateBuffer<float>(totalSize * 4, GPUReadWrite);
	Buffer<float> readbackBuffer = dx12.CreateBuffer<float>(totalSize * 4, Readback);

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

			BufferView<ConstantInput> constantView = dx12.GetBufferView(constantUploadBuffer);
			constantView[0].divValue = 5.0f;
		}

		// initialize shader
		dx12.SetShader(shader);

		// upload buffers
		dx12.UploadBuffer(uploadBuffer, gpuBuffer);
		dx12.UploadBuffer(constantUploadBuffer, constantBuffer);

		// set buffer inputs
		dx12.SetBuffer(0, constantBuffer);
		dx12.SetBuffer(1, gpuBuffer);

		// dispatch the shader
		dx12.DispatchShader(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);

		// add readback
		dx12.ReadbackBuffer(gpuBuffer, readbackBuffer);

		// execute all commands
		if (!dx12.FlushQueue())
		{
			return -1;
		}

		BufferView<float> outputView = dx12.GetBufferView(readbackBuffer);

		for (int x = 0; x < 2; x++)
			printf("uav[%d] = %.3f, %.3f, %.3f, %.3f\n", x, outputView[x * 4 + 0], outputView[x * 4 + 1], outputView[x * 4 + 2], outputView[x * 4 + 3]);
		printf("\n");
	}
	
	return 0;
}
