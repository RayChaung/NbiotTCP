cmake_minimum_required(VERSION 2.8)
project(NbiotUDP)

set(CMAKE_CXX_STANDARD 14)


add_executable(UDP_Server UDP_Server.cpp)
target_link_libraries(UDP_Server boost_thread boost_system -lpthread)

add_executable(UDP_Client UDP_Client.cpp)
target_link_libraries(UDP_Client boost_thread boost_system -lpthread)