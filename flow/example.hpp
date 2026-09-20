// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_EXAMPLE_HPP
#define ZENGINE_FLOW_EXAMPLE_HPP

#include "flow/author.hpp"

namespace zengine::flow {

inline constexpr std::string_view thermostat_commands = R"flow(state heating Bool false
state on_below Int 18
state off_above Int 22
accept Reading degrees:Int
accept Lower lower:Int
accept Upper upper:Int
publish Status heating:Bool on_below:Int off_above:Int
on Reading heating
node compare.less_int $degrees $on_below
node compare.less_int $off_above $degrees
node logic.select_bool %1 false $heating
node logic.select_bool %0 true %2
result 3
emit Status heating=$heating on_below=$on_below off_above=$off_above
end
on Lower on_below
node compare.less_int $lower $off_above
node logic.select_int %0 $lower $off_above
result 1
emit Status heating=$heating on_below=$on_below off_above=$off_above
end
on Upper off_above
node math.max $upper $on_below
result 0
emit Status heating=$heating on_below=$on_below off_above=$off_above
end
)flow";

inline Project thermostat(const op::Catalog& catalog) {
    Draft draft(catalog, "thermostat");
    std::istringstream commands{std::string(thermostat_commands)};
    std::string line;
    while (std::getline(commands, line)) draft.command(words(line));
    return draft.finish();
}

} // namespace zengine::flow
#endif
