#
#  Standard DICe use examples
#

SET(EXAMPLES
   mechanism
   obstruction
   full_field)

# very loose tolerances for the examples (these are not regression tests)

FOREACH ( test ${EXAMPLES})
    # copy all the necessary files over to the build dir
    FILE(COPY ${test} DESTINATION . )
    # create a test for each folder (or subfolder)
    ADD_TEST ( NAME "RUN_EXAMPLE_${test}"
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${test}
         COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../../bin/dice -i input.xml -v)
    # remove the results folder (to clear previous runs)
    FILE(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/${test}/results)
    # Separate test for each output file (could be more elegant way to do this...)
    FILE ( GLOB gold_files "${CMAKE_CURRENT_BINARY_DIR}/${test}/gold/*.txt")
    FOREACH ( gold_file ${gold_files})
        # get the name of the file without the directory
        GET_FILENAME_COMPONENT ( trim_name ${gold_file} NAME)
        ADD_TEST ( NAME "DIFF_EXAMPLE_${test}_${trim_name}" 
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../../bin/DICe_Diff ${gold_file} ${CMAKE_CURRENT_BINARY_DIR}/${test}/results/${trim_name} -t 10.0 -v)
        set_tests_properties("DIFF_EXAMPLE_${test}_${trim_name}" PROPERTIES PASS_REGULAR_EXPRESSION "TEST PASSED")
        set_property(TEST "DIFF_EXAMPLE_${test}_${trim_name}" APPEND PROPERTY DEPENDS "RUN_EXAMPLE_${test}")
    ENDFOREACH ( )
ENDFOREACH ( )

#
#  Examples using DICe in an custom app
#

# Copy over all the test files (input and results)
FILE(COPY custom_app DESTINATION . )
# create a test
ADD_TEST ( NAME RUN_EXAMPLE_custom_app
   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/custom_app
   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/custom_app/build/main)
set_tests_properties(RUN_EXAMPLE_custom_app PROPERTIES PASS_REGULAR_EXPRESSION "TEST PASSED")

# use an external project to build the custom_app example

include(ExternalProject)
MESSAGE(STATUS "Building cutsom_app_example from ${CMAKE_CURRENT_SOURCE_DIR}")
ExternalProject_Add(custom_app_example
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/custom_app
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/tests/examples/custom_app/build --clean-first
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/custom_app
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/custom_app/build
  TMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/utils_tmp
  CMAKE_CACHE_ARGS -DDICE_LIB_DIR:STRING=${DICE_OUTPUT_PREFIX}/lib -DDICE_HEADER_DIR:STRING=${DICE_HEADER_DIRS} -DDICE_TRILINOS_DIR:STRING=${DICE_TRILINOS_DIR} -DTIFF_DIR:STRING=${TIFF_DIR} -DJPEG_DIR:STRING=${JPEG_DIR} -DPNG_DIR:STRING=${PNG_DIR} -DBOOST_ROOT:STRING=${BOOST_ROOT} -DBOOST_LIBRARYDIR:STRING=${BOOST_LIBRARYDIR} -DBOOST_INCLUDEDIR:STRING=${BOOST_INCLUDEDIR} -DDICE_ENABLE_MANYCORE:BOOL=${DICE_ENABLE_MANYCORE} -DDICE_MPI:BOOL=${DICE_MPI} -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE})

# force rebuild of custom_app example
ExternalProject_Add_Step(custom_app_example forcebuild
  COMMAND ${CMAKE_COMMAND} -E echo "Force rebuild of custom_app_example"
  DEPENDEES configure
  DEPENDERS build
  ALWAYS 1)
