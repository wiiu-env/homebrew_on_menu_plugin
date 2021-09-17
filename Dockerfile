FROM wiiuenv/devkitppc:20210917

COPY --from=wiiuenv/wiiupluginsystem:20210917 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210116 /artifacts $DEVKITPRO

WORKDIR project