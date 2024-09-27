# Use an official Ubuntu image as the base image
FROM ubuntu:24.04

# Set the environment variables to prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
    git \
    build-essential \
    meson \
    ninja-build \
    pkg-config \
    nlohmann-json3-dev \
    libsqlite3-dev \ 
    libpugixml-dev \
    libpq-dev \
    cmake

# Set the working directory inside the container
WORKDIR /app

# Copy the project files to the container
COPY . .

# Create build directory, compile project with meson and install
RUN meson setup build && \
   meson compile -C build &&\
   cp -r build/* /usr/local/bin/

# Default command to run when the container starts (modify as needed)
CMD ["tkserv"]
