# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.15)

# Enable Hot Reload for MSVC compilers if supported. WindowsSdkVerBinPath VSCMD_ARG_TGT_ARCH
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("DX12ComputeTmpl")

add_compile_options(/utf-8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(D3D12BinPath "$ENV{WindowsSdkVerBinPath}$ENV{VSCMD_ARG_TGT_ARCH}")
set(D3D12CompilerDLL "${CMAKE_SOURCE_DIR}\\InstallPackages\\dxcompiler.dll")
set(D3D12DXILDLL "${CMAKE_SOURCE_DIR}\\InstallPackages\\dxil.dll")
set(D3D12SDKPath "$ENV{USERPROFILE}\\.nuget\\packages\\microsoft.direct3d.d3d12\\1.614.1\\build\\native\\bin\\x64")	    

# Include sub-projects.
add_subdirectory ("samples")
