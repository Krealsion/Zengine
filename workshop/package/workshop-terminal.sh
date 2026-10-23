#!/bin/sh
# Workshop in this terminal. Its project is ~/Zengine/terminal, made at the first start and kept
# across packages, so a plan saved in the window's project does not start here. Each start names
# this package's zengine-app in the project, where an exported app's run.sh finds it.
here=$(cd "$(dirname "$0")" && pwd -P) || exit 1
if [ -z "$HOME" ]; then
    echo "Workshop keeps your project in \$HOME/Zengine/terminal, and HOME is not set." >&2
    exit 1
fi
project=$HOME/Zengine/terminal
mkdir -p "$project" && cd "$project" || {
    echo "Workshop could not make or enter its project folder, $project." >&2
    exit 1
}
printf '%s\n' "$here/bin/zengine-app" > "$project/zengine-app.path"
exec "$here/bin/zengine-workshop" "$@"
