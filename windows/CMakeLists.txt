cmake_minimum_required(VERSION 3.20)
project(ZhiPin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: no VC++ redistributable needed on user machines.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core)

add_library(ZhiPin SHARED
    src/Globals.cpp
    src/dllmain.cpp
    src/Register.cpp
    src/TextService.cpp
    src/KeyHandler.cpp
    src/Composition.cpp
    src/DisplayAttribute.cpp
    src/CandidateWindow.cpp
    src/EngineBridge.cpp
    src/ZhiPin.def
    ${CORE_DIR}/src/pinyin.cpp
    ${CORE_DIR}/src/engine.cpp
)

target_include_directories(ZhiPin PRIVATE
    src
    ${CORE_DIR}/include
    ${CORE_DIR}/src
)

target_compile_definitions(ZhiPin PRIVATE UNICODE _UNICODE NOMINMAX)
if(MSVC)
    target_compile_options(ZhiPin PRIVATE /utf-8 /W3)
endif()

target_link_libraries(ZhiPin PRIVATE
    ole32 oleaut32 uuid user32 gdi32 advapi32 shell32 shlwapi
)

set_target_properties(ZhiPin PROPERTIES OUTPUT_NAME "ZhiPin" PREFIX "")
