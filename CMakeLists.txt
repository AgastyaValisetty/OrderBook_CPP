cmake_minimum_required(VERSION 4.2)
project(OrderBookCPP)

set(CMAKE_CXX_STANDARD 26)

add_executable(OrderBookCPP
        main.cpp
        Orderbook.cpp
        Orderbook.h
        Usings.h
        Order.h
        OrderType.h
        Side.h
        Constants.h
        OrderModify.h
        OrderbookLevelInfos.h
        LevelInfo.h
        Trade.h
        TradeInfo.h
)
