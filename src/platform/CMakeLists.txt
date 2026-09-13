add_library(voelund_platform STATIC AtomicFileStore.cpp AtomicFileStoreNative.cpp)
target_include_directories(voelund_platform PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(voelund_platform PUBLIC voelund_project_options)
add_library(Voelund::Platform ALIAS voelund_platform)
