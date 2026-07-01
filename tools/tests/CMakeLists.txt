# Smoke test target, driven by CTest. Built only when NETWORKML_BUILD_TESTS=ON;
# consumers of the library never see this.
add_executable(networkml_smoke main.cpp)
target_link_libraries(networkml_smoke PRIVATE networkml)

add_test(NAME smoke COMMAND networkml_smoke)
# Keep the whole run under two minutes even if postman-echo is slow.
set_tests_properties(smoke PROPERTIES TIMEOUT 120)
