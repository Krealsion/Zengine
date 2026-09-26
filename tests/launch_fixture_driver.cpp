// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE DEVELOPMENT LAUNCH WITH A CASE'S OWN FACTS, for the cases that run launches as separate
// processes (tests/test_workshop_files.cpp): the same `launch` through the same `real_world` doors
// as `zengine-workshop-develop` -- the same claim, the same in-use check -- with its facts taken
// from its arguments, so two started at once meet exactly as two Runs of the real launch would.
//     zengine-launch-fixture-driver --cmake <cmake> --script <script> --host <file name>
//                                   --runtime <dir> --project <dir>

#include "workshop/develop.hpp"

#include <string>
#include <vector>

int main(int argc, char** argv) {
    namespace develop = zengine::workshop::develop;
    develop::Facts facts;
    facts.build = "the launch fixture's build tree";
    facts.runtime = "unnamed-runtime";
    facts.project = "unnamed-project";
    facts.plan = "plan.json";
    facts.catalog = "catalog.json";
    std::vector<std::string> choices;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--cmake") {
            facts.cmake = value;
        } else if (key == "--script") {
            facts.script = value;
        } else if (key == "--host") {
            facts.host = value;
        } else {
            choices.push_back(key);
            choices.push_back(value);
        }
    }
    return develop::launch(facts, develop::choose(choices, facts), develop::real_world());
}
