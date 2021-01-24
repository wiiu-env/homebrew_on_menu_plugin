FROM wiiuenv/devkitppc:20210101

COPY --from=wiiuenv/wiiupluginsystem:20210109 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210116 /artifacts $DEVKITPRO

WORKDIR project