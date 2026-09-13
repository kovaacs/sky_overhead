# Pin the base image and Debian package snapshot together when updating tools.
FROM debian:bookworm-slim@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171 AS toolchain

ARG TARGETARCH
RUN test "$TARGETARCH" = arm64

RUN rm /etc/apt/sources.list.d/debian.sources \
    && printf '%s\n' \
      'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/20260825T000000Z bookworm main' \
      'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian-security/20260825T000000Z bookworm-security main' \
      > /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
      ca-certificates curl git g++ python3 librsvg2-bin imagemagick \
      zip libdigest-sha-perl xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL --retry 3 \
      https://github.com/arduino/arduino-cli/releases/download/v1.5.1/arduino-cli_1.5.1_Linux_ARM64.tar.gz \
      -o /tmp/arduino-cli.tar.gz \
    && printf '%s\n' '1e69e077479f300614d4551334e0a33f08ee40b04315d83b8e7e0e94f0d0ee62  /tmp/arduino-cli.tar.gz' | sha256sum -c - \
    && tar -xzf /tmp/arduino-cli.tar.gz -C /usr/local/bin arduino-cli \
    && rm /tmp/arduino-cli.tar.gz

# Fixed paths and compiler timestamps keep host paths and wall-clock time out.
ENV ARDUINO_DIRECTORIES_USER=/workspace/sky_overhead/.arduino-sketchbook \
    ARDUINO_DIRECTORIES_DATA=/opt/arduino/data \
    ARDUINO_DIRECTORIES_DOWNLOADS=/opt/arduino/downloads \
    SOURCE_DATE_EPOCH=1704067200 \
    TZ=UTC \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8
WORKDIR /workspace/sky_overhead
COPY tools/setup_arduino_dependencies.sh tools/setup_arduino_dependencies.sh
RUN sh tools/setup_arduino_dependencies.sh

FROM toolchain AS source
COPY *.h *.ino sketch.yaml ./
COPY tools/ tools/
COPY README.md FLASHING.md config.example.txt THIRD_PARTY_NOTICES.md ./

FROM source AS tests
RUN sh tools/run_unit_tests.sh

# Building firmware also requires the tests to pass.
FROM tests AS firmware-build
RUN sh tools/build_firmware.sh --clean --jobs 2 \
      --build-path /tmp/sky-overhead-build --output-dir /out/firmware

FROM firmware-build AS release-build
ARG RELEASE_VERSION=v0.0.0
RUN sh tools/package_release.sh "$RELEASE_VERSION" /out/firmware /out/release \
    && cd /out/release && shasum -a 256 -c SHA256SUMS

FROM scratch AS firmware
COPY --from=firmware-build /out/firmware/ /

FROM scratch AS release
COPY --from=release-build /out/release/ /
