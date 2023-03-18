FROM ghcr.io/wiiu-env/devkitppc:20221228

COPY --from=ghcr.io/wiiu-env/libkernel:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230107 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20230106 /artifacts $DEVKITPRO

WORKDIR project
