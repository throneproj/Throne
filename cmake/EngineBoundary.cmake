# Engine/UI boundary lint, rules E1-E5. Runs without a configured tree:
#   cmake -DROOT=<repo> -P cmake/EngineBoundary.cmake
cmake_minimum_required(VERSION 3.20)

if (NOT DEFINED ROOT OR ROOT STREQUAL "")
    get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif ()
file(TO_CMAKE_PATH "${ROOT}" ROOT)
get_filename_component(ROOT "${ROOT}" ABSOLUTE)

# E5 ratchet: files outside the UI that still include include/ui/. Entries only ever leave this list.
set(UI_INCLUDE_ALLOWLIST
        src/sys/macos/TrayIcon.mm                       # permanent
)

set(LOWER_LAYER_REGEX "^(include|src)/(api|configs|database|global|scanner|stats|sys)/")
set(LOWER_LAYER_EXEMPT_REGEX "^src/api/remote/Router[^/]*\\.cpp$")
set(LOWER_ENGINE_HEADERS EngineTypes.h Lifecycle.h EngineEvents.h EngineTr.h)

set(SOURCE_EXT_REGEX "\\.(h|hh|hpp|hxx|inl|ipp|c|cc|cpp|cxx|m|mm)$")
set(IDENT_END "([^A-Za-z0-9_]|$)")

set(E1_INCLUDE_PATTERNS
        "^include/ui/"
        "(^|/)ui_[^/]*$"
        "(^|/)mainwindow[^/]*\\.h$"
        "^Qt(Widgets|Gui)"
        "^(QApplication|QWidget|QDialog|QMessageBox|QMainWindow|QMenu|QAction|QSystemTrayIcon|QInputDialog|QFileDialog)$"
        "^(QGuiApplication|QPixmap|QIcon|QClipboard|QDesktopServices|QScreen|QCursor|QFont|QFontDatabase|QStyleHints)$"
)

set(E2_TOKEN_PATTERNS
        "GetMainWindow[ \t]*\\("
        "mainwindow${IDENT_END}"
        "MainWindow[ \t]*::"
        "Ui[ \t]*::"
        "MW_(dialog_message|handle_deeplink|import_files)${IDENT_END}"
        "MessageBox(Warning|Info|Check|Scrollable)[ \t]*\\("
        "(Show|Post)PassiveWarning[ \t]*\\("
        "GetMessageBoxParent[ \t]*\\("
        "(ActivateWindow|HideWindow|isDarkMode)[ \t]*\\("
        "qApp${IDENT_END}"
        "Q(Gui)?Application[ \t]*::"
)

# Stand-ins for the characters CMake's list splitting reacts to, so a file can be walked as a list of lines.
string(ASCII 1 SEMI)
string(ASCII 2 LBR)
string(ASCII 3 RBR)
string(ASCII 4 BSL)

set_property(GLOBAL PROPERTY ENGINE_BOUNDARY_VIOLATIONS 0)

function(eb_report rule file line text)
    string(REPLACE "${SEMI}" ";" text "${text}")
    string(REPLACE "${LBR}" "[" text "${text}")
    string(REPLACE "${RBR}" "]" text "${text}")
    string(REPLACE "${BSL}" "\\" text "${text}")
    string(STRIP "${text}" text)
    message("engine-boundary: ${rule} ${file}:${line}: ${text}")
    get_property(count GLOBAL PROPERTY ENGINE_BOUNDARY_VIOLATIONS)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY ENGINE_BOUNDARY_VIOLATIONS ${count})
endfunction()

