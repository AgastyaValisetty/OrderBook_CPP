#pragma once

#include "TradeInfo.h"

class Trade {
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_ { bidTrade }
        , askTrade_ { askTrade }
    {}

    const TradeInfo& bidTrade() const { return bidTrade_; }
    const TradeInfo& askTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;