#!/bin/bash

sudo apt update
sudo apt install bear libx11-dev libxrandr-dev libwayland-dev libprotobuf-dev protobuf-compiler libssl-dev libcurl4-openssl-dev libboost-all-dev -y


 extensions=(
    ms-vscode.cpptools-extension-pack   # C++ Extension Pack
    llvm-vs-code-extensions.vscode-clangd # Clangd
    ms-azuretools.vscode-docker         # Docker
    docker.docker                 # Docker DX
    yzhang.markdown-all-in-one          # Markdown All in One
    pkief.material-icon-theme           # Material Icon Theme
    cschlosser.doxdocgen
    mhutchie.git-graph
)

# Install each extension
for extension in "${extensions[@]}"; do
    echo "Installing $extension..."
    code --install-extension "$extension" --force
done

echo "✅ All requested extensions installed!"