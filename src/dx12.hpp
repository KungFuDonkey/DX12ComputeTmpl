#pragma once
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <unordered_map>
#include <filesystem>
#include <wrl.h>
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include "spdlog/spdlog.h"

// DELETE
#include <iostream>
using namespace Microsoft::WRL;


#define SETUP_DX12 extern "C" { __declspec(dllexport) extern const UINT     D3D12SDKVersion = 614; } \
                   extern "C" { __declspec(dllexport) extern const char8_t* D3D12SDKPath = u8".\\D3D12\\"; }

struct DX12Env;
struct Shader;
struct ShaderCompilation;

// Util to temporarily switch cwd to Shaders
struct ShaderPathUtil
{
    std::filesystem::path oldPath;

    ShaderPathUtil()
    {
        std::filesystem::current_path(std::filesystem::current_path().append("Shaders"));
    }

    ~ShaderPathUtil()
    {
        std::filesystem::current_path(std::filesystem::current_path().parent_path());
    }
};

enum BufferFlags : uint32_t
{
    BFUnknown = 0,
    CPURead = 1,
    CPUWrite = 2,
    GPUConstant = 4
};

BufferFlags operator|(BufferFlags x, BufferFlags y) { return (BufferFlags)((uint32_t)x | (uint32_t)y); }



struct Vertex
{
    float x;
    float y;
    float z;
};

struct DX12Buffer
{
    ComPtr<ID3D12Resource> buffer;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

template<typename T>
struct Buffer
{
    DX12Buffer gpuBuffer = { nullptr, D3D12_RESOURCE_STATE_COMMON };
    DX12Buffer hostUploadBuffer = { nullptr, D3D12_RESOURCE_STATE_COMMON };
    DX12Buffer hostReadbackBuffer = { nullptr, D3D12_RESOURCE_STATE_COMMON };
    uint32_t length = 0;
    BufferFlags flags = BufferFlags::BFUnknown;
};

template<typename T>
struct ReadView
{
    T* data;
    uint32_t length;
    Buffer<T>* buffer;

    bool IsClosed()
    {
        return data == nullptr;
    }

    void Close()
    {
        if (IsClosed())
        {
            return;
        }

        buffer->hostReadbackBuffer.buffer->Unmap(0, nullptr);

        data = nullptr;
    }

    ~ReadView()
    {
        Close();
    }

    const T& operator[](uint32_t offset) const
    {
        return data[offset];
    }
};

template<typename T>
struct WriteView
{
    T* data;
    uint32_t length;
    Buffer<T>* buffer;

    bool IsClosed()
    {
        return data == nullptr;
    }

    void Close()
    {
        if (IsClosed())
        {
            return;
        }

        D3D12_RANGE range = { 0, buffer->length };
        buffer->hostUploadBuffer.buffer->Unmap(0, &range);

        data = nullptr;
    }

    ~WriteView()
    {
        Close();
    }

    const T& operator[](uint32_t offset) const
    {
        return data[offset];
    }

    T& operator[](uint32_t offset)
    {
        return data[offset];
    }
};

// usually the device with the most video memory is the best card
// change this if you want another device
bool IsAdapterBetter(DXGI_ADAPTER_DESC1& prevAdapter, DXGI_ADAPTER_DESC1& newAdapter)
{
    return prevAdapter.DedicatedVideoMemory < newAdapter.DedicatedVideoMemory;
}

struct ShaderDefines
{
    std::vector<DxcDefine> defines;

    void AddDefineStr(LPCWSTR k, const std::wstring& v)
    {
        wchar_t* copy = new wchar_t[v.length() + 1];
        wcscpy_s(copy, v.length() + 1, v.c_str());
        defines.push_back({ k, copy });
    }

    template<typename T>
    void AddDefine(LPCWSTR k, const T& v)
    {
        AddDefineStr(k, std::to_wstring(v));
    }
};

struct Shader
{
    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pso;
};

struct ShaderCompilation
{
    ComPtr<IDxcBlobEncoding> sourceBlob;
    ComPtr<IDxcBlobEncoding> compileErrorBlob;
    ComPtr<ID3D12ShaderReflection> shaderReflection;
    ComPtr<IDxcBlobEncoding> disassembleBlob;
    ComPtr<IDxcBlob> shaderBlob;
    bool compileSuccess;

