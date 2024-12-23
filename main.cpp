#include "dx12.h"

struct ConstantInput
{
	UINT numElements;
};

int main()
{
	DX12Env dx12 = DX12Env::InitializeDX12();

	ConstantInput constantInput = {4096};
	
	// Initialization constants for dispatch
	const uint32_t threadGroupSizeX	= 32;
	const uint32_t dispatchSizeX    = constantInput.numElements / threadGroupSizeX;

	ShaderDefines defines;
	defines.AddDefine(L"THREAD_GROUP_SIZE_X", threadGroupSizeX);
	
	ShaderCompilation initGlobalHistogramCompile = dx12.CompileShader(L"OneSweep.hlsl", L"InitGlobalHistogram", defines);
	if (!initGlobalHistogramCompile.compileSuccess)
	{
		initGlobalHistogramCompile.PrintCompilationErrors();
		return -1;
	}
	Shader initGlobalHistogram = initGlobalHistogramCompile.GetShader(dx12, initGlobalHistogramCompile);

	ShaderCompilation scanGlobalHistogramCompile = dx12.CompileShader(L"OneSweep.hlsl", L"ScanGlobalHistogram", defines);
	if (!scanGlobalHistogramCompile.compileSuccess)
	{
		scanGlobalHistogramCompile.PrintCompilationErrors();
		return -1;
	}
	Shader scanGlobalHistogram = scanGlobalHistogramCompile.GetShader(dx12, scanGlobalHistogramCompile);

	Buffer<ConstantInput> constantUploadBuffer = dx12.CreateBuffer<ConstantInput>(1, Upload);
	Buffer<ConstantInput> constantBuffer = dx12.CreateBuffer<ConstantInput>(1, GPUConstant);
	Buffer<UINT> keysUploadBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Upload);
	Buffer<UINT> valuesUploadBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Upload);
	Buffer<UINT> keysBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> valuesBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> globalHistogramBuffer = dx12.CreateBuffer<UINT>(4 * 256, GPUReadWrite);
	Buffer<UINT> keysReadbackBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Readback);
	Buffer<UINT> valuesReadbackBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Readback);

	for (int i = 0; i < 2; i++)
	{
		{
			BufferView<UINT> keysView = dx12.GetBufferView(keysUploadBuffer);
			BufferView<UINT> valuesView = dx12.GetBufferView(valuesUploadBuffer);
			for (UINT j = 0; j < constantInput.numElements; j++)
			{
				keysView[j] = std::rand();
				valuesView[j] = j;
			}

			BufferView<ConstantInput> constantView = dx12.GetBufferView(constantUploadBuffer);
			constantView[0] = constantInput;
		}

		// initialize shader
		dx12.SetShader(shader);

		// upload buffers
		dx12.UploadBuffer(keysUploadBuffer, keysBuffer);
		dx12.UploadBuffer(valuesUploadBuffer, valuesBuffer);
		dx12.UploadBuffer(constantUploadBuffer, constantBuffer);

		// set buffer inputs
		dx12.SetBuffer(0, constantBuffer);
		dx12.SetBuffer(1, keysBuffer);
		dx12.SetBuffer(2, valuesBuffer);

		// dispatch the shader
		dx12.DispatchShader(threadGroupSizeX, 1, 1);

		// add readback
		dx12.ReadbackBuffer(keysBuffer, keysReadbackBuffer);
		dx12.ReadbackBuffer(valuesBuffer, valuesReadbackBuffer);

		// execute all commands
		if (!dx12.FlushQueue())
		{
			return -1;
		}

		{
			BufferView<UINT> keysView = dx12.GetBufferView(keysReadbackBuffer);
			BufferView<UINT> valuesView = dx12.GetBufferView(valuesReadbackBuffer);

			printf("[");
			for (UINT j = 0; j < constantInput.numElements - 1; j++)
			{
				printf("(%i, %i), ", keysView[j], valuesView[j]);
			}
			printf("(%i, %i)]\n", keysView[constantInput.numElements - 1], valuesView[constantInput.numElements - 1]);
		}
	}
	
	return 0;
}
