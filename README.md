# DX12ComputeTmpl
A minimalistic template for creating Compute Shaders in DirectX12 for VS without the exhausting setup for device, buffers, etc...

## Installation
After downloading run InstallPackages.cmd (requires C# to be installed in VS for dotnet CLI) which installs the [Microsoft.Direct3D.D3D12](https://www.nuget.org/packages/Microsoft.Direct3D.D3D12/) package

## Features

### Easy initialization
Instantiating the environment for dx12 can be done with the following lines:

```c++
// paste this anywhere in a .cpp file
SETUP_DX12;

// run this at the start of the program
DX12Env dx12 = DX12Env::InitializeDX12();
```

Which initializes most required dx12 structures

### Compilation
The shaders in the `Shaders` folder can be compiled by the following line:

```c++
Shader shader = dx12.CompileShader(L"Shader.hlsl", L"main", defines);
```

Optionally, in the `ShaderDefines` struct you can add definitions:

```c++
defines.AddDefine(L"THREAD_GROUP_SIZE_X", threadGroupSizeX);
defines.AddDefine(L"THREAD_GROUP_SIZE_Y", threadGroupSizeY);
defines.AddDefine(L"THREAD_GROUP_SIZE_Z", threadGroupSizeZ);
...
```

If compilation fails the program will terminate.

### Buffers
The framework has 4 buffers, GPUReadWrite, GPUConstant, Upload, and Readback.
Creating these buffers of a specific type can be done like this:

```c++
Buffer<ConstantInput> constantUploadBuffer = dx12.CreateBuffer<ConstantInput>(1, Upload);
Buffer<ConstantInput> constantBuffer = dx12.CreateBuffer<ConstantInput>(1, GPUConstant);
Buffer<float> uploadBuffer = dx12.CreateBuffer<float>(totalSize * 4, Upload);
Buffer<float> gpuBuffer = dx12.CreateBuffer<float>(totalSize * 4, GPUReadWrite);
Buffer<float> readbackBuffer = dx12.CreateBuffer<float>(totalSize * 4, Readback);
```
No need to specifically define the number of bytes you want, you just need to state the number of elements that you need.

The Upload and Readback buffers are buffers to export data to and from the GPU respectively, and can be accessed by the `BufferView` struct:

```c++
BufferView<float> uploadView = dx12.GetBufferView(uploadBuffer);
upload_view[0] = // some data you want to push to the GPU
```

To upload to the GPU you can use the `UploadBuffer` function to upload from a `Upload` buffer to a `GPU` buffer:

```c++
dx12.UploadBuffer(uploadBuffer, gpuBuffer);
```

To readback a buffer from the GPU you can use the `ReadbackBuffer` function to readback from a `GPU` buffer to a `Readback` buffer:

```c++
dx12.ReadbackBuffer(gpuBuffer, readbackBuffer);
```

### Execution
A typical execution of a shader is done like this:

```c++
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
```
After which (if nothing fails), the `readbackBuffer` can be read by the CPU.

### Execution failure
If execution fails on the GPU, the error message created by dx12 will be printed to the console.

### Easy target creation
Feel free to look through the samples folder, it shows an easy way to initialize a new target for CMake
