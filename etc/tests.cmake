enable_testing()

add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_"
                         COMMENT "Testing...")

add_test(NAME t_addblob WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/add_tester
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/addblob.wasm
)

add_test(NAME t_add_simple WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/add_tester
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/add-simple.wasm
)
