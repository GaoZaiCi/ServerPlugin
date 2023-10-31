cmake_minimum_required(VERSION 3.21)
project(TemplatePlugin)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_BUILD_TYPE Release)

file(GLOB_RECURSE SRC_FILES_DIR
        ${PROJECT_SOURCE_DIR}/SDK/Header/*.cpp
        ${PROJECT_SOURCE_DIR}/SDK/Header/*.hpp
        ${PROJECT_SOURCE_DIR}/SDK/Header/*.h
        ${PROJECT_SOURCE_DIR}/Template/*.cpp
        ${PROJECT_SOURCE_DIR}/Template/*.hpp
        ${PROJECT_SOURCE_DIR}/Template/*.h
        )

include_directories(SDK/include)
include_directories(SDK/include/llapi)
include_directories(Template)
include_directories(
        capstone-next/include
        capstone-next/arch/X86
        capstone-next/cs_simple_types.h
        capstone-next/cs_priv.h
        capstone-next/LEB128.h
        capstone-next/Mapping.h
        capstone-next/MathExtras.h
        capstone-next/MCDisassembler.h
        capstone-next/MCFixedLenDisassembler.h
        capstone-next/MCInst.h
        capstone-next/MCInstrDesc.h
        capstone-next/MCRegisterInfo.h
        capstone-next/SStream.h
        capstone-next/utils.h
)

link_directories(.)

add_definitions(-D"NDEBUG" -D"TEMPLATE_EXPORTS" -D"WIN32_LEAN_AND_MEAN" -D"_CRT_SECURE_NO_WARNINGS" -D"_WINDOWS"
        -D"_USRDLL" -D"_AMD64_" -D"NOMINMAX" -D"_WINDLL" -D"_UNICODE" -D"UNICODE")

# 用于启用mc函数
add_definitions(-DENABLE_VIRTUAL_FAKESYMBOL_RANDOM)

add_compile_options(
        /permissive- /GS /GL /W3 /Gy /Zc:wchar_t /Zi /Gm- /sdl /Zc:inline /fp:precise /errorReport:prompt /WX-
        /Zc:forScope /Gd /Oi /MD /std:c++17 /FC /EHsc /nologo /diagnostics:column /utf-8
)

add_link_options(
        /MANIFEST /LTCG:incremental /NXCOMPAT /DEBUG /DLL /MACHINE:X64 /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:WINDOWS
        /MANIFESTUAC:NO /OPT:ICF /ERRORREPORT:PROMPT /NOLOGO /DELAYLOAD:"bedrock_server.dll" /TLBID:1
)

add_library(capstone
        capstone-next/arch/X86/X86Disassembler.c
        capstone-next/arch/X86/X86DisassemblerDecoder.c
        capstone-next/arch/X86/X86IntelInstPrinter.c
        capstone-next/arch/X86/X86InstPrinterCommon.c
        capstone-next/arch/X86/X86Mapping.c
        capstone-next/arch/X86/X86Module.c
)

add_library(TemplatePlugin SHARED ${SRC_FILES_DIR})

target_link_libraries(TemplatePlugin capstone)

add_custom_command(TARGET TemplatePlugin PRE_BUILD
        COMMAND cmd /c ${PROJECT_SOURCE_DIR}/prepareLib.cmd ${PROJECT_SOURCE_DIR}
        COMMENT "Preparing Library"
        )

add_custom_command(TARGET TemplatePlugin POST_BUILD
        COMMAND cmd /c ${PROJECT_SOURCE_DIR}/copy.cmd
        COMMENT "Copy DLL"
        )
