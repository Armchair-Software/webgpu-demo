#!/bin/bash

build_dir="build"
if [ "${CMAKE_BUILD_TYPE,,}" = "release" ]; then
  build_dir="build_rel"
fi

launch_html="$build_dir/client.html"

if [ ! -f "$launch_html" ]; then
  echo "Project not yet built, building..."
  ./build.sh
fi

port=6932

echo "Launching $launch_html with emrun..."
#emrun --port "$port" "$launch_html" --browser /home/slowriot/usr/firefox-nightly/firefox-bin --browser_args="--new-window"
emrun --port "$port" "$launch_html" --browser /usr/lib/chromium/chromium --browser_args='--new-window --enable-unsafe-webgpu --enable-features=Vulkan'
echo "Finished."
