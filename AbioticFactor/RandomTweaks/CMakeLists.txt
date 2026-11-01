set(TARGET RandomTweaks)

add_library(${TARGET} SHARED
        dllmain.cpp
        RandomTweaks.cpp
        config.cpp
)

target_include_directories(${TARGET} PRIVATE .)
target_link_libraries(${TARGET} PUBLIC UE4SS)

add_executable(RandomTweaksConfigTest
        config.cpp
        config_test.cpp
)
target_include_directories(RandomTweaksConfigTest PRIVATE .)
target_compile_features(RandomTweaksConfigTest PRIVATE cxx_std_17)

# Deploy dir = a UE4SS Mods folder (or a mod-manager mod that maps onto it); set per machine, e.g. in CMakeUserPresets.json
if(AF_DEPLOY_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "${AF_DEPLOY_DIR}/${TARGET}/dlls/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/enabled.txt" "${AF_DEPLOY_DIR}/${TARGET}/enabled.txt"
    )
endif()
