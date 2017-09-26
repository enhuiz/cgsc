#include <iostream>
#include <string>
#include <vector>

#include "cgsc/model/polygon.h"
#include "cgsc/model/grid.h"
#include "cgsc/model/aoi.h"
#include "cgsc/model/scene.h"
#include "cgsc/solver/greedy.h"
#include "cgsc/utils/data.h"
#include "cgsc/utils/result.h"
#include "cgsc/utils/timestamp.h"

using namespace cgsc::model;
using namespace cgsc::utils;
using namespace cgsc::solver;

using namespace std;

int main()
{
    shared_ptr<Data> data = make_shared<Data>("../../data/input/scenes_small.csv", "../../data/input/aois.csv");

    shared_ptr<Greedy> greedy = make_shared<Greedy>(data);

    auto results = greedy->calculateResults();
    for (const auto &result : results)
    {
        cout << result << endl;
    }
    
    cout << Timestamp::GetJSON() << endl;

    return 0;
}
