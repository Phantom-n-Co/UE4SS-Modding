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

add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "F:/Apps/Steam/steamapps/common/AbioticFactor/AbioticFactor/Binaries/Win64/ue4ss/Mods/${TARGET}/dlls/"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "F:/Apps/Steam/steamapps/common/AbioticFactor/AbioticFactor/Binaries/Win64/ue4ss/Mods/${TARGET}/dlls/"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/enabled.txt" "F:/Apps/Steam/steamapps/common/AbioticFactor/AbioticFactor/Binaries/Win64/ue4ss/Mods/${TARGET}/enabled.txt"
)
