#include "dx12.hpp"

SETUP_DX12;

struct ConstantInput
{
	UINT numElements;
	UINT sortPass;
};

int main()
{
	DX12Env dx12 = DX12Env::InitializeDX12();

	std::srand(0xa4f5d4);

	ConstantInput constantInput = {3840, 0};
	
	// Initialization constants for dispatch

	ShaderDefines defines;
	
	Shader initGlobalHistogram = dx12.CompileShader(L"OneSweep.hlsl", L"InitGlobalHistogram", defines);
	Shader scanGlobalHistogram = dx12.CompileShader(L"OneSweep.hlsl", L"ScanGlobalHistogram", defines);
	Shader sortStep = dx12.CompileShader(L"OneSweep.hlsl", L"SortStep", defines);
	
	Buffer<ConstantInput> constantUploadBuffer = dx12.CreateBuffer<ConstantInput>(1, Upload);
	Buffer<ConstantInput> constantBuffer = dx12.CreateBuffer<ConstantInput>(1, GPUConstant);
	Buffer<UINT> keysUploadBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Upload);
	Buffer<UINT> valuesUploadBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Upload);
	Buffer<UINT> keysBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> valuesBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> keysOutBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> valuesOutBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, GPUReadWrite);
	Buffer<UINT> globalHistogramBuffer = dx12.CreateBuffer<UINT>(4 * 256 + 4, GPUReadWrite);
	Buffer<UINT> globalHistogramReadbackBuffer = dx12.CreateBuffer<UINT>(4 * 256 + 4, Readback);

	uint32_t numThreadGroups = ((constantInput.numElements - 1) / 3840) + 1;
	const uint32_t numPasses = 4;

	Buffer<UINT> passHistogramBuffer = dx12.CreateBuffer<UINT>((numThreadGroups * 256) * numPasses, GPUReadWrite);
	Buffer<UINT> passHistogramReadbackBuffer = dx12.CreateBuffer<UINT>((numThreadGroups * 256) * numPasses, Readback);
	Buffer<UINT> keysReadbackBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Readback);
	Buffer<UINT> valuesReadbackBuffer = dx12.CreateBuffer<UINT>(constantInput.numElements, Readback);

	const UINT debugBuffers = 4;
	Buffer<UINT> debugBuffer[debugBuffers] = {
		 dx12.CreateBuffer<UINT>(256, GPUReadWrite),
		 dx12.CreateBuffer<UINT>(256, GPUReadWrite),
		 dx12.CreateBuffer<UINT>(256, GPUReadWrite),
		 dx12.CreateBuffer<UINT>(256, GPUReadWrite)
	};
	Buffer<UINT> debugOutputBuffer[debugBuffers] = {
		 dx12.CreateBuffer<UINT>(256, Readback),
		 dx12.CreateBuffer<UINT>(256, Readback),
		 dx12.CreateBuffer<UINT>(256, Readback),
		 dx12.CreateBuffer<UINT>(256, Readback)
	};

	UINT origionalValues[256] = { 0 };
	{
		BufferView<UINT> keysView = dx12.GetBufferView(keysUploadBuffer);
		BufferView<UINT> valuesView = dx12.GetBufferView(valuesUploadBuffer);
		for (UINT j = 0; j < constantInput.numElements; j++)
		{
			keysView[j] = std::rand() % 255;
			valuesView[j] = j;
			origionalValues[keysView[j]]++;
		}

		BufferView<ConstantInput> constantView = dx12.GetBufferView(constantUploadBuffer);
		constantView[0] = constantInput;
	}

	// initialize shader
	dx12.SetShader(initGlobalHistogram);

	// upload buffers
	dx12.UploadBuffer(keysUploadBuffer, keysBuffer);
	dx12.UploadBuffer(valuesUploadBuffer, valuesBuffer);
	dx12.UploadBuffer(constantUploadBuffer, constantBuffer);

	// set buffer inputs
	UINT bufferIndex = 0;
	dx12.SetBuffer(bufferIndex++, constantBuffer);
	dx12.SetBuffer(bufferIndex++, keysBuffer);
	dx12.SetBuffer(bufferIndex++, valuesBuffer);
	dx12.SetBuffer(bufferIndex++, keysOutBuffer);
	dx12.SetBuffer(bufferIndex++, valuesOutBuffer);
	dx12.SetBuffer(bufferIndex++, globalHistogramBuffer);
	dx12.SetBuffer(bufferIndex++, passHistogramBuffer);
	for (uint32_t i = 0; i < debugBuffers; i++)
	{
		dx12.SetBuffer(bufferIndex++, debugBuffer[i]);
	}

	// dispatch the shader
	dx12.DispatchShader(constantInput.numElements / 32, 1, 1);

	// set second shader
	dx12.SetShader(scanGlobalHistogram);
	
	// set buffer inputs
	bufferIndex = 0;
	dx12.SetBuffer(bufferIndex++, constantBuffer);
	dx12.SetBuffer(bufferIndex++, keysBuffer);
	dx12.SetBuffer(bufferIndex++, valuesBuffer);
	dx12.SetBuffer(bufferIndex++, keysOutBuffer);
	dx12.SetBuffer(bufferIndex++, valuesOutBuffer);
	dx12.SetBuffer(bufferIndex++, globalHistogramBuffer);
	dx12.SetBuffer(bufferIndex++, passHistogramBuffer);
	for (uint32_t i = 0; i < debugBuffers; i++)
	{
		dx12.SetBuffer(bufferIndex++, debugBuffer[i]);
	}

	// dispatch the shader
	dx12.DispatchShader(4, 1, 1);

	// set third shader
	dx12.SetShader(sortStep);

	// set buffer inputs
	bufferIndex = 0;
	dx12.SetBuffer(bufferIndex++, constantBuffer);
	dx12.SetBuffer(bufferIndex++, keysBuffer);
	dx12.SetBuffer(bufferIndex++, valuesBuffer);
	dx12.SetBuffer(bufferIndex++, keysOutBuffer);
	dx12.SetBuffer(bufferIndex++, valuesOutBuffer);
	dx12.SetBuffer(bufferIndex++, globalHistogramBuffer);
	dx12.SetBuffer(bufferIndex++, passHistogramBuffer);
	for (uint32_t i = 0; i < debugBuffers; i++)
	{
		dx12.SetBuffer(bufferIndex++, debugBuffer[i]);
	}

	// dispatch the shader
	dx12.DispatchShader(1, 1, 1);

	// add readback
	dx12.ReadbackBuffer(keysBuffer, keysReadbackBuffer);
	dx12.ReadbackBuffer(valuesBuffer, valuesReadbackBuffer);
	dx12.ReadbackBuffer(globalHistogramBuffer, globalHistogramReadbackBuffer);
	dx12.ReadbackBuffer(passHistogramBuffer, passHistogramReadbackBuffer);
	for (uint32_t i = 0; i < debugBuffers; i++)
	{
		dx12.ReadbackBuffer(debugBuffer[i], debugOutputBuffer[i]);
	}

	// execute all commands
	if (!dx12.FlushQueue())
	{
		return -1;
	}

	{
		BufferView<UINT> keysView = dx12.GetBufferView(keysReadbackBuffer);
		BufferView<UINT> valuesView = dx12.GetBufferView(valuesReadbackBuffer);
		BufferView<UINT> histogramView = dx12.GetBufferView(globalHistogramReadbackBuffer);
		BufferView<UINT> passHistogramView = dx12.GetBufferView(passHistogramReadbackBuffer);
		BufferView<UINT> debugBufferView[debugBuffers] =
		{
			dx12.GetBufferView(debugOutputBuffer[0]),
			dx12.GetBufferView(debugOutputBuffer[1]),
			dx12.GetBufferView(debugOutputBuffer[2]),
			dx12.GetBufferView(debugOutputBuffer[3])
		};

		/*printf("[");
		for (UINT j = 0; j < constantInput.numElements - 1; j++)
		{
			printf("(%i, %i), ", keysView[j], valuesView[j]);
		}
		printf("(%i, %i)]\n", keysView[constantInput.numElements - 1], valuesView[constantInput.numElements - 1]);*/

		for (UINT j = 0; j < 256; j++)
		{
			printf("(%i, %i): (%i) (%i,%i,%i,%i)\n", j, origionalValues[j], histogramView[j],  debugBufferView[0][j], debugBufferView[1][j], debugBufferView[2][j], debugBufferView[3][j]);
		}
		printf("\n");
	}
	
	return 0;
}
