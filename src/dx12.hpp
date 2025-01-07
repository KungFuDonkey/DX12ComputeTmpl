#pragma once
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <unordered_map>
#include <filesystem>
#include <wrl.h>

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
    CPURead = 1,
    CPUWrite = 2,
    GPUConstant = 4
};

BufferFlags operator|(BufferFlags x, BufferFlags y) { return (BufferFlags)((uint32_t)x | (uint32_t)y); }


struct DX12Buffer
{
    ComPtr<ID3D12Resource> buffer;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

template<typename T>
struct Buffer
{
    DX12Buffer gpuBuffer;
    DX12Buffer hostUploadBuffer;
    DX12Buffer hostReadbackBuffer;
    uint32_t length = 0;
    BufferFlags flags = 0;
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

    static DX12Env InitializeDX12()
    {
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
        printf("Using GPU: %ls\n", adapterDesc.Description);

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

        return DX12Env{
            d3d12Debug,
            factory,
            adapter,
            adapterDesc,
            device,
            options1,
            library,
            compiler,
            includeHandler,
            utils,
            infoQueue,
            commandQueue,
            commandAllocator,
            commandList
        };
    }

    ShaderCompilation CreateShaderCompilation(LPCWSTR fileName, LPCWSTR entrypoint, ShaderDefines& defines)
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

        HRESULT hr = compiler->Compile(sourceBlob.Get(), fileName, entrypoint, L"cs_6_7", arguments, _countof(arguments), defs, numDefines, includeHandler.Get(), &result);
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

    Shader CompileShader(LPCWSTR fileName, LPCWSTR entrypoint, ShaderDefines& defines)
    {
        ShaderCompilation shaderCompile = CreateShaderCompilation(fileName, entrypoint, defines);

        if (!shaderCompile.compileSuccess)
        {
            shaderCompile.PrintCompilationErrors();

            exit(-1);
        }

        return shaderCompile.GetShader(*this);
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

    bool FlushQueue()
    {
        commandList->Close();

        // Execute the list in the command queue
        ID3D12CommandList* commandLists[] = { commandList.Get() };
        queue->ExecuteCommandLists(_countof(commandLists), commandLists);

        // Fence to wait on the gpu to finish
        ComPtr<ID3D12Fence> fence;
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        queue->Signal(fence.Get(), 1);

        // Wait on the GPU
        HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(1, handle);
        WaitForSingleObject(handle, INFINITE);

        bool success = true;

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
                printf("GPU Error: %s \n", message->pDescription);
                success = false;
            }

            free(message);
        }

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