# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "/mnt/ssd/haoran/types_proj/baseline/grappa/applications/dataframe/cmake")

find_package(TBB)
find_package(Threads REQUIRED)

# set(SOURCES dataframe_query.cpp)
set(SOURCES dataframe_global.cpp)

add_grappa_application(dataframe ${SOURCES})

# add_executable(dataframe_query dataframe_query.cpp)

target_link_libraries(dataframe ${TBB_LIBRARIES} Threads::Threads)

