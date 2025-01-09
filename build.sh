#!/bin/bash

compiled_resources=(
  render/shaders/*.wgsl
)

# redirect all stdout to stderr
exec 1>&2

build_dir="build"
if [ "${CMAKE_BUILD_TYPE,,}" = "release" ]; then
  build_dir="build_rel"
fi

target="$1"
if [ -z "$target" ]; then target="client"; fi

if [ "$target" = "clean" ]; then
  echo "Cleaning build directory \"$build_dir\"..."
  rm -r "$build_dir"
  echo "Cleaning ${#compiled_resources[@]} compiled resources..."
  for file in "${compiled_resources[@]}"; do
    rm "${file}.h"
  done
  target="client"
fi

procs=$(nproc)
./generate_git_version.sh

if [ ! -d "$build_dir" ]; then
  echo "Creating build directory \"$build_dir\"..."
  mkdir "$build_dir"
fi

# compile resources
compiled_resources_total=0
compiled_resources_up_to_date=0
for file in "${compiled_resources[@]}"; do
  ((++compiled_resources_total))
  result=$(./compile_resource_to_raw_string.sh "$file")
  if grep -Fq "up to date" <<< "$result"; then
    ((++compiled_resources_up_to_date))
  fi
done
compiled_resources_updated=$((compiled_resources_total - compiled_resources_up_to_date))
echo "Compiled resources: $compiled_resources_updated updated, $compiled_resources_up_to_date up to date ($compiled_resources_total total)"

if [ "$compiled_resources_updated" != 0 ]; then
  # validate shaders
  if ! [ -z "$(which naga)" ]; then
    echo "Validating shaders with naga-cli"
    naga --bulk-validate render/shaders/*.wgsl || exit 1
  fi
fi

ccache="$(which ccache)"
if [ -n "$ccache" ]; then
  echo "CCache enabled"
  export EM_COMPILER_WRAPPER="$ccache"
fi

if [ ! -f "Makefile" ] || [ "../CMakeLists.txt" -nt "Makefile" ]; then
  echo "Running CMAKE to generate Makefile..."
  emcmake cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -B "$build_dir" || exit 1
fi

echo "Running make..."
#emmake cmake --build "$build_dir" -j"$procs" VERBOSE=1 -t "$target" || exit 1
emmake cmake --build "$build_dir" -j"$procs" -t "$target" || exit 1

echo "Assembling resources..."
rsync -ar --progress "resources/"* "$build_dir/"
echo "Done."
