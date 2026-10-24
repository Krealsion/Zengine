#!/bin/sh
# Workshop in a window, or in this terminal where no window can open. Its project is ~/Zengine,
# made at the first start and kept across packages; a plan that project holds (workshop-plan.json)
# starts instead of the shipped one. Each start names this package's zengine-app in the project,
# where an exported app's run.sh finds it.
here=$(cd "$(dirname "$0")" && pwd -P) || exit 1
if [ -z "$HOME" ]; then
    echo "Workshop keeps your project in \$HOME/Zengine, and HOME is not set." >&2
    exit 1
fi
project=$HOME/Zengine
mkdir -p "$project" && cd "$project" || {
    echo "Workshop could not make or enter its project folder, $project." >&2
    exit 1
}
printf '%s\n' "$here/bin/zengine-app" > "$project/zengine-app.path"
exec "$here/bin/zengine-workshop" "$@"
