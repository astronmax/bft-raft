cmake_minimum_required(VERSION 3.21)
project(bft-raft)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/bft_state_machine.cpp
    src/request_handler.cpp
    src/in_memory_log_store.cxx)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/NuRaft)
target_link_libraries(${PROJECT_NAME} PRIVATE NuRaft::static_lib)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Crow)
target_link_libraries(${PROJECT_NAME} PRIVATE Crow::Crow)
