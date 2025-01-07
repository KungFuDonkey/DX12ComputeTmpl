
function(create_target TARGET_NAME)

	file(GLOB_RECURSE CPP_FILES *.cpp *.c *.h *.hpp)
	file(GLOB_RECURSE HLSL_FILES *.hlsl)

	add_executable(${TARGET_NAME} ${CPP_FILES} ${HLSL_FILES})

	target_sources(${TARGET_NAME} PUBLIC ${CPP_FILES} ${HLSL_FILES})

	set_source_files_properties(SOURCE ${HLSL_FILES} PROPERTIES VS_SETTINGS "ExcludedFromBuild=true")

	target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src/")

	target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/spdlog/include")

	target_link_libraries(${TARGET_NAME} d3d12.lib d3dcompiler.lib dxgi.lib dxcompiler.lib)

	if (CMAKE_VERSION VERSION_GREATER 3.12)
	  set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
	endif()

	add_custom_target(copy_shaders_${TARGET_NAME} 
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/Shaders/"
			"${CMAKE_CURRENT_BINARY_DIR}/Shaders"
		COMMENT "Copying Shaders of ${TARGET_NAME}"
	)

	add_custom_target(install_compiler_${TARGET_NAME}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${D3D12CompilerDLL}"
			"${CMAKE_CURRENT_BINARY_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${D3D12DXILDLL}"
			"${CMAKE_CURRENT_BINARY_DIR}"
		COMMENT "Installing compiler for ${TARGET_NAME}"
	)

	add_custom_target(install_d3d12sdk_${TARGET_NAME}
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
			"${D3D12SDKPath}"
			"${CMAKE_CURRENT_BINARY_DIR}/D3D12"
		COMMENT "Installing d3d12sdk for ${TARGET_NAME}"
	)

	add_dependencies(${TARGET_NAME} copy_shaders_${TARGET_NAME})
	add_dependencies(${TARGET_NAME} install_compiler_${TARGET_NAME})
	add_dependencies(${TARGET_NAME} install_d3d12sdk_${TARGET_NAME})
endfunction(create_target TARGET_NAME)