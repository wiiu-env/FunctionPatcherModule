FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libkernel:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libfunctionpatcher:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20220724 /artifacts $DEVKITPRO

WORKDIR project
