FROM wiiuenv/devkitppc:20210101

COPY --from=wiiuenv/libkernel:20200812 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libfunctionpatcher:20200812 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20210101 /artifacts $DEVKITPRO

WORKDIR project