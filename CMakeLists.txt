cmake_minimum_required(VERSION 4.2)
project(OrderBookCPP)

set(CMAKE_CXX_STANDARD 26)

add_executable(OrderBookCPP test.cpp
        main.cpp
        Orderbook.cpp
        Orderbook.h
        Usings.h
        Order.h
)
