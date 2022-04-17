FROM wiiuenv/devkitppc:20220417

COPY --from=wiiuenv/wiiupluginsystem:20220123 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220417 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libsdutils:20220303 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libwuhbutils:20220415 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220414 /artifacts $DEVKITPRO

WORKDIR project