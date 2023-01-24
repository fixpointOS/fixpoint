enable_testing()

file(
  MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/.fix
)

add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_"
                         COMMENT "Testing..."
                         DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/file.txt)

add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/file.txt
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tester/serialization_test_deep | cut -c28- | head -c -1 > ${CMAKE_CURRENT_BINARY_DIR}/file.txt
)

add_test(NAME t_deep_open WORKING_DIRECTORY
  COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/src/tester/wasi_tester ${CMAKE_CURRENT_BINARY_DIR}/flatware-prefix/src/flatware-build/examples/open/open-deep-fixpoint.wasm -h `cat ${CMAKE_CURRENT_BINARY_DIR}/file.txt`"
)

add_test(NAME t_open WORKING_DIRECTORY
  COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/src/tester/wasi_tester ${CMAKE_CURRENT_BINARY_DIR}/flatware-prefix/src/flatware-build/examples/open/open-fixpoint.wasm -h `cat ${CMAKE_CURRENT_BINARY_DIR}/file.txt`"
)

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

add_test(NAME t_fib WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/fib-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/fib_tester
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/fib.wasm
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/add-simple.wasm
)

add_test(NAME t_return3 WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/flatware/testing/return-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/wasi_tester
  ${CMAKE_CURRENT_BINARY_DIR}/flatware-prefix/src/flatware-build/examples/return3/return3-fixpoint.wasm
)

add_test(NAME t_hello_world WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/flatware/testing/hello-world-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/wasi_tester
  ${CMAKE_CURRENT_BINARY_DIR}/flatware-prefix/src/flatware-build/examples/helloworld/helloworld-fixpoint.wasm
)

add_test(NAME t_add_args WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/flatware/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/wasi_tester
  ${CMAKE_CURRENT_BINARY_DIR}/flatware-prefix/src/flatware-build/examples/add/add-fixpoint.wasm
)
# XXX we don't catch the trap yet
#add_test(NAME t_trap WORKING_DIRECTORY
#  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
#  tree:2 string:ignored
#  "file:${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/trap.wasm"
#)
