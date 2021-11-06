FROM wiiuenv/devkitppc:20211106

COPY --from=wiiuenv/wiiupluginsystem:20211001 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20211002 /artifacts $DEVKITPRO

WORKDIR project