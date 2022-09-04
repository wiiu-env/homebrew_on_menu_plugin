FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/wiiupluginsystem:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220903 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libsdutils:20220903 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libwuhbutils:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220903 /artifacts $DEVKITPRO

WORKDIR project
