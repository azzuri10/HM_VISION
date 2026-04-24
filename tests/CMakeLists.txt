set(HMVISION_TEST_SOURCES
    smoke_test.cpp
)

add_executable(hmvision_smoke_test ${HMVISION_TEST_SOURCES})
target_compile_features(hmvision_smoke_test PRIVATE cxx_std_17)

add_test(NAME hmvision_smoke_test COMMAND hmvision_smoke_test)
