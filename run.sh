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

port=6934

echo "Launching $launch_html with emrun..."
#emrun --port "$port" "$launch_html" --browser /home/slowriot/usr/firefox-nightly/firefox-bin --browser_args="--new-window"
if [ -n "$WAYLAND_DISPLAY" ]; then
  echo "Special handling for chromium on Wayland..."
  # special handling for wayland and chromium: needs XWayland to run Vulkan
  if ! pgrep -x Xwayland >/dev/null; then
    echo "WARNING: Chromium under Wayland requires XWayland for Vulkan support, without it this may not work."
  fi
  xwayland_display=$(ps -o args= -C Xwayland | cut -d ' ' -f 2)
  echo "XWayland display is $xwayland_display"
  DISPLAY="$xwayland_display" emrun --port "$port" --serve_root / "$launch_html" --browser /usr/lib/chromium/chromium --browser_args="--new-window --enable-unsafe-webgpu --ozone-platform=x11 --enable-features=Vulkan"
else
  emrun --port "$port" --serve_root / "$launch_html" --browser /usr/lib/chromium/chromium --browser_args='--new-window --enable-unsafe-webgpu --enable-features=Vulkan'
fi
echo "Finished."
