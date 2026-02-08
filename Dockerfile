FROM ghcr.io/wiiu-env/devkitppc:20260204

COPY --from=ghcr.io/wiiu-env/libkernel:20260208 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20260208  /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20260126 /artifacts $DEVKITPRO

WORKDIR /project