    Shader GetShader(DX12Env& dx12);

    void PrintCompilationErrors()
    {
        printf("Shader compile %s\n", compileSuccess ? "succeed" : "failed");
        if (compileErrorBlob)
        {
            std::string message((const char*)compileErrorBlob->GetBufferPointer(), compileErrorBlob->GetBufferSize());
            printf("%s", message.c_str());
            printf("\n");
        }
    }

    void PrintDissasembly()
    {
        UINT64 shaderRequiredFlags = shaderReflection->GetRequiresFlags();
        printf("D3D_SHADER_REQUIRES_WAVE_OPS = %d\n", (shaderRequiredFlags & D3D_SHADER_REQUIRES_WAVE_OPS) ? 1 : 0);
        printf("D3D_SHADER_REQUIRES_DOUBLES = %d\n", (shaderRequiredFlags & D3D_SHADER_REQUIRES_DOUBLES) ? 1 : 0);
        printf("\n");

        std::string message((const char*)disassembleBlob->GetBufferPointer(), disassembleBlob->GetBufferSize());
        printf("%s", message.c_str());
        printf("\n");
    }
};

struct DX12Env
{
    ComPtr<ID3D12Debug> d3d12Debug;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapterDesc;
    ComPtr<ID3D12Device2> device;
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
    ComPtr<IDxcLibrary> library;
    ComPtr<IDxcCompiler> compiler;
    ComPtr<IDxcIncludeHandler> includeHandler;
    ComPtr<IDxcUtils> utils;
    ComPtr<ID3D12InfoQueue> infoQueue;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    // For rendering a screen
    static const UINT FrameCount = 2;
    UINT frameIndex;
    HWND hwnd;
    UINT rtvDescriptorSize;
    HANDLE fenceEvent;
    UINT64 fenceValue;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12Resource> renderTargets[FrameCount];
    ComPtr<ID3D12RootSignature> renderSignature;
    ComPtr<ID3D12PipelineState> renderPipelineState;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12Fence> fence;
    Buffer<Vertex> vertices;
    D3D12_VERTEX_BUFFER_VIEW vertexView;

    static DX12Env InitializeDX12()
    {
        spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] %v");
        spdlog::info("Initialized Logger");
        spdlog::info("Initializing dx12");

