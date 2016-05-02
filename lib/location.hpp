//
// Created by Clarkok on 16/4/29.
//

#ifndef CYAN_LOCATION_HPP
#define CYAN_LOCATION_HPP

#include <string>

namespace cyan {

struct Location
{
    std::string filename;
    int line = 0;
    int column = 0;

    Location(std::string filename)
        : filename(filename)
    { }
};

}

namespace std {

std::string to_string(cyan::Location location)
{ return location.filename + ":" + std::to_string(location.line) + " " + std::to_string(location.column); }

}

#endif //CYAN_LOCATION_HPP
