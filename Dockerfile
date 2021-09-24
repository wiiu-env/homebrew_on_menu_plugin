FROM wiiuenv/devkitppc:20210920

COPY --from=wiiuenv/wiiupluginsystem:20210924 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210924 /artifacts $DEVKITPRO

WORKDIR project