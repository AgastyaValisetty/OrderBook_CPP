#include <iostream>
#include <memory>

#include "Orderbook.h"

void PrintTrades(const Trades& trades)
{
    if (trades.empty())
    {
        std::cout << "No trades executed.\n\n";
        return;
    }

    std::cout << "\nTrades Executed\n";
    std::cout << "=========================\n";

    for (const auto& trade : trades)
    {
        const auto& bid = trade.bidTrade();
        const auto& ask = trade.askTrade();

        std::cout
            << "BUY Order " << bid.orderId_
            << " <--> "
            << "SELL Order " << ask.orderId_
            << " | Price = " << bid.price_
            << " | Qty = " << bid.quantity_
            << '\n';
    }

    std::cout << '\n';
}

void PrintBook(const Orderbook& book)
{
    auto infos = book.GetOrderInfos();

    std::cout << "\n========== ORDER BOOK ==========\n";

    std::cout << "\nASKS\n";
    for (const auto& level : infos.GetAsks())
    {
        std::cout
            << level.price
            << "  Qty: "
            << level.quantity_
            << '\n';
    }

    std::cout << "\nBIDS\n";
    for (const auto& level : infos.GetBids())
    {
        std::cout
            << level.price
            << "  Qty: "
            << level.quantity_
            << '\n';
    }

    std::cout << "================================\n\n";
}

int main()
{
    Orderbook orderbook;

    OrderId id = 1;

    //------------------------
    // BUY 100 @100
    //------------------------

    PrintTrades(orderbook.AddOrder(
        std::make_shared<Order>(
            OrderType::GoodTillCancel,
            id++,
            Side::Buy,
            100,
            100
        )));

    PrintBook(orderbook);

    //------------------------
    // BUY 50 @99
    //------------------------

    PrintTrades(orderbook.AddOrder(
        std::make_shared<Order>(
            OrderType::GoodTillCancel,
            id++,
            Side::Buy,
            99,
            50
        )));

    PrintBook(orderbook);

    //------------------------
    // SELL 70 @101
    //------------------------

    PrintTrades(orderbook.AddOrder(
        std::make_shared<Order>(
            OrderType::GoodTillCancel,
            id++,
            Side::Sell,
            101,
            70
        )));

    PrintBook(orderbook);

    //------------------------
    // SELL 80 @100
    // should match immediately
    //------------------------

    PrintTrades(orderbook.AddOrder(
        std::make_shared<Order>(
            OrderType::GoodTillCancel,
            id++,
            Side::Sell,
            100,
            80
        )));

    PrintBook(orderbook);

    //------------------------
    // SELL 100 @100 (FillAndKill)
    // should match partially, remainder killed
    //------------------------

    PrintTrades(orderbook.AddOrder(
        std::make_shared<Order>(
            OrderType::FillAndKill,
            id++,
            Side::Sell,
            100,
            100
        )));

    PrintBook(orderbook);

    return 0;
}
