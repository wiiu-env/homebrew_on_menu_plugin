FROM wiiuenv/devkitppc:20210414

COPY --from=wiiuenv/wiiupluginsystem:20210417 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210116 /artifacts $DEVKITPRO

WORKDIR project