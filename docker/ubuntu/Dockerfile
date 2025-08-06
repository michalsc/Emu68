# syntax=docker/dockerfile:1
FROM ubuntu:24.10
RUN apt update
RUN apt upgrade -y
RUN apt install -y build-essential git ninja-build cmake mtools parted dosfstools device-tree-compiler
RUN apt install -y gcc-14-aarch64-linux-gnu g++-14-aarch64-linux-gnu clang lld
RUN apt install -y nfs-common xxd
RUN cp /usr/aarch64-linux-gnu/include/gnu/stubs-lp64.h /usr/aarch64-linux-gnu/include/gnu/stubs-lp64_be.h
