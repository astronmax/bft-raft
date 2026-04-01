cmake_minimum_required(VERSION 3.21)
project(auth-service)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Crow::Crow
    cpr::cpr
    CLI11::CLI11
    cryptopp
)
