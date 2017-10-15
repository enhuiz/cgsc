#include "solution.h"

#include <algorithm>
#include <list>
#include <iostream>

using namespace std;

vector<Scene *> select_possible_scenes(AOI *aoi, const vector<Scene *> &scenes)
{
    vector<Scene *> ret;
    copy_if(scenes.begin(), scenes.end(), back_inserter(ret), [aoi](Scene *scene) {
        return intersects(aoi->poly, scene->poly);
    });
    return ret;
}

BoostPoint point_to_boost_point(const Point &p)
{
    return BoostPoint(p.x, p.y);
}

BoostPolygon polygon_to_boost_polygon(const Polygon &poly)
{
    vector<BoostPoint> bps;
    bps.reserve(poly.size());
    for (const auto &p : poly)
    {
        bps.push_back(point_to_boost_point(p));
    }
    BoostPolygon bpoly;
    boost::geometry::append(bpoly, bps);
    return bpoly;
}

string boost_polygon_to_string(const BoostPolygon &bpoly)
{
    string ret = "[";
    for (auto p : bpoly.outer())
    {
        ret += "[" + to_string(p.x()) + ", " + to_string(p.y()) + "], ";
    }
    // ", " to "]"
    ret.pop_back();
    ret[ret.size() - 1] = ']';
    return ret;
}

Polygon boost_polygon_to_polygon(const BoostPolygon &bpoly)
{
    Polygon ret;
    for (const auto &bp : bpoly.outer())
    {
        ret.push_back(Point{bp.x(), bp.y()});
    }
    return ret;
}

namespace discrete
{

void discretize_aoi(const Discretizer &discretizer, AOI *aoi)
{
    aoi->cell_set = discretizer.discretize(aoi->poly, true);
}

// O(nmlogm)
void discretize_scenes(const Discretizer &discretizer, AOI *aoi, const vector<Scene *> &scenes)
{
    auto aoi_bbox = discretizer.axis_aligned_bounding_box(aoi->poly, static_cast<double (*)(double)>(floor), static_cast<double (*)(double)>(ceil));
    auto aoi_bbox_bpoly = polygon_to_boost_polygon(aoi_bbox);

    for (auto scene : scenes) // n
    {
        auto scene_bpoly = polygon_to_boost_polygon(scene->poly);
        vector<BoostPolygon> output_bpolys;
        boost::geometry::intersection(aoi_bbox_bpoly, scene_bpoly, output_bpolys);
        CellSet cs;
        for (const auto &output_bpoly : output_bpolys)
        {
            // find cid in the intersecitons
            auto itersection_cs = discretizer.discretize(boost_polygon_to_polygon(output_bpoly), false);
            for (const auto &cid : itersection_cs)
            {
                cs.insert(cid);
            }
        }
        // intersection
        scene->cell_set.clear();
        set_intersection(cs.begin(),
                         cs.end(),
                         aoi->cell_set.begin(),
                         aoi->cell_set.end(),
                         inserter(scene->cell_set, scene->cell_set.begin()));
    }
}

// O(n^2mlogm)
vector<Scene *> select_approx_optimal_scenes(AOI *aoi, const vector<Scene *> &scenes)
{
    list<Scene *> possible_scenes(scenes.begin(), scenes.end());
    vector<Scene *> result_scenes;
    int covered = 0;
    int num_aoi_cells = aoi->cell_set.size();

    auto remove_scenes_with_empty_cell_set = [](list<Scene *> &scenes) {
        scenes.erase(remove_if(scenes.begin(),
                               scenes.end(),
                               [](const Scene *scene) {
                                   return scene->cell_set.size() == 0;
                               }),
                     scenes.end());
    };

    remove_scenes_with_empty_cell_set(possible_scenes);
    while (covered < num_aoi_cells && possible_scenes.size() > 0) // n
    {
        auto it = min_element(possible_scenes.begin(), possible_scenes.end(), [](const Scene *a, const Scene *b) { // n
            return a->price / a->cell_set.size() < b->price / b->cell_set.size();
        });
        // remove the selected
        auto scene = *it;
        possible_scenes.erase(it);
        // remove cells from the left possible scenes
        for (auto &possible_scene : possible_scenes) // n
        {
            for (const auto &cell : scene->cell_set) // m
            {
                possible_scene->cell_set.erase(cell); // logm
            }
        }
        covered += scene->cell_set.size();
        result_scenes.push_back(scene);
        remove_scenes_with_empty_cell_set(possible_scenes);
    }
    return result_scenes;
}
}

