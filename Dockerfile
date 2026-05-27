# syntax=docker/dockerfile:1.7

ARG DEBIAN_VERSION=bookworm

FROM debian:${DEBIAN_VERSION}-slim AS base-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        build-essential \
        pkg-config \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY configure.ac Makefile.am ./
COPY src ./src
COPY test ./test
COPY cov_build.sh ./cov_build.sh

RUN chmod +x ./cov_build.sh ./test/run_ut.sh

FROM base-build AS build

# Build with JSON and TESTME support so the same binary can run unit tests.
RUN ./cov_build.sh --enable-cjson --test \
    && strip --strip-unneeded ./meminsight || true

FROM build AS test

# Executes fixture-driven unit tests. Build with:
#   docker build --target test .
RUN ./test/run_ut.sh

FROM debian:${DEBIAN_VERSION}-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libcjson1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/meminsight

COPY --from=build /src/meminsight /usr/local/bin/meminsight

ENTRYPOINT ["/usr/local/bin/meminsight"]
CMD ["--help"]
