#include "geometry.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <iostream>

#include "global.h"

using namespace std;

Vector2 Vector2::operator+(const Vector2 &other) const
{
    return Vector2{x + other.x, y + other.y};
}

Vector2 Vector2::operator-(const Vector2 &other) const
{
    return Vector2{x - other.x, y - other.y};
}

bool Vector2::operator==(const Vector2 &other) const
{
    return x == other.x && y == other.y;
}

bool Vector2::almost_equal(const Vector2 &other, int ulp) const
{
    return ::almost_equal(x, other.x, ulp) && ::almost_equal(y, other.y, ulp);
}

double cross(const Vector2 &a, const Vector2 &b)
{
    return a.x * b.y - a.y * b.x;
}

string to_string(const Vector2 &v)
{
    return "[" + to_string(v.x, 20) + ", " + to_string(v.y, 20) + "]";
}

string to_string(const Polygon &poly)
{
    if (poly.size() == 0)
        return "[]";
    string ret = "[";
    for (const auto &p : poly)
    {
        ret += to_string(p) + ", ";
    }
    // ", " -> "]"
    ret.pop_back();
    ret[ret.size() - 1] = ']';
    return ret;
}

ostream &operator<<(ostream &os, const Vector2 &v)
{
    os << to_string(v);
    return os;
}

ostream &operator<<(ostream &os, const Polygon &poly)
{
    os << to_string(poly);
    return os;
}

Polygon parse_polygon(const string &s)
{
    Polygon ret;
    char br, comma;
    double x, y;
    istringstream iss(s);
    iss >> br; // read [
    while (!iss.eof())
    {
        iss >> br >> x >> comma >> y >> br; // read [1, 2]
        ret.push_back(Point{x, y});
        iss >> comma; // read , or ]
        if (comma == ']')
            break;
    }
    return ret;
}

bool onside(const Point &p, const Point &a, const Point &b)
{
    auto u = b - a;
    auto v = p - a;
    return almost_equal(u.x * v.y, u.y * v.x, 1);
}

bool inside(const Point &p, const Point &a, const Point &b) // inside a line means on the left side of on it
{
    return cross(b - a, p - a) > 0 && !onside(p, a, b);
}

bool outside(const Point &p, const Point &a, const Point &b)
{
    return cross(b - a, p - a) < 0 && !onside(p, a, b);
}

bool inside(const Point &p, const Polygon &poly)
{
    auto s = poly.back();
    for (const auto &e : poly)
    {
        if (!inside(p, s, e))
        {
            return false;
        }
        s = e;
    }
    return true;
}

bool outside(const Point &p, const Polygon &poly)
{
    auto s = poly.back();
    for (const auto &e : poly)
    {
        if (outside(p, s, e))
        {
            return true;
        }
        s = e;
    }
    return false;
}

bool disjoints(const Polygon &a, const Polygon &b)
{
    return !intersects(a, b);
}

bool intersects(const Polygon &a, const Polygon &b)
{
    return intersection(a, b).size() > 0;
}

bool intersects(const Point &a, const Point &b, const Point &c, const Point &d)
{
    return inside(a, c, d) == outside(b, c, d) && inside(c, a, b) == outside(d, a, b);
}

bool simple(const Polygon &poly)
{
    using Iter = decltype(poly.begin());

    auto post = [&poly](const Iter &it) {
        return next(it) == poly.end() ? poly.begin() : next(it);
    };

    for (auto i = poly.begin(); next(next(i)) != poly.end(); ++i)
    {
        auto ip = post(i);
        for (auto j = post(ip); j != poly.end(); ++j)
        {
            auto jp = post(j);
            if (jp != i && intersects(*i, *ip, *j, *jp))
            {
                return false;
            }
        }
    }
    return true;
}

bool convex(const Polygon &poly)
{
    auto prev = poly.begin();
    auto cur = next(prev);
    auto post = next(cur);
    while (post != poly.end())
    {
        if (!inside(*post, *prev, *cur))
        {
            return false;
        }
        prev = cur;
        cur = post;
        post++;
    }
    return true;
}

double area(const Polygon &poly)
{
    double ret = 0;
    auto s = poly.back();
    for (const auto &e : poly)
    {
        ret += cross(s, e);
        s = e;
    }
    return 0.5 * ret;
}

// list<Polygon> simplify(const Polygon &poly)
// {
//     auto simplified = Polygon();
//     {
//         auto s = poly.back();
//         for (const auto &e : poly)
//         {
//             if (!s.almost_equal(e, 1e9))
//             {
//                 simplified.push_back(e);
//             }
//             s = e;
//         }
//     }
//     if (simplified.size() != 2)
//     {
//         cout << poly << endl;
//         cout << simplified.size() << endl;
//         throw runtime_error("Error: triangulating nonsimple polygon!\n" + to_string(poly));
//     }
//     return triangulate(simplified);
//     // throw runtime_error("Error: triangulating nonsimple polygon!\n" + to_string(poly));
// }

