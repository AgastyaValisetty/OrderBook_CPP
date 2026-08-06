#pragma once

#include <list>
#include <exception>
#include <format>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"


class Order {
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_ (orderType)
        , orderId_ (orderId)
        , side_ (side)
        , price_ (price)
        , initialQuantity_ (quantity)
        , remainingQuantity_ (quantity)
    {}

    Order(OrderId orderId, Side side, Quantity quantity)
        : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
    {}

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    bool isFilled() const { return GetInitialQuantity - remainingQuantity_; }


};
