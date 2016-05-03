//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_ERROR_COLLECTOR_HPP
#define CYAN_ERROR_COLLECTOR_HPP

#include <exception>
#include <iostream>
#include <vector>
#include <memory>
#include <limits>

namespace cyan {

class ErrorCollector
{
public:
    ErrorCollector() = default;
    virtual ~ErrorCollector() = default;

    virtual void error(const std::exception &) = 0;
    virtual void warn(const std::exception &) = 0;
    virtual void info(const std::exception &) = 0;
};

class EmptyErrorCollector : public ErrorCollector
{
public:
    EmptyErrorCollector() = default;

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

class ChainErrorCollector : public ErrorCollector
{
    std::vector<std::unique_ptr<ErrorCollector> > collectors;

    ChainErrorCollector() { }

public:
    struct Builder
    {
        std::unique_ptr<ChainErrorCollector> product;

        Builder()
            : product(new ChainErrorCollector())
        { }

        inline ChainErrorCollector *
        release()
        { return product.release(); }

        inline Builder &
        addCollector(ErrorCollector *collector)
        {
            product->collectors.emplace_back(collector);
            return *this;
        }
    };

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

class FilterErrorCollector : public ErrorCollector
{
    std::unique_ptr<ErrorCollector> error_collector;
    std::unique_ptr<ErrorCollector> warn_collector;
    std::unique_ptr<ErrorCollector> info_collector;

public:
    FilterErrorCollector(ErrorCollector *error, ErrorCollector *warn, ErrorCollector *info)
        : error_collector(error), warn_collector(warn), info_collector(info)
    { }

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

class CounterErrorCollector : public ErrorCollector
{
    size_t error_counter;
    size_t warn_counter;
    size_t info_counter;

public:
    CounterErrorCollector()
        : error_counter(0),
          warn_counter(0),
          info_counter(0)
    { }

    inline size_t
    getErrorCount() const
    { return error_counter; }

    inline size_t
    getWarnCount() const
    { return warn_counter; }

    inline size_t
    getInfoCount() const
    { return info_counter; }

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

class LimitErrorCollector : public CounterErrorCollector
{
    size_t error_limit;
    size_t warn_limit;
    size_t info_limit;

public:
    struct TooManyMessagesException : std::exception
    {
        std::string _what;

        TooManyMessagesException(std::string type, size_t limit)
            : _what("too many " + type + " reaching limit " + std::to_string(limit))
        { }

        virtual const char *
        what() const noexcept
        { return _what.c_str(); }
    };

    LimitErrorCollector(
        size_t error_limit = std::numeric_limits<size_t>::max(),
        size_t warn_limit = std::numeric_limits<size_t>::max(),
        size_t info_limit = std::numeric_limits<size_t>::max()
    ) : error_limit(error_limit),
        warn_limit(warn_limit),
        info_limit(info_limit)
    { }

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

class ScreenOutputErrorCollector : public ErrorCollector
{
public:
    ScreenOutputErrorCollector() = default;

    virtual void error(const std::exception &);
    virtual void warn(const std::exception &);
    virtual void info(const std::exception &);
};

}

#endif //CYAN_ERROR_COLLECTOR_HPP
