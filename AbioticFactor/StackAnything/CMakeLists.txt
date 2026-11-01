set(TARGET StackAnything)

add_library(${TARGET} SHARED
        dllmain.cpp
        StackAnything.cpp
        config.cpp
)

target_include_directories(${TARGET} PRIVATE .)
target_link_libraries(${TARGET} PUBLIC UE4SS)
target_compile_features(${TARGET} PRIVATE cxx_std_23)

add_executable(StackAnythingConfigTest
        config.cpp
        config_test.cpp
)
target_include_directories(StackAnythingConfigTest PRIVATE .)
target_compile_features(StackAnythingConfigTest PRIVATE cxx_std_23)

# Deploy dir = a UE4SS Mods folder (or a mod-manager mod that maps onto it); set per machine, e.g. in CMakeUserPresets.json
if(AF_DEPLOY_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E touch "${AF_DEPLOY_DIR}/${TARGET}/enabled.txt"
    )
endif()
