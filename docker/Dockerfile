# Headless Throne: ghcr.io/throneproj/throne. https://throneproj.github.io/headless/ covers running it; compose.yaml is an example.
#
# The build context holds the bundled Linux builds as linux-amd64/ and linux-arm64/ (one is enough for a single platform):
#   - CI: each Throne-<tag>-linux-<arch>.zip unzipped, its Throne/ dir renamed to linux-<arch>/;
#   - locally: deployment/ after a build (Dockerfile.dockerignore sends only those dirs, without Throne.debug).
# THRONE_API_REF is the throneproj/Throne-api commit thronectl is built from; CI passes build.yml's THRONE_API_REF.
#   docker build -f docker/Dockerfile --build-arg THRONE_API_REF=<commit> -t throne deployment
#   docker buildx build --platform linux/amd64,linux/arm64 -f docker/Dockerfile --build-arg THRONE_API_REF=<commit> <context>

# Matches the go-version in build.yml.
ARG GO_VERSION=1.27.0

FROM --platform=$BUILDPLATFORM golang:${GO_VERSION} AS thronectl
ARG THRONE_API_REF
ARG TARGETOS
ARG TARGETARCH
WORKDIR /src
RUN set -e; \
    if [ -z "$THRONE_API_REF" ]; then echo "pass --build-arg THRONE_API_REF=<Throne-api commit>" >&2; exit 1; fi; \
    git init -q .; \
    git fetch -q --depth=1 https://github.com/throneproj/Throne-api "$THRONE_API_REF" \
        || { echo "Throne-api $THRONE_API_REF is not on GitHub" >&2; exit 1; }; \
    git checkout -q FETCH_HEAD
RUN CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH go build -trimpath -o /out/thronectl ./cmd/thronectl

FROM ubuntu:24.04
ARG TARGETARCH
ARG DEBIAN_FRONTEND=noninteractive
LABEL org.opencontainers.image.title="Throne" \
      org.opencontainers.image.description="Throne proxy manager, headless; managed with thronectl" \
      org.opencontainers.image.source="https://github.com/throneproj/Throne" \
      org.opencontainers.image.licenses="GPL-3.0-or-later"
# --headless runs the desktop binary, so the libraries Qt's GUI modules link must load even though no window ever opens.
# libgssapi-krb5-2: Qt's official QtNetwork links it (Negotiate authentication); CI runners have it preinstalled.
# No machine-id in the image: each container then generates its own HWID into the data dir instead of every pull sharing one.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates tini \
        libegl1 libgl1 libopengl0 libglx0 libfontconfig1 libfreetype6 libxkbcommon0 libx11-6 libx11-xcb1 libxcb1 \
        libdbus-1-3 libglib2.0-0t64 libssl3t64 libgssapi-krb5-2 \
 && rm -rf /var/lib/apt/lists/* /etc/machine-id /var/lib/dbus/machine-id \
 && install -d -m 0700 /var/lib/throne
COPY linux-${TARGETARCH}/ /opt/Throne/
COPY --from=thronectl /out/thronectl /usr/local/bin/thronectl
# A missing library fails the build here rather than in the first container's dynamic loader.
RUN missing=$(ldd /opt/Throne/Throne /opt/Throne/ThroneCore 2>&1 | grep -F 'not found'); \
    if [ -n "$missing" ]; then echo "$missing" >&2; exit 1; fi

# Change the data dir with THRONE_DATA_DIR, not --data-dir: the health check finds the instance through it.
ENV THRONE_DATA_DIR=/var/lib/throne \
    THRONE_RESTART_MODE=exit \
    HOME=/var/lib/throne
VOLUME /var/lib/throne
# --health exits 0 healthy, 1 unhealthy or starting, 2 no instance; Docker counts any non-zero as a failed probe.
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s CMD ["/opt/Throne/Throne", "--health"]
STOPSIGNAL SIGTERM
# tini without -g: SIGTERM reaches Throne alone, which then stops the core and the kill switch in order.
ENTRYPOINT ["/usr/bin/tini", "--", "/opt/Throne/Throne", "--headless"]
