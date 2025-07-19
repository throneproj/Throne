# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

set(prefix "${TEST_PREFIX}")
set(suffix "${TEST_SUFFIX}")
set(spec ${TEST_SPEC})
set(extra_args ${TEST_EXTRA_ARGS})
set(properties ${TEST_PROPERTIES})
set(add_labels ${TEST_ADD_LABELS})
set(junit_output_dir "${TEST_JUNIT_OUTPUT_DIR}")
set(script)
set(suite)
set(tests)

function(add_command NAME)
  set(_args "")
  foreach(_arg ${ARGN})
    if(_arg MATCHES "[^-./:a-zA-Z0-9_]")
      set(_args "${_args} [==[${_arg}]==]") # form a bracket_argument
    else()
      set(_args "${_args} ${_arg}")
    endif()
  endforeach()
  set(script "${script}${NAME}(${_args})\n" PARENT_SCOPE)
endfunction()

# Run test executable to get list of available tests
if(NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR
    "Specified test executable '${TEST_EXECUTABLE}' does not exist"
  )
endif()

if("${spec}" MATCHES .)
  set(spec "--test-case=${spec}")
endif()

execute_process(
  COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" ${spec} --list-test-cases
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
  WORKING_DIRECTORY "${TEST_WORKING_DIR}"
)
if(NOT ${result} EQUAL 0)
  message(FATAL_ERROR
    "Error running test executable '${TEST_EXECUTABLE}':\n"
    "  Result: ${result}\n"
    "  Output: ${output}\n"
  )
endif()

string(REPLACE "\n" ";" output "${output}")

# Parse output
foreach(line ${output})
  if("${line}" STREQUAL "===============================================================================" OR "${line}" MATCHES [==[^\[doctest\] ]==])
    continue()
  endif()
  set(test ${line})
  set(labels "")
  if(${add_labels})
    # get test suite that test belongs to
    execute_process(
      COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" --test-case=${test} --list-test-suites
      OUTPUT_VARIABLE labeloutput
      RESULT_VARIABLE labelresult
      WORKING_DIRECTORY "${TEST_WORKING_DIR}"
    )
    if(NOT ${labelresult} EQUAL 0)
      message(FATAL_ERROR
        "Error running test executable '${TEST_EXECUTABLE}':\n"
        "  Result: ${labelresult}\n"
        "  Output: ${labeloutput}\n"
      )
    endif()

    string(REPLACE "\n" ";" labeloutput "${labeloutput}")
    foreach(labelline ${labeloutput})
      if("${labelline}" STREQUAL "===============================================================================" OR "${labelline}" MATCHES [==[^\[doctest\] ]==])
        continue()
      endif()
      list(APPEND labels ${labelline})
    endforeach()
  endif()

  if(NOT "${junit_output_dir}" STREQUAL "")
    # turn testname into a valid filename by replacing all special characters with "-"
    string(REGEX REPLACE "[/\\:\"|<>]" "-" test_filename "${test}")
    set(TEST_JUNIT_OUTPUT_PARAM "--reporters=junit" "--out=${junit_output_dir}/${prefix}${test_filename}${suffix}.xml")
  else()
    unset(TEST_JUNIT_OUTPUT_PARAM)
  endif()
  # use escape commas to handle properly test cases with commas inside the name
  string(REPLACE "," "\\," test_name ${test})
  # ...and add to script
  add_command(add_test
    "${prefix}${test}${suffix}"
    ${TEST_EXECUTOR}
    "${TEST_EXECUTABLE}"
    "--test-case=${test_name}"
    "${TEST_JUNIT_OUTPUT_PARAM}"
    ${extra_args}
  )
  add_command(set_tests_properties
    "${prefix}${test}${suffix}"
    PROPERTIES
    WORKING_DIRECTORY "${TEST_WORKING_DIR}"
    ${properties}
    LABELS ${labels}
  )
  unset(labels)
  list(APPEND tests "${prefix}${test}${suffix}")
endforeach()

# Create a list of all discovered tests, which users may use to e.g. set
# properties on the tests
add_command(set ${TEST_LIST} ${tests})

# Write CTest script
file(WRITE "${CTEST_FILE}" "${script}")
