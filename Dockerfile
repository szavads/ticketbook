# ── Stage 1: builder ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        python3 \
        python3-pip \
    && pip3 install "conan>=2.0" \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN conan profile detect --force \
    && conan install . --output-folder=build --build=missing \
           -s build_type=Release \
    && cmake -B build \
           -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
           -DCMAKE_BUILD_TYPE=Release \
           -DBUILD_TESTS=OFF \
    && cmake --build build

# ── Stage 2: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/ticketbook /usr/local/bin/ticketbook

ENTRYPOINT ["ticketbook"]
