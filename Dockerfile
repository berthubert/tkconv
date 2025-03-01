# Use an official Ubuntu image as the base image
FROM ubuntu:24.04

# Set the environment variables to prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
    git \
    build-essential \
    catdoc \
    meson \
    ninja-build \
    pandoc \
    pkg-config \
    poppler-utils \
    nlohmann-json3-dev \
    libsqlite3-dev \
    libpugixml-dev \
    libpq-dev \
    sqlite3 \
    cmake \
    xmlstarlet

# Set the working directory inside the container
WORKDIR /workdir

# Copy the project files to the container
COPY . .

# Create build directory, compile project with meson and install
RUN meson setup build && \
   meson compile -C build &&\
   cp -r build/* /usr/local/bin/ &&\
   cp docker/tksync.sh /usr/local/bin/tksync &&\
   chmod +x /usr/local/bin/tksync

WORKDIR /app

RUN cp -r /workdir/html/ html/ &&\
    cp -r /workdir/partials/ partials/ &&\
    cp -r /workdir/build/ build/ &&\
    cp /workdir/tk.xslt tk.xslt &&\
    cp /workdir/tk-div.xslt tk-div.xslt

# Default command to run when the container starts (modify as needed)
# start with `tksync` to loop through all sync scripts
CMD ["tkserv"]
