FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential flex bison gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 \
    pkg-config git ca-certificates libx11-dev libxext-dev libfreetype-dev \
    libfontconfig-dev libxrender-dev libxrandr-dev libxi-dev libxcursor-dev \
    libxinerama-dev libxxf86vm-dev libgl-dev libgnutls28-dev libasound2-dev \
    libpulse-dev libdbus-1-dev libudev-dev libvulkan-dev libpng-dev libjpeg-dev \
    libtiff-dev liblcms2-dev libxml2-dev libxslt1-dev libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev libunwind-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
