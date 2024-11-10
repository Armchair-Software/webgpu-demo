#!/bin/bash

infile="$1"
if [ -z "$infile" ]; then
  echo "Usage: $0 <filename> [namespace]" 2>&1
  exit 1
fi

if [ ! -f "$infile" ]; then
  echo "Resource compiler: File $infile not found." 2>&1
  exit 1
fi

outfile="$1.h"

# don't update if outfile is newer than infile
if [ ! "$infile" -nt "$outfile" ]; then
  echo "Resource compiler: $outfile up to date"
  exit
fi

# generate a fairly unique hash to act as a rawstring prefix/suffix
shorthash=$(md5sum "$infile" | cut -c 1-16)

if [ -z "$2" ]; then
  namespace=$(dirname "$infile" | sed 's/\//::/g')
else
  namespace="$2"
fi
resourcename=$(basename "$infile" | sed 's/\./_/g')

# truncate the destination file
> "$outfile"

# optional namespace
if [ ! -z "$namespace" ]; then
  echo "namespace $namespace {" >> "$outfile"
fi

echo -n "inline constexpr char const *${resourcename}{R\"${shorthash}(" >> "$outfile"
cat "$infile" >> "$outfile"
echo -n ")${shorthash}\"};" >> "$outfile"

if [ ! -z "$namespace" ]; then
  echo "}" >> "$outfile"
fi

echo "Resource compiler: $infile compiled to $outfile: $namespace::$resourcename"