# Splits the line in `raw` into `code` (comments removed) and `bare` (comments and literals removed).
# `in_block` and `raw_end` carry block comments and raw strings across lines.
macro(eb_lex_line)
    set(code "")
    set(bare "")
    set(rest "${raw}")
    while (NOT rest STREQUAL "")
        if (in_block)
            string(FIND "${rest}" "*/" pos)
            if (pos EQUAL -1)
                set(rest "")
            else ()
                math(EXPR pos "${pos} + 2")
                string(SUBSTRING "${rest}" ${pos} -1 rest)
                set(in_block FALSE)
                string(APPEND code " ")
                string(APPEND bare " ")
            endif ()
        elseif (NOT raw_end STREQUAL "")
            string(FIND "${rest}" "${raw_end}" pos)
            if (pos EQUAL -1)
                string(APPEND code "${rest}")
                set(rest "")
            else ()
                string(LENGTH "${raw_end}" len)
                math(EXPR pos "${pos} + ${len}")
                string(SUBSTRING "${rest}" 0 ${pos} literal)
                string(APPEND code "${literal}")
                string(SUBSTRING "${rest}" ${pos} -1 rest)
                set(raw_end "")
            endif ()
        else ()
            string(REGEX MATCH "^[^\"'/]+" head "${rest}")
            string(LENGTH "${head}" len)
            string(APPEND code "${head}")
            string(APPEND bare "${head}")
            string(SUBSTRING "${rest}" ${len} -1 rest)
            string(SUBSTRING "${rest}" 0 2 lead)
            set(len 1)
            if (rest STREQUAL "")
                set(len 0)
            elseif (lead STREQUAL "//")
                set(len 0)
                set(rest "")
            elseif (lead STREQUAL "/*")
                set(len 2)
                set(in_block TRUE)
            elseif (lead MATCHES "^/")
                string(APPEND code "/")
                string(APPEND bare "/")
            elseif (lead MATCHES "^\"")
                if (code MATCHES "(^|[^A-Za-z0-9_])(u8|u|U|L)?R$" AND rest MATCHES "^\"([^ ()\"${BSL}]*)\\(")
                    set(raw_end ")${CMAKE_MATCH_1}\"")
                    set(literal "${CMAKE_MATCH_0}")
                    string(APPEND bare "\"\"")
                elseif (rest MATCHES "^\"([^\"${BSL}]|${BSL}.)*\"")
                    set(literal "${CMAKE_MATCH_0}")
                    string(APPEND bare "\"\"")
                else ()
                    set(literal "${rest}")
                endif ()
                string(APPEND code "${literal}")
                string(LENGTH "${literal}" len)
            elseif (NOT code MATCHES "[0-9A-Fa-f]$" AND rest MATCHES "^'([^'${BSL}]|${BSL}.)*'")
                string(APPEND code "${CMAKE_MATCH_0}")
                string(APPEND bare "''")
                string(LENGTH "${CMAKE_MATCH_0}" len)
            else ()
                # A digit separator or a stray quote.
                string(APPEND code "'")
                string(APPEND bare "'")
            endif ()
            if (len GREATER 0)
                string(SUBSTRING "${rest}" ${len} -1 rest)
            endif ()
        endif ()
    endwhile ()
endmacro()

# ENGINE runs E1/E2; LOWER runs E3/E5 and reports in <out_has_ui> whether the file includes include/ui/.
function(eb_scan_file rel mode out_has_ui)
    set(lower FALSE)
    if (mode STREQUAL "LOWER" AND rel MATCHES "${LOWER_LAYER_REGEX}" AND NOT rel MATCHES "${LOWER_LAYER_EXEMPT_REGEX}")
        set(lower TRUE)
    endif ()

    file(READ "${ROOT}/${rel}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\\" "${BSL}" content "${content}")
    string(REPLACE ";" "${SEMI}" content "${content}")
    string(REPLACE "[" "${LBR}" content "${content}")
    string(REPLACE "]" "${RBR}" content "${content}")
    # The "|" prefix keeps empty lines as list elements, so element N is line N.
    string(REPLACE "\n" ";|" content "|${content}")

    set(in_block FALSE)
    set(raw_end "")
    set(has_ui FALSE)
    set(n 0)
    foreach (raw IN LISTS content)
        math(EXPR n "${n} + 1")
        string(SUBSTRING "${raw}" 1 -1 raw)
        eb_lex_line()
        if (code MATCHES "^[ \t]*#[ \t]*(include|import)[ \t]*[<\"]([^>\"]*)[>\"]")
            set(target "${CMAKE_MATCH_2}")
            if (mode STREQUAL "ENGINE")
                foreach (pattern IN LISTS E1_INCLUDE_PATTERNS)
                    if (target MATCHES "${pattern}")
                        eb_report(E1 "${rel}" ${n} "${raw}")
                        break()
                    endif ()
                endforeach ()
            else ()
                if (target MATCHES "^include/ui/")
                    set(has_ui TRUE)
                    if (NOT rel IN_LIST UI_INCLUDE_ALLOWLIST)
                        eb_report(E5 "${rel}" ${n} "${raw}")
                    endif ()
                endif ()
                if (lower AND target MATCHES "^include/engine/(.*)$")
                    set(header "${CMAKE_MATCH_1}")
                    if (NOT header IN_LIST LOWER_ENGINE_HEADERS)
                        eb_report(E3 "${rel}" ${n} "${raw}")
                    endif ()
                endif ()
            endif ()
        elseif (mode STREQUAL "ENGINE")
            foreach (pattern IN LISTS E2_TOKEN_PATTERNS)
                if (bare MATCHES "(^|[^A-Za-z0-9_])${pattern}")
                    eb_report(E2 "${rel}" ${n} "${raw}")
                    break()
                endif ()
            endforeach ()
        endif ()
    endforeach ()
    set(${out_has_ui} ${has_ui} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE engine_files LIST_DIRECTORIES false RELATIVE "${ROOT}"
        "${ROOT}/include/engine/*" "${ROOT}/src/engine/*")
