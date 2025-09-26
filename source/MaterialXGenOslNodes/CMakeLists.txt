

file(GLOB GenNodes_SRC "${CMAKE_CURRENT_SOURCE_DIR}/LibsToOso.cpp")

set(MATERIALX_LIBRARIES
    MaterialXCore
    MaterialXFormat
    MaterialXGenShader
    MaterialXGenOsl
    MaterialXRenderOsl)

add_executable(MaterialXGenOslNodes_LibsToOso ${GenNodes_SRC})

target_link_libraries(
    MaterialXGenOslNodes_LibsToOso
    ${MATERIALX_LIBRARIES})

set_target_properties(
    MaterialXGenOslNodes_LibsToOso PROPERTIES
    INSTALL_RPATH "${MATERIALX_UP_ONE_RPATH}")

# TODO: We likely want to install that file elsewhere and not under `bin`...
install(TARGETS MaterialXGenOslNodes_LibsToOso
    EXPORT MaterialX
    RUNTIME DESTINATION ${MATERIALX_INSTALL_BIN_PATH})
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:MaterialXGenOslNodes_LibsToOso>
            DESTINATION ${MATERIALX_INSTALL_BIN_PATH} OPTIONAL)
endif()
