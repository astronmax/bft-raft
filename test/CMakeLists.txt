cmake_minimum_required(VERSION 3.21)
project(test)

set(CMAKE_CXX_STANDARD 23)

add_executable(simple_client simple_client.cpp)
target_link_libraries(simple_client PRIVATE
    Crow::Crow
    cpr::cpr
    CLI11::CLI11
    cryptopp
)
