set(TARGET StackAnything)

add_library(${TARGET} SHARED
        dllmain.cpp
        StackAnything.cpp
        config.cpp
)

target_include_directories(${TARGET} PRIVATE .)
target_link_libraries(${TARGET} PUBLIC UE4SS)

add_executable(StackAnythingConfigTest
        config.cpp
        config_test.cpp
)
target_include_directories(StackAnythingConfigTest PRIVATE .)
target_compile_features(StackAnythingConfigTest PRIVATE cxx_std_17)

add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "F:/Apps/Steam/steamapps/common/AbioticFactor/AbioticFactor/Binaries/Win64/ue4ss/Mods/${TARGET}/dlls/"
)
