#!/bin/bash
# Generate a standardised header file from the git version info

outfile="git_version.h"

if [ ! -f "$outfile" ]; then
  touch "$outfile"
fi

branch="$(git rev-parse --abbrev-ref HEAD)"
#branch="$(git branch | sed -n '/\* /s///p')"
#sha_full="$(git rev-parse HEAD)"
sha_full="$(git log --pretty=format:'%H' -n 1)"
sha="$(git log --pretty=format:'%h' -n 1)"
date_full="$(git log -1 --format="%cd")"
date_short="$(git log -1 --pretty='format:%ai')"
date_seconds="$(git log -1 --pretty='format:%at')"
if [ -z "$date_seconds" ]; then
  date_seconds=0;
fi

if grep -q "#define RC_GIT_REVISION_STRING \"$sha\"" "$outfile"; then
  echo "Version $sha ($date_full) on $branch is already up to date."
  exit
fi

cat << EOFEOFEOF > "$outfile"
#pragma once

namespace AutoVersion {

  #define RC_GIT_BRANCH $branch
  #define RC_GIT_BRANCH_STRING "$branch"
  static char const GIT_BRANCH[] = "$branch";

  #define RC_GIT_REVISION $sha
  #define RC_GIT_REVISION_FULL $sha_full
  #define RC_GIT_REVISION_STRING "$sha"
  #define RC_GIT_REVISION_FULL_STRING "$sha_full"
  static char const GIT_REVISION[] = "$sha";
  static char const GIT_REVISION_FULL[] = "$sha_full";

  #define RC_GIT_DATE $date_full
  #define RC_GIT_DATE_SHORT $date_short
  #define RC_GIT_DATE_SECONDS $date_seconds
  #define RC_GIT_DATE_STRING "$date_full"
  #define RC_GIT_DATE_SHORT_STRING "$date_short"
  #define RC_GIT_DATE_SECONDS_STRING "$date_seconds"
  static char const GIT_DATE[] = "$date_full";
  static char const GIT_DATE_SHORT[] = "$date_short";
  static unsigned int const GIT_DATE_SECONDS = $date_seconds;
  static char const GIT_DATE_SECONDS_STRING[] = "$date_seconds";
}
EOFEOFEOF

echo "New version is $sha ($date_full) on $branch."
