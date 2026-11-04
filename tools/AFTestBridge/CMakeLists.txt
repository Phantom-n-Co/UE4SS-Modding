set(TARGET AFTestBridge)

# Dev-only UE4SS mod: loopback TCP command bridge for automated in-game tests
# (client: tools/afbridge.py). Never released; lives outside AbioticFactor/ so
# tools/nexus_release.py does not treat it as a mod.
add_library(${TARGET} SHARED
        dllmain.cpp
        AFTestBridge.cpp
)

target_include_directories(${TARGET} PRIVATE .)
target_link_libraries(${TARGET} PUBLIC UE4SS ws2_32)
target_compile_features(${TARGET} PRIVATE cxx_std_23)

if(AF_DEPLOY_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E touch "${AF_DEPLOY_DIR}/${TARGET}/enabled.txt"
    )
endif()
