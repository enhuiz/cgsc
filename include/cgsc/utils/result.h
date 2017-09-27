#pragma once
#ifndef CGSC_SOLVER_RESULT_H
#define CGSC_SOLVER_RESULT_H

#include <list>
#include <string>
#include <iostream>
#include <map>

#include "json.hpp"

#include "cgsc/model/scene.h"
#include "cgsc/model/aoi.h"
#include "cgsc/model/polygon.h"

namespace cgsc
{
namespace utils
{
class Result
{
public:
  Result(const model::AOI &aoi,
         const std::vector<std::shared_ptr<const model::Scene>> &possibleScenes,
         const std::vector<std::shared_ptr<const model::Scene>> &resultScenes);

  nlohmann::json toJSON() const;

public:
  friend std::ostream &operator<<(std::ostream &os, const Result &result);

private:
  nlohmann::json jobj;
};
}
}

#endif