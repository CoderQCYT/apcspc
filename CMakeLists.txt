cmake_minimum_required (VERSION 3.10)
project(apcspc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add source to this project's executable.
add_executable(apcspc
	"src/main.cpp"
	"src/block.cpp"
	"src/block.h"
	"src/expressions.cpp"
	"src/expressions.h"
	"src/util.cpp"
	"src/util.h"
	"src/types/CompilerContext.h"
	"src/types/Variable.cpp"
	"src/types/Variable.h"
	"src/types/Procedure.cpp"
	"src/types/Procedure.h"
	"src/types/ExecResult.cpp"
	"src/types/ExecResult.h"
    "resources.rc"
)


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT apcspc)

install(TARGETS apcspc DESTINATION .)

if (MSVC)
    add_link_options( PRIVATE
        $<$<CONFIG:Debug>:/W3 /Zi /RTC1>
        $<$<CONFIG:RelWithDebInfo>:/O2 /GL /Gy /Gw /Zo>
        $<$<CONFIG:Release>:/O2 /GL /Gy /Gw>
        $<$<CONFIG:MinSizeRel>:/O1 /GL /Gy /Gw /GS- /GR->
    )

    add_link_options(PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF /INCREMENTAL:NO /LTCG>
        # for debug disable aslr
        $<CONFIG:Debug>:/DYNAMICBASE:NO>
    )
else() # GCC / Clang / AppleClang
    add_compile_options(${target} PRIVATE
        $<$<CONFIG:Debug>:
            -O0 -g -fno-omit-frame-pointer
        >

        $<$<CONFIG:RelWithDebInfo>:
            -O3
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            -ffunction-sections
            -fdata-sections
        >

        $<$<CONFIG:Release>:
            -O3
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            -ffunction-sections
            -fdata-sections
        >

        $<$<CONFIG:MinSizeRel>:
            -Os
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            -ffunction-sections
            -fdata-sections
        >
    )

    add_link_options(PRIVATE
        # Linux / ELF
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<PLATFORM_ID:Linux>>:
            -Wl,--gc-sections
        >

        # macOS / Mach-O
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<PLATFORM_ID:Darwin>>:
            -Wl,-dead_strip
        >

        # Strip only for MinSizeRel
        $<$<AND:$<CONFIG:MinSizeRel>,$<PLATFORM_ID:Linux>>:
            -Wl,--strip-all
        >
    )
endif()