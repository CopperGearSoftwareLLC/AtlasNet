#!/bin/bash

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null
then
    echo "Doxygen is not installed. Please install Doxygen and try again."
    exit 1
fi

# Check if the configuration file exists
if [ ! -f "Doxyfile" ]; then
    echo "Doxyfile not found! Please make sure you have a Doxyfile in the current directory."
    exit 1
fi

# Run Doxygen to generate the docs
doxygen Doxyfile