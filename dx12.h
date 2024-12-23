#pragma once
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <unordered_map>

#include <wrl.h>
using namespace Microsoft::WRL;

struct DX12Env;
struct Shader;
struct ShaderCompilation;

enum BufferType
{
    GPUReadWrite,
    GPUConstant,
    Upload,
    Readback
};

template<typename T>
struct Buffer
{
    ComPtr<ID3D12Resource> gpuBuffer;
    uint32_t length = 0;
    BufferType type = BufferType::GPUReadWrite;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

template<typename T>
struct BufferView
{
    T* data;
    uint32_t length;
    Buffer<T>* buffer;

    ~BufferView()
    {
        // no support for writeback yet
        if (buffer->type == Upload)
        {
            D3D12_RANGE range = { 0, buffer->length };
            buffer->gpuBuffer->Unmap(0, &range);
        }
        else
        {
            buffer->gpuBuffer->Unmap(0, nullptr);
        }

        data = nullptr;
    }

    const T& operator[](uint32_t offset) const
    {
        return data[offset];
    }

    T& operator[](uint32_t index)
    {
        return data[index];
    }
};

struct ShaderDefines
{
    std::vector<DxcDefine> defines;

    void AddDefineStr(LPCWSTR k, const std::wstring& v);

    template<typename T>
    void AddDefine(LPCWSTR k, const T& v)
    {
        AddDefineStr(k, std::to_wstring(v));
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

    static DX12Env InitializeDX12();

    ShaderCompilation CreateShaderCompilation(LPCWSTR fileName, LPCWSTR entrypoint, ShaderDefines& defines);

    Shader CompileShader(LPCWSTR fileName, LPCWSTR entrypoint, ShaderDefines& defines);

    template<typename T>
    Buffer<T> CreateBuffer(uint32_t length, BufferType type)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(T) * length;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
        D3D12_HEAP_PROPERTIES properties = {};
        properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        properties.CreationNodeMask = 0;
        properties.VisibleNodeMask = 0;

        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

        if (type == GPUConstant)
        {
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        }
        else if (type == Upload)
        {
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;
            properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            state = D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        else if (type == Readback)
        {
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;
            properties.Type = D3D12_HEAP_TYPE_READBACK;
            state = D3D12_RESOURCE_STATE_COPY_DEST;
        }
    
        ComPtr<ID3D12Resource> mGPUResource;
        this->device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&mGPUResource));

        return {
            mGPUResource,
            length,
            type,
            state
        };
    }

    void SetShader(Shader& shader);

    void DispatchShader(uint32_t x, uint32_t y = 1, uint32_t z = 1);

    bool FlushQueue();

    template<typename T>
    BufferView<T> GetBufferView(Buffer<T>& buffer)
    {
        T* data = nullptr;
        D3D12_RANGE range = { 0, buffer.length };

        buffer.gpuBuffer->Map(0, &range, reinterpret_cast<void**>(&data));
        
        return { data, buffer.length, &buffer };
    }

    template<typename T>
    void SetBuffer(uint32_t index, Buffer<T> buffer)
    {
        if (buffer.type == GPUReadWrite)
        {
            this->commandList->SetComputeRootUnorderedAccessView(index, buffer.gpuBuffer->GetGPUVirtualAddress());
        }
        else if (buffer.type == GPUConstant)
        {
            this->commandList->SetComputeRootConstantBufferView(index, buffer.gpuBuffer->GetGPUVirtualAddress());
        }
    }
    
    template<typename T>
    void BufferToCopySrc(Buffer<T>& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.gpuBuffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
    }

    template<typename T>
    void BufferToReadWrite(Buffer<T>& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.gpuBuffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
    }

    template<typename T>
    void BufferToCopyDest(Buffer<T>& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.gpuBuffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_COPY_DEST;
        }
    }

    template<typename T>
    void BufferToConstant(Buffer<T>& buffer)
    {
        if (buffer.state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        {
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = buffer.gpuBuffer.Get();
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = buffer.state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            this->commandList->ResourceBarrier(_countof(barriers), &barriers[0]);

            buffer.state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }
    }

    template<typename T>
    void CopyBuffer(Buffer<T>& src, Buffer<T>& dst)
    {
        this->commandList->CopyResource(dst.gpuBuffer.Get(), src.gpuBuffer.Get());
    }

    template<typename T>
    void UploadBuffer(Buffer<T>& src, Buffer<T>& dst)
    {
        if (src.type != Upload)
        {
            return; // error?
        }
        if (dst.type != GPUReadWrite && dst.type != GPUConstant)
        {
            return; // error?
        }

        BufferType dstType = dst.type;

        BufferToCopyDest(dst);
        CopyBuffer(src, dst);
        if (dstType == GPUReadWrite)
        {
            BufferToReadWrite(dst);
        }
        else if (dstType == GPUConstant)
        {
            BufferToConstant(dst);
        }
    }

    template<typename T>
    void ReadbackBuffer(Buffer<T>& src, Buffer<T>& dst)
    {
        if (src.type != GPUReadWrite)
        {
            return; // error?
        }
        if (dst.type != Readback)
        {
            return; // error?
        }

        BufferToCopySrc(src);
        CopyBuffer(src, dst);
        BufferToReadWrite(src);
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

    void PrintCompilationErrors();
    
    void PrintDissasembly();
};

