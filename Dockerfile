# iWord development and testing image
#
# Usage:
#   docker build -t iword .
#   docker run --rm iword iwordctl version
#
# For interactive development:
#   docker run --rm -it -v $(pwd):/workspace iword bash
#
# Note: System V SHM works inside Docker on Linux hosts.
# On macOS (Docker Desktop), SHM is available but limited to --shm-size.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    cppcheck \
    php-dev \
    php-cli \
    python3 \
    python3-pip \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

# Install Go
RUN curl -fsSL https://go.dev/dl/go1.22.0.linux-amd64.tar.gz \
    | tar -C /usr/local -xz
ENV PATH="/usr/local/go/bin:${PATH}"

WORKDIR /workspace
COPY . .

# Build C tools and PECL extension
RUN make tool && make pecl

# Smoke test
RUN cd tool && cp -r ../include/* . && \
    printf "apple\t9\nspam_word\t2\n" > /tmp/test_dict.txt && \
    ./iwordctl load /tmp/test_dict.txt && \
    ./iwordctl seek apple && \
    ./iwordctl --json seek apple && \
    ./iwordctl --json status && \
    ./iwordctl stop

CMD ["/workspace/bin/iwordctl", "version"]
