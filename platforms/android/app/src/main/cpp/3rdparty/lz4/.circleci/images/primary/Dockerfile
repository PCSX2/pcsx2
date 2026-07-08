FROM circleci/buildpack-deps:bionic

RUN sudo apt-get -y -qq update
RUN sudo apt-get -y install software-properties-common
RUN sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN sudo apt-get -y install cmake
RUN sudo apt-get -y install qemu-system-ppc qemu-user-static qemu-system-arm
RUN sudo apt-get -y install libc6-dev-armel-cross  libc6-dev-arm64-cross libc6-dev-i386
RUN sudo apt-get -y install clang clang-tools
RUN sudo apt-get -y install gcc-5 gcc-5-multilib gcc-6
RUN sudo apt-get -y install valgrind
RUN sudo apt-get -y install gcc-multilib-powerpc-linux-gnu gcc-powerpc-linux-gnu gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu
