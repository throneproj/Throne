if(SPB_PROTO_USE_ADDRESS_SANITIZER)
    message("-- SPB: Enable address sanitizer")
endif()

if(SPB_PROTO_USE_UB_SANITIZER)
    message("-- SPB: Enable undefined behavior sanitizer")
endif()

if(SPB_PROTO_USE_COVERAGE)
    find_program(GCOV gcov)
    if (NOT GCOV)
        message(FATAL "SPB: program gcov not found")
    endif()

    find_program(LCOV lcov)
    if (NOT LCOV)
        message(FATAL "SPB: program lcov not found")
    endif()

    find_program(GENHTML genhtml)
    if (NOT GENHTML)
        message(FATAL "SPB: program genhtml not found")
    endif()

    message("-- SPB: Enable code coverage")
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT IPO_ENABLED)
endif()

function(spb_enable_warnings TARGET)
    if(MSVC)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # clang-cl doesn't support /Zc:preprocessor
            target_compile_options(${TARGET} PRIVATE /W4 /WX /wd4996 /wd4244 /EHs -Wno-missing-field-initializers)
        else()
            # https://developercommunity.visualstudio.com/t/c2017-illegal-escape-sequence-when-using-in-a-raw/919371
            target_compile_options(${TARGET} PRIVATE /W4 /WX /wd4996 /wd4244 /Zc:preprocessor)
        endif()        
        target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
    endif()
endfunction(spb_enable_warnings)

function(spb_disable_warnings TARGET)
    if(NOT MSVC)
        target_compile_options(${TARGET} PRIVATE -Wno-deprecated-declarations -Wno-deprecated "-Wno-#warnings")
    endif()
endfunction(spb_disable_warnings)

function(spb_set_compile_options TARGET)
    spb_enable_warnings(${TARGET})

    set_target_properties(${TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION IPO_ENABLED)

    if(SPB_PROTO_USE_ADDRESS_SANITIZER)
        target_compile_options(${TARGET} PRIVATE -fsanitize=address)
        target_link_options(${TARGET} PRIVATE -fsanitize=address)
    endif()

    if(SPB_PROTO_USE_UB_SANITIZER)
        target_compile_options(${TARGET} PRIVATE -fsanitize=undefined)
        target_link_options(${TARGET} PRIVATE -fsanitize=undefined)
    endif()

    if(SPB_PROTO_USE_COVERAGE)
        target_compile_options(${TARGET} PRIVATE -fprofile-arcs -ftest-coverage)
        target_link_options(${TARGET} PRIVATE -fprofile-arcs -ftest-coverage)
    endif()

endfunction(spb_set_compile_options)
