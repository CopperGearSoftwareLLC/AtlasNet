
apt update
apt install -y git tini gdbserver build-essential binutils automake libtool m4 autoconf gdb curl zip less unzip ccache coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev
git clone https://github.com/microsoft/vcpkg.git 
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install 

git clone https://github.com/premake/premake-core.git premake
cd premake
make -f Bootstrap.mak linux
cd ..
./premake/bin/release/premake5 gmake 

cp -a vcpkg_installed/x64-linux/lib/* /usr/local/lib/
ldconfig