list(FILTER engine_files EXCLUDE REGEX "(^|/)\\.")
list(SORT engine_files)

file(GLOB_RECURSE other_files LIST_DIRECTORIES false RELATIVE "${ROOT}" "${ROOT}/include/*" "${ROOT}/src/*")
list(FILTER other_files INCLUDE REGEX "${SOURCE_EXT_REGEX}")
list(FILTER other_files EXCLUDE REGEX "^(include|src)/(ui|engine)/")
list(FILTER other_files EXCLUDE REGEX "^src/main\\.cpp$")
list(SORT other_files)

# E4: every engine file is listed in ENGINE_SOURCES.
file(READ "${ROOT}/CMakeLists.txt" cmakelists)
string(REGEX REPLACE "#[^\n]*" "" cmakelists "${cmakelists}")
set(listed "")
if (cmakelists MATCHES "set[ \t\r\n]*\\([ \t\r\n]*ENGINE_SOURCES([^)]*)\\)")
    string(REGEX REPLACE "[ \t\r\n]+" ";" listed "${CMAKE_MATCH_1}")
    list(REMOVE_ITEM listed "")
else ()
    eb_report(E4 CMakeLists.txt 1 "no set(ENGINE_SOURCES ...) block")
endif ()
foreach (rel IN LISTS engine_files)
    if (NOT rel IN_LIST listed)
        eb_report(E4 "${rel}" 1 "not listed in ENGINE_SOURCES in CMakeLists.txt")
    endif ()
endforeach ()

# E1/E2
foreach (rel IN LISTS engine_files)
    if (rel MATCHES "${SOURCE_EXT_REGEX}")
        eb_scan_file("${rel}" ENGINE has_ui)
    endif ()
endforeach ()

# E3/E5: a native prefilter picks the files worth lexing.
set(files_with_ui "")
foreach (rel IN LISTS other_files)
    file(STRINGS "${ROOT}/${rel}" hits REGEX "#[ \t]*(include|import)[ \t]*[<\"]include/(ui|engine)/")
    if (NOT hits STREQUAL "")
        eb_scan_file("${rel}" LOWER has_ui)
        if (has_ui)
            list(APPEND files_with_ui "${rel}")
        endif ()
    endif ()
endforeach ()
foreach (rel IN LISTS UI_INCLUDE_ALLOWLIST)
    if (NOT rel IN_LIST files_with_ui)
        eb_report(E5 "${rel}" 1 "no longer includes include/ui/: remove it from the allowlist in cmake/EngineBoundary.cmake")
    endif ()
endforeach ()

get_property(violations GLOBAL PROPERTY ENGINE_BOUNDARY_VIOLATIONS)
if (violations GREATER 0)
    message(FATAL_ERROR "engine-boundary: ${violations} violation(s)")
endif ()
