# DX12ComputeTmpl
A minimalistic template for creating Compute Shaders in DirectX12 without the exhausting setup for device, buffers, etc...

## Features

### Easy initialization
Instantiating the environment for dx12 can be done with the following line:

```c++
DX12Env dx12 = DX12Env::InitializeDX12();
```

Which initializes most required dx12 structures

### Compilation
The shaders in the `Shaders` folder can be compiled by the following line:

```c++
ShaderCompilation shaderCompile = dx12.CompileShader(L"Shader.hlsl", L"main", defines);
```

Optionally, in the `ShaderDefines` struct you can add definitions:

```c++
defines.AddDefine(L"THREAD_GROUP_SIZE_X", threadGroupSizeX);
defines.AddDefine(L"THREAD_GROUP_SIZE_Y", threadGroupSizeY);
defines.AddDefine(L"THREAD_GROUP_SIZE_Z", threadGroupSizeZ);
...
```

If compilation succeeds or fails, there are helper functions to get the final shader:

```c++
if (!shaderCompile.compileSuccess)
{
    shaderCompile.PrintCompilationErrors();
    return -1; // exit
}
Shader shader = shaderCompile.GetShader(dx12, shaderCompile);
```

### Buffers
The framework has 3 buffers, GPU, Upload, and Download.
Creating these buffers of a specific type can be done like this:

```c++
Buffer<float> uploadBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, Upload);
Buffer<float> gpuBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, GPU);
Buffer<float> downloadBuffer = dx12.CreateBuffer<float>(totalSize * sizeof(float) * 4, Download);
```

The Upload and Download buffers are buffers to export data to and from the GPU respectively, and can be accessed by the `BufferView` struct:

```c++
BufferView<float> uploadView = dx12.GetBufferView(uploadBuffer);
upload_view[0] = // some data you want to push to the GPU
```

To upload to the GPU you can use the `UploadBuffer` function to upload from a `Upload` buffer to a `GPU` buffer:

```c++
dx12.UploadBuffer(uploadBuffer, gpuBuffer);
```

To download a buffer from the GPU you can use the `DownloadBuffer` function to download from a `GPU` buffer to a `Download` buffer:

```c++
dx12.DownloadBuffer(gpuBuffer, downloadBuffer);
```

### Execution
A typical execution of a shader is done like this:

```c++
dx12.SetShader(shader);
dx12.UploadBuffer(uploadBuffer, gpuBuffer);
dx12.SetBuffer(0, gpuBuffer);
dx12.DispatchShader(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
dx12.DownloadBuffer(gpuBuffer, downloadBuffer);
dx12.FlushQueue();
```

After which, the `downloadBuffer` can be read by the CPU.