        // Create debugging interface
        ComPtr<ID3D12Debug> d3d12Debug;
        D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug));
        d3d12Debug->EnableDebugLayer();

        ComPtr<IDXGIFactory4> factory;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

        ComPtr<IDXGIAdapter1> adapter;
        factory->EnumAdapters1(0, &adapter);

        DXGI_ADAPTER_DESC1 adapterDesc;
        adapter->GetDesc1(&adapterDesc);

        ComPtr<IDXGIAdapter1> adapter2;
        UINT index = 1;
        while (SUCCEEDED(factory->EnumAdapters1(index++, &adapter2)))
        {
            DXGI_ADAPTER_DESC1 adapterDesc2;
            adapter2->GetDesc1(&adapterDesc2);

            if (IsAdapterBetter(adapterDesc, adapterDesc2))
            {
                adapter = adapter2;
                adapterDesc = adapterDesc2;
            }
        }
        spdlog::info(L"Using GPU: {}", adapterDesc.Description);

        // Create device with latest features
        ComPtr<ID3D12Device2> device;
        D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

        D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
        device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS1));

        // create library
        ComPtr<IDxcLibrary> library;
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

        // create compiler
        ComPtr<IDxcCompiler> compiler;
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

        // more utils
        ComPtr<IDxcUtils> utils;
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

        // include handler for different files
        ComPtr<IDxcIncludeHandler> includeHandler;
        utils->CreateDefaultIncludeHandler(&includeHandler);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ComPtr<ID3D12CommandQueue> commandQueue;
        device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

        ComPtr<ID3D12CommandAllocator> commandAllocator;
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

        ComPtr<ID3D12GraphicsCommandList> commandList;
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

        ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
        HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));

        spdlog::info("Sucessfully Initialized dx12");
        spdlog::info("");
        spdlog::info("");

        DX12Env env;
        env.d3d12Debug = d3d12Debug;
        env.factory = factory;
        env.adapter = adapter;
        env.device = device;
        env.options1 = options1;
        env.library = library;
        env.compiler = compiler;
        env.includeHandler = includeHandler;
        env.utils = utils;
        env.infoQueue = infoQueue;
        env.queue = commandQueue;
        env.commandAllocator = commandAllocator;
        env.commandList = commandList;

        return env;
    }

    ShaderCompilation CreateShaderCompilation(LPCWSTR fileName, LPCWSTR entrypoint, ShaderDefines& defines, LPCWSTR targetProfile = L"cs_6_7")
    {
        // switch cwd
        ShaderPathUtil pathUtil;

        // create a code blob
        uint32_t codePage = CP_UTF8;
        ComPtr<IDxcBlobEncoding> sourceBlob;
        library->CreateBlobFromFile(fileName, &codePage, &sourceBlob);

        ComPtr<IDxcOperationResult> result;
        LPCWSTR arguments[] =
        {
            L"-O3",
            L"-HV 2021",
            L"-I /Shaders"
            // L"-Zi",
        };

        DxcDefine* defs = defines.defines.data();
        uint32_t numDefines = (uint32_t)defines.defines.size();

        HRESULT hr = compiler->Compile(sourceBlob.Get(), fileName, entrypoint, targetProfile, arguments, _countof(arguments), defs, numDefines, includeHandler.Get(), &result);
        if (SUCCEEDED(hr))
            result->GetStatus(&hr);
        bool compileSuccess = SUCCEEDED(hr);

        ComPtr<IDxcBlobEncoding> errorBlob;
        if (SUCCEEDED(result->GetErrorBuffer(&errorBlob)) && errorBlob)
        {
            if (!compileSuccess)
                return {
                sourceBlob,
                errorBlob,
                nullptr,
                nullptr,
                nullptr,
                compileSuccess
            };
        }

        // retrieve compiled result
        ComPtr<IDxcBlob> shaderBlob;
        result->GetResult(&shaderBlob);

        // get reflection code
        DxcBuffer dxcBuffer{ .Ptr = shaderBlob->GetBufferPointer(), .Size = shaderBlob->GetBufferSize(), .Encoding = DXC_CP_ACP };
        ComPtr<ID3D12ShaderReflection> shaderReflection;
        utils->CreateReflection(&dxcBuffer, IID_PPV_ARGS(&shaderReflection));

        ComPtr<IDxcBlobEncoding> disassembleBlob;
        compiler->Disassemble(shaderBlob.Get(), &disassembleBlob);

        return{
            sourceBlob,
            errorBlob,
            shaderReflection,
            disassembleBlob,
            shaderBlob,
            compileSuccess
        };
    }

    Shader CompileShader(LPCWSTR fileName, LPCWSTR entryPoint, ShaderDefines& defines, LPCWSTR targetProfile = L"cs_6_7")
    {
        ShaderCompilation shaderCompile = CreateShaderCompilation(fileName, entryPoint, defines, targetProfile);

        if (!shaderCompile.compileSuccess)
        {
            shaderCompile.PrintCompilationErrors();

            exit(-1);
        }

        return shaderCompile.GetShader(*this);
    }

    Shader CompileShader(LPCWSTR fileName, LPCWSTR entryPoint, LPCWSTR targetProfile = L"cs_6_7")
    {
        ShaderDefines defines;
        return CompileShader(fileName, entryPoint, defines, targetProfile);
    }

    void CreateScreen(int width, int height)
    {
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = D3D12_MIN_DEPTH;
        viewport.MaxDepth = D3D12_MAX_DEPTH;

        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = static_cast<LONG>(width);
        scissorRect.bottom = static_cast<LONG>(height);
        

        hwnd = ::CreateWindowA("Static", "DX12ComputeTmpl", WS_VISIBLE, 0, 0, width, height, NULL, NULL, NULL, NULL);

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> tempSwapChain;
        if (!SUCCEEDED(factory->CreateSwapChainForHwnd(
            queue.Get(),
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &tempSwapChain
        )))
        {
            spdlog::error("Failed to create swap chain");
            return;
        }

        // no fullscreen
        if (!SUCCEEDED(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
        {
            spdlog::error("Failed to create window association");
            return;
        }

        tempSwapChain.As(&swapChain);

        frameIndex = swapChain->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        auto rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT n = 0; n < FrameCount; n++)
        {
            swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n]));
            device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += rtvDescriptorSize;
        }

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.NumParameters = 0;
        rootSignatureDesc.pParameters = nullptr;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        if (!SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            spdlog::error("Failed to serialize root signature");
            return;
        }

        if (!SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&renderSignature))))
        {
            spdlog::error("Failed to create root signature");
            return;
        }

        ShaderDefines defines;
        ShaderCompilation vertexShader = CreateShaderCompilation(L"C:\\Users\\sietz\\source\\repos\\DX12ComputeTmpl\\src\\renderShaders.hlsl", L"vsMain", defines, L"vs_6_7");
        ShaderCompilation pixelShader = CreateShaderCompilation(L"C:\\Users\\sietz\\source\\repos\\DX12ComputeTmpl\\src\\renderShaders.hlsl", L"psMain", defines, L"ps_6_7");

        if (!vertexShader.compileSuccess)
        {
            vertexShader.PrintCompilationErrors();
            exit(-1);
        }
        if (!pixelShader.compileSuccess)
        {
            pixelShader.PrintCompilationErrors();
            exit(-1);
        }

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };


        D3D12_SHADER_BYTECODE vertexByteCode;
        vertexByteCode.pShaderBytecode = vertexShader.shaderBlob.Get()->GetBufferPointer();
        vertexByteCode.BytecodeLength = vertexShader.shaderBlob.Get()->GetBufferSize();

        D3D12_SHADER_BYTECODE pixelByteCode;
        pixelByteCode.pShaderBytecode = pixelShader.shaderBlob.Get()->GetBufferPointer();
        pixelByteCode.BytecodeLength = pixelShader.shaderBlob.Get()->GetBufferSize();

        D3D12_RASTERIZER_DESC rasterizerState;
        rasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerState.FrontCounterClockwise = FALSE;
        rasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerState.DepthClipEnable = TRUE;
        rasterizerState.MultisampleEnable = FALSE;
        rasterizerState.AntialiasedLineEnable = FALSE;
        rasterizerState.ForcedSampleCount = 0;
        rasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_BLEND_DESC blendDesc;
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = renderSignature.Get();
        psoDesc.VS = vertexByteCode;
        psoDesc.PS = pixelByteCode;
        psoDesc.RasterizerState = rasterizerState;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        if (!SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipelineState))))
        {
            spdlog::error("Failed to create graphics pipeline state");
            ReadMessages();
            return;
        }

        vertices = CreateBuffer<Vertex>(4, BufferFlags::CPUWrite);

        {
            auto vertexView = GetWriteView(vertices);
            vertexView[0] = { 0.0f, 0.25f, 0.0f };
            vertexView[1] = { 0.25f, -0.25f, 0.0f };
            vertexView[2] = { -0.25f, -0.25f, 0.0f };
            vertexView[3] = { 0.0f, -0.5f, 0.0f };
        }

        UploadBuffer(vertices);

        if (vertices.gpuBuffer.state != D3D12_RESOURCE_STATE_GENERIC_READ)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = vertices.gpuBuffer.buffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = vertices.gpuBuffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            vertices.gpuBuffer.state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }
        
        vertexView.BufferLocation = vertices.gpuBuffer.buffer->GetGPUVirtualAddress();
        vertexView.StrideInBytes = sizeof(Vertex);
        vertexView.SizeInBytes = vertices.length * sizeof(Vertex);
    }

    void Render()
    {
        commandList->SetGraphicsRootSignature(renderSignature.Get());
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = renderTargets[frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
        rtvHandle.ptr = rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + (frameIndex * rtvDescriptorSize);

        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vertexView);
        commandList->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER barrier2 = {};
        barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier2.Transition.pResource = renderTargets[frameIndex].Get();
        barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &barrier2);

        frameIndex = frameIndex == 1 ? 0 : 1;
    }
    

    template<typename T>
    Buffer<T> CreateBuffer(uint32_t length, BufferFlags flags)
    {
        D3D12_RESOURCE_DESC gpuDesc = {};
        gpuDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        gpuDesc.Alignment = 0;
        gpuDesc.Width = sizeof(T) * length;
        gpuDesc.Height = 1;
        gpuDesc.DepthOrArraySize = 1;
        gpuDesc.MipLevels = 1;
        gpuDesc.Format = DXGI_FORMAT_UNKNOWN;
        gpuDesc.SampleDesc.Count = 1;
        gpuDesc.SampleDesc.Quality = 0;
        gpuDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        gpuDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
        D3D12_HEAP_PROPERTIES gpuProperties = {};
        gpuProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        gpuProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        gpuProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        gpuProperties.CreationNodeMask = 0;
        gpuProperties.VisibleNodeMask = 0;

        D3D12_RESOURCE_STATES gpuState = D3D12_RESOURCE_STATE_COMMON;

        if (flags & GPUConstant)
        {
            gpuDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        }

        ComPtr<ID3D12Resource> mGPUResource;
        this->device->CreateCommittedResource(&gpuProperties, D3D12_HEAP_FLAG_NONE, &gpuDesc, gpuState, nullptr, IID_PPV_ARGS(&mGPUResource));

        D3D12_RESOURCE_STATES hostUploadState = D3D12_RESOURCE_STATE_COPY_SOURCE;
        D3D12_RESOURCE_STATES hostReadbackState = D3D12_RESOURCE_STATE_COPY_DEST;
        ComPtr<ID3D12Resource> mHostUploadResource;
        ComPtr<ID3D12Resource> mHostReadbackResource;

        if (flags & CPUWrite)
        {
            D3D12_RESOURCE_DESC cpuDesc = {};
            cpuDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            cpuDesc.Alignment = 0;
            cpuDesc.Width = sizeof(T) * length;
            cpuDesc.Height = 1;
            cpuDesc.DepthOrArraySize = 1;
            cpuDesc.MipLevels = 1;
            cpuDesc.Format = DXGI_FORMAT_UNKNOWN;
            cpuDesc.SampleDesc.Count = 1;
            cpuDesc.SampleDesc.Quality = 0;
            cpuDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            cpuDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES cpuProperties = {};
            cpuProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            cpuProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            cpuProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            cpuProperties.CreationNodeMask = 0;
            cpuProperties.VisibleNodeMask = 0;

            this->device->CreateCommittedResource(&cpuProperties, D3D12_HEAP_FLAG_NONE, &cpuDesc, hostUploadState, nullptr, IID_PPV_ARGS(&mHostUploadResource));
        }    

        if (flags & CPURead)
        {
            D3D12_RESOURCE_DESC cpuDesc = {};
            cpuDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            cpuDesc.Alignment = 0;
            cpuDesc.Width = sizeof(T) * length;
            cpuDesc.Height = 1;
            cpuDesc.DepthOrArraySize = 1;
            cpuDesc.MipLevels = 1;
            cpuDesc.Format = DXGI_FORMAT_UNKNOWN;
            cpuDesc.SampleDesc.Count = 1;
            cpuDesc.SampleDesc.Quality = 0;
            cpuDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            cpuDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES cpuProperties = {};
            cpuProperties.Type = D3D12_HEAP_TYPE_READBACK;
            cpuProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            cpuProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            cpuProperties.CreationNodeMask = 0;
            cpuProperties.VisibleNodeMask = 0;

            this->device->CreateCommittedResource(&cpuProperties, D3D12_HEAP_FLAG_NONE, &cpuDesc, hostReadbackState, nullptr, IID_PPV_ARGS(&mHostReadbackResource));
        }
        
        return {
            { mGPUResource, gpuState },
            { mHostUploadResource, hostUploadState },
            { mHostReadbackResource, hostReadbackState },
            length,
            flags,
        };
    }

    void SetShader(Shader& shader)
    {
        commandList->SetComputeRootSignature(shader.rootSignature.Get());
        commandList->SetPipelineState(shader.pso.Get());
    }

    void DispatchShader(uint32_t x, uint32_t y = 1, uint32_t z = 1)
    {
        commandList->Dispatch(x, y, z);
    }

    bool ReadMessages()
    {
        bool noErrors = true;
        // Check for errors in the info queue
        SIZE_T messageLength;
        UINT i = 0;
        while (SUCCEEDED(infoQueue->GetMessage(i++, nullptr, &messageLength)))
        {
            UINT index = i - 1;
            D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageLength);
            infoQueue->GetMessage(index, message, &messageLength);

            if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR)
            {
                spdlog::error("{}", message->pDescription);
                noErrors = false;
            }

            free(message);
        }

        return noErrors;
    }

    bool FlushQueue()
    {
        commandList->Close();

        // Execute the list in the command queue
        ID3D12CommandList* commandLists[] = { commandList.Get() };
        queue->ExecuteCommandLists(_countof(commandLists), commandLists);

        // Fence to wait on the gpu to finish
        ComPtr<ID3D12Fence> fence;
        if (!device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))
        {
            spdlog::error("Failed to create fence");
            ReadMessages();
            exit(-1);
        }
        queue->Signal(fence.Get(), 1);

        // Wait on the GPU
        HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(1, handle);
        WaitForSingleObject(handle, INFINITE);

        bool success = ReadMessages();

        commandList->Reset(commandAllocator.Get(), nullptr);

        return success;
    }

    template<typename T>
    ReadView<T> GetReadView(Buffer<T>& buffer)
    {
        T* data = nullptr;

        if (buffer.flags & CPURead)
        {
            D3D12_RANGE range = { 0, buffer.length };

            buffer.hostReadbackBuffer.buffer->Map(0, &range, reinterpret_cast<void**>(&data));
        }

        return { data, buffer.length, &buffer };
    }

    template<typename T>
    WriteView<T> GetWriteView(Buffer<T>& buffer)
    {
        T* data = nullptr;

        if (buffer.flags & CPUWrite)
        {
            D3D12_RANGE range = { 0, buffer.length };

            buffer.hostUploadBuffer.buffer->Map(0, &range, reinterpret_cast<void**>(&data));
        }
        return { data, buffer.length, &buffer };
    }

    template<typename T>
    void SetBuffer(uint32_t index, Buffer<T>& buffer)
    {
        if (buffer.flags & GPUConstant)
        {
            this->commandList->SetComputeRootConstantBufferView(index, buffer.gpuBuffer.buffer->GetGPUVirtualAddress());
        }
        else
        {
            this->commandList->SetComputeRootUnorderedAccessView(index, buffer.gpuBuffer.buffer->GetGPUVirtualAddress());
        }
    }
    
    void BufferToCopySrc(DX12Buffer& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.buffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
    }

    void BufferToReadWrite(DX12Buffer& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.buffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
    }

    void BufferToCopyDest(DX12Buffer& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.buffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_COPY_DEST;
        }
    }

    void BufferToConstant(DX12Buffer& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.buffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }
    }

    template<typename T>
    void UploadBuffer(Buffer<T>& buffer)
    {
        if ((buffer.flags & CPUWrite) == 0)
        {
            return; // error?
        }

        BufferToCopySrc(buffer.hostUploadBuffer);
        BufferToCopyDest(buffer.gpuBuffer);

        this->commandList->CopyResource(buffer.gpuBuffer.buffer.Get(), buffer.hostUploadBuffer.buffer.Get());

        // setup for use, not necessarily will upload again
        if (buffer.flags & GPUConstant)
        {
            BufferToConstant(buffer.gpuBuffer);
        }
        else
        {
            BufferToReadWrite(buffer.gpuBuffer);
        }
    }

    template<typename T>
    void ReadbackBuffer(Buffer<T>& buffer)
    {
        if ((buffer.flags & CPURead) == 0)
        {
            return; // error?
        }

        BufferToCopySrc(buffer.gpuBuffer);
        BufferToCopyDest(buffer.hostReadbackBuffer);

        this->commandList->CopyResource(buffer.hostReadbackBuffer.buffer.Get(), buffer.gpuBuffer.buffer.Get());

        // setup for reuse, not necessarily will upload again
        if (buffer.flags & GPUConstant)
        {
            BufferToConstant(buffer.gpuBuffer);
        }
        else
        {
            BufferToReadWrite(buffer.gpuBuffer);
        }
    }
};


Shader ShaderCompilation::GetShader(DX12Env& dx12)
{
    ComPtr<ID3D12RootSignature> rootSignature;
    dx12.device->CreateRootSignature(0, shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
    psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();

    ComPtr<ID3D12PipelineState> pso;
    dx12.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));

    return {
        shaderBlob,
        rootSignature,
        pso
    };
}