list<Triangle> triangulate(const Polygon &poly)
{
    list<Triangle> ret;

    enum Type
    {
        CONVEX,
        EARTIP,
        REFLEX,
    };

    struct Vertex
    {
        Point p;
        Type t;
    };

    list<Vertex> vs;

    using Iter = decltype(vs.begin());

    auto get_prev = [&vs](const Iter &it) {
        return it == vs.begin() ? prev(vs.end()) : prev(it);
    };

    auto get_post = [&vs](const Iter &it) {
        return next(it) == vs.end() ? vs.begin() : next(it);
    };

    auto is_reflex = [](const Iter &cur, const Iter &pre, const Iter &pst) {
        return cross(cur->p - pre->p, pst->p - pre->p) < 0;
    };

    auto no_rv_inside = [&vs](const Iter &cur, const Iter &pre, const Iter &pst) {
        return all_of(vs.begin(), vs.end(), [pre, cur, pst](const Vertex &v) {
            return v.t != REFLEX || !inside(v.p, Polygon{pre->p, cur->p, pst->p});
        });
    };

    auto update_reflex = [get_prev, get_post, is_reflex](const Iter &cur) {
        auto pre = get_prev(cur);
        auto pst = get_post(cur);
        if (is_reflex(cur, pre, pst))
        {
            cur->t = REFLEX;
        }
        else if (cur->t == REFLEX)
        {
            cur->t = CONVEX;
        }
    };

    auto update_eartip = [get_prev, get_post, no_rv_inside](const Iter &cur) {
        if (cur->t != REFLEX)
        {
            auto pre = get_prev(cur);
            auto pst = get_post(cur);
            if (no_rv_inside(cur, pre, pst))
            {
                cur->t = EARTIP;
            }
            else // disqualify the eartip
            {
                cur->t = CONVEX;
            }
        }
    };

    auto find_eartip = [&vs]() {
        for (auto it = vs.begin(); it != vs.end(); ++it)
        {
            if (it->t == EARTIP)
            {
                return it;
            }
        }
        return vs.end();
    };

    { // initialize vs
        for (auto it = vs.begin(); it != vs.end(); ++it)
        {
            update_reflex(it);
        }
        for (auto it = vs.begin(); it != vs.end(); ++it)
        {
            update_eartip(it);
        }
    }

    while (vs.size() > 2)
    {
        auto eartip = find_eartip();
        if (eartip == vs.end())
        {
            list<Point> ps;
            transform(vs.begin(), vs.end(), back_inserter(ps), [](const Vertex &v) {
                return v.p;
            });
            ret.push_back(ps);
            throw runtime_error("Error: eartip is not enough!\n" + to_string(poly) + '\n' + to_string(ps));
        }
        auto pre = get_prev(eartip);
        auto pst = get_post(eartip);
        ret.push_back(Triangle{pre->p, eartip->p, pst->p});
        vs.erase(eartip);
        update_reflex(pre);
        update_reflex(pst);
        update_eartip(pre);
        update_eartip(pst);
    }

    return ret;
}

Point line_line_intersection(const Point &a, const Point &b, const Point &c, const Point &d)
{
    double denominator = cross(a, c) + cross(b, d) + cross(c, b) + cross(d, a);
    if (denominator == 0)
    {
        throw runtime_error("Error: denominator = 0 in line_line_intersection\n" + to_string(Polygon{a, b}) + "," + to_string(Polygon{c, d}));
    }
    double numerator_part1 = cross(a, b);
    double numerator_part2 = cross(c, d);
    Point e = {(numerator_part1 * (c.x - d.x) - numerator_part2 * (a.x - b.x)) / denominator,
               (numerator_part1 * (c.y - d.y) - numerator_part2 * (a.y - b.y)) / denominator};
    return e;
}

Polygon intersection(const Polygon &clippee, const Polygon &clipper)
{
    if (!convex(clipper))
    {
        throw runtime_error("Error: clipper is non-convex in intersection!\n" + to_string(clipper));
    }

    auto output_list = clippee;
    auto s2 = clipper.back();
    for (const auto &e2 : clipper)
    {
        auto input_list = output_list;
        output_list.clear();
        auto s1 = input_list.back();
        for (const auto &e1 : input_list)
        {
            if (inside(e1, s2, e2))
            {
                if (outside(s1, s2, e2))
                {
                    output_list.push_back(line_line_intersection(s1, e1, s2, e2));
                }
                output_list.push_back(e1);
            }
            else if (outside(e1, s2, e2) && inside(s1, s2, e2))
            {
                output_list.push_back(line_line_intersection(s1, e1, s2, e2));
            }
            s1 = e1;
        }
        s2 = e2;
    }
    return output_list;
}

list<Polygon> difference(const Polygon &clippee, const Polygon &clipper)
{
    if (!convex(clipper))
    {
        throw runtime_error("Error: clipper is non-convex in difference!\n" + to_string(clipper));
    }

    list<Polygon> ret;
    auto output_list = clippee;
    auto s2 = clipper.back();
    for (const auto &e2 : clipper)
    {
        auto offcut = list<Point>();
        auto input_list = output_list;
        output_list.clear();
        auto s1 = input_list.back();
        for (const auto &e1 : input_list)
        {
            if (inside(e1, s2, e2))
            {
                if (!inside(s1, s2, e2))
                {
                    auto p = line_line_intersection(s1, e1, s2, e2);
                    output_list.push_back(p);
                    offcut.push_back(p);
                }
                output_list.push_back(e1);
            }
            else
            {
                if (inside(s1, s2, e2))
                {
                    auto p = line_line_intersection(s1, e1, s2, e2);
                    output_list.push_back(p);
                    offcut.push_back(p);
                }
                offcut.push_back(e1);
            }
            s1 = e1;
        }
        s2 = e2;
        if (offcut.size() > 0)
        {
            if (convex(offcut))
            {
                ret.push_back(offcut);
            }
            else
            {
                // throw runtime_error("Error: offcut is not convex after difference!\n" + to_string(offcut));
            }
        }
    }
    if (output_list.size() == 0) // no intersection
    {
        ret.clear();
        ret.push_back(clippee);
    }
    return ret;
}