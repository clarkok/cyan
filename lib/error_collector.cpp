//
// Created by Clarkok on 16/5/3.
//

#include <iostream>
#include "rlutil/rlutil.h"

#include "error_collector.hpp"

using namespace cyan;

void EmptyErrorCollector::error(const std::exception &) { }
void EmptyErrorCollector::warn(const std::exception &) { }
void EmptyErrorCollector::info(const std::exception &) { }

void
ChainErrorCollector::error(const std::exception &e)
{
    for(auto &collector : collectors) {
        collector->error(e);
    }
}

void
ChainErrorCollector::warn(const std::exception &e)
{
    for(auto &collector : collectors) {
        collector->warn(e);
    }
}

void
ChainErrorCollector::info(const std::exception &e)
{
    for(auto &collector : collectors) {
        collector->info(e);
    }
}

void
FilterErrorCollector::error(const std::exception &e)
{ error_collector->error(e); }

void
FilterErrorCollector::warn(const std::exception &e)
{ warn_collector->warn(e); }

void
FilterErrorCollector::info(const std::exception &e)
{ info_collector->info(e); }

void
CounterErrorCollector::error(const std::exception &)
{ ++error_counter; }

void
CounterErrorCollector::warn(const std::exception &)
{ ++warn_counter; }

void
CounterErrorCollector::info(const std::exception &)
{ ++info_counter; }

void
LimitErrorCollector::error(const std::exception &e)
{
    CounterErrorCollector::error(e);
    if (getErrorCount() > error_limit) {
        throw TooManyMessagesException("error", error_limit);
    }
}

void
LimitErrorCollector::warn(const std::exception &e)
{
    dynamic_cast<CounterErrorCollector*>(this)->warn(e);
    if (getWarnCount() > warn_limit) {
        throw TooManyMessagesException("warning", warn_limit);
    }
}

void
LimitErrorCollector::info(const std::exception &e)
{
    dynamic_cast<CounterErrorCollector*>(this)->info(e);
    if (getInfoCount() > info_limit) {
        throw TooManyMessagesException("info", info_limit);
    }
}

void
ScreenOutputErrorCollector::error(const std::exception &e)
{
    rlutil::saveDefaultColor();
    rlutil::setColor(rlutil::RED);
    std::cout << "ERR!";
    rlutil::resetColor();

    rlutil::setColor(rlutil::WHITE);
    std::cout << ": " << e.what() << std::endl;
    rlutil::resetColor();
}

void
ScreenOutputErrorCollector::warn(const std::exception &e)
{
    rlutil::saveDefaultColor();
    rlutil::setColor(rlutil::YELLOW);
    std::cout << "WARN";
    rlutil::resetColor();

    rlutil::setColor(rlutil::WHITE);
    std::cout << ": " << e.what() << std::endl;
    rlutil::resetColor();
}

void
ScreenOutputErrorCollector::info(const std::exception &e)
{
    rlutil::saveDefaultColor();
    rlutil::setColor(rlutil::GREEN);
    std::cout << "INFO";
    rlutil::resetColor();

    rlutil::setColor(rlutil::WHITE);
    std::cout << ": " << e.what() << std::endl;
    rlutil::resetColor();
}