namespace continuous
{

vector<Scene *> select_approx_optimal_scenes(AOI *aoi, const vector<Scene *> &scenes)
{

    auto boost_polygons_to_string = [](const vector<BoostPolygon> &bpolys) {
        string ret = "[";
        for (const auto &bpoly : bpolys)
        {
            ret += boost_polygon_to_string(bpoly) + ", ";
        }
        // ", " to "]"
        ret.pop_back();
        ret[ret.size() - 1] = ']';
        return ret;
    };

    auto scene_different_boost_polygon = [](Scene *scene, const BoostPolygon &bpoly) {
        vector<BoostPolygon> result_bpolys;
        for (const auto &scene_bpoly : scene->bpolys)
        {
            vector<BoostPolygon> output_bpolys;
            try
            {
                boost::geometry::difference(scene_bpoly, bpoly, output_bpolys);
            }
            catch (...)
            {
            }
            for (const auto &output_bpoly : output_bpolys)
            {
                if (boost::geometry::area(output_bpoly) > 1e-4)
                {
                    result_bpolys.push_back(output_bpoly);
                }
            }
        }
        scene->bpolys = result_bpolys;
    };

    auto scene_different_boost_polygons = [scene_different_boost_polygon](Scene *scene, vector<BoostPolygon> &bpolys) {
        for (const auto &bpoly : bpolys)
        {
            scene_different_boost_polygon(scene, bpoly);
        }
    };

    auto scene_intersection_boost_polygon = [](Scene *scene, const BoostPolygon &bpoly) {
        vector<BoostPolygon> result_bpolys;
        for (const auto &scene_bpoly : scene->bpolys)
        {
            vector<BoostPolygon> output_bpolys;
            boost::geometry::intersection(scene_bpoly, bpoly, output_bpolys);
            for (const auto &output_bpoly : output_bpolys)
            {
                if (boost::geometry::area(output_bpoly) > 1e-4)
                {
                    result_bpolys.push_back(output_bpoly);
                }
            }
        }
        scene->bpolys = result_bpolys;
    };

    auto scene_intersection_boost_polygons = [scene_intersection_boost_polygon](Scene *scene, vector<BoostPolygon> &bpolys) {
        for (const auto &bpoly : bpolys)
        {
            scene_intersection_boost_polygon(scene, bpoly);
        }
    };

    { // assemble bpoly
        aoi->bpolys = vector<BoostPolygon>{polygon_to_boost_polygon(aoi->poly)};
        for (auto scene : scenes)
        {
            scene->bpolys = vector<BoostPolygon>{polygon_to_boost_polygon(scene->poly)};
            scene_intersection_boost_polygons(scene, aoi->bpolys);
        }
    }

    list<Scene *> possible_scenes(scenes.begin(), scenes.end());
    vector<Scene *> result_scenes;

    double covered_area = 0;

    auto scene_area = [](const Scene *scene) {
        double ret = 0;
        for (const auto &bpoly : scene->bpolys)
        {
            ret += boost::geometry::area(bpoly);
        }
        return ret;
    };

    double aoi_area = 0;
    for (const auto &bpoly : aoi->bpolys)
        aoi_area += boost::geometry::area(bpoly);

    auto remove_scenes_with_empty_bpolys = [scene_area](list<Scene *> &scenes) {
        cout << "before: " << scenes.size() << endl;
        scenes.erase(remove_if(scenes.begin(),
                               scenes.end(),
                               [scene_area](const Scene *scene) {
                                   return scene_area(scene) < 1e-3;
                               }),
                     scenes.end());
        cout << "after: " << scenes.size() << endl
             << endl;
    };

    // return vector<Scene *>(possible_scenes.begin(), possible_scenes.end());
    remove_scenes_with_empty_bpolys(possible_scenes);
    while (covered_area < aoi_area && possible_scenes.size() > 0) // n
    {
        auto it = min_element(possible_scenes.begin(), possible_scenes.end(), [scene_area](const Scene *a, const Scene *b) { // n
            return a->price / scene_area(a) < b->price / scene_area(b);
        });
        // remove the selected
        auto scene = *it;
        possible_scenes.erase(it);
        // add coverd area
        covered_area += scene_area(scene);
        // clip the left possible scenes
        for (auto possible_scene : possible_scenes) // n
        {
            scene_different_boost_polygons(possible_scene, scene->bpolys);
        }
        remove_scenes_with_empty_bpolys(possible_scenes);
        for (auto possible_scene : possible_scenes) // n
        {
            cout << boost_polygons_to_string(possible_scene->bpolys) << endl;
        }
        result_scenes.push_back(scene);
    }

    return result_scenes;
}
}