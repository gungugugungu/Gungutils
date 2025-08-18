#!/bin/bash

echo "starting compilation"

find . -type f -name "*.glsl" | while read -r shader_file; do
    echo "-----"
    # shader directory
    dir_path=$(dirname "$shader_file")
    # base name of the shader file
    base_name=$(basename "$shader_file")
    # output file name
    output_file="$dir_path/${base_name}.h"

    echo "$shader_file -> $output_file"
    ./sokol-shdc --input "$shader_file" --output "$output_file" --slang glsl430

echo "finished compilation"

done