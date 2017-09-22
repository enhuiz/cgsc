#ifndef CGSC_MODEL_UTILS_HPP
#define CGSC_MODEL_UTILS_HPP

#include <string>
#include <list>
#include <sstream>
#include <cctype>
#include <cassert>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

namespace cgsc
{
namespace model
{

using Point = boost::geometry::model::d2::point_xy<double>;

template <class T>
std::list<T> parseListOf(const std::string &s)
{
    return std::list<T>();
}

template <>
std::list<double> parseListOf(const std::string &s)
{
    std::list<double> ret;
    // required string format:
    // [[0.105, 0.105], [0.2, 0.2], ..., [0.2, 0.2]]

    const auto nexti = [&](const std::string &s, int &i) {
        while (i < s.size() && !std::isdigit(s[i]) && s[i] != '.' && s[i] != '-')
            ++i;
    };

    const auto nextdouble = [](const std::string &s, int &i) {
        unsigned long long digits = 0;
        double decimal = 1;

        bool dot = false;
        int sign = 1;

        if (s[i] == '-')
        {
            sign = -1;
            ++i;
        }

        while (i < s.size())
        {
            if (s[i] == '.')
            {
                dot = true;
            }
            else if (std::isdigit(s[i]))
            {
                digits *= 10;
                digits += s[i] - '0';
                if (dot)
                {
                    decimal *= 10;
                }
            }
            else
            {
                ++i; // to avoid an extra check
                break;
            }
            ++i;
        }
        return digits / decimal * sign;
    };

    int i = 0;
    nexti(s, i);

    while (i < s.size())
    {
        ret.push_back(nextdouble(s, i));
        nexti(s, i);
    }

    return ret;
}

template <>
std::list<Point> parseListOf(const std::string &s)
{
    auto nums = parseListOf<double>(s);

    std::list<Point> points;
    for (auto i = nums.begin(); i != nums.end(); ++i)
    {
        double x = *i;
        double y = *(++i);
        points.emplace_back(x, y);
    }

    return points;
}
}
}

#endif