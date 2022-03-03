FROM wiiuenv/devkitppc:20220303

COPY --from=wiiuenv/wiiupluginsystem:20220123 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220212 /artifacts $DEVKITPRO

WORKDIR project