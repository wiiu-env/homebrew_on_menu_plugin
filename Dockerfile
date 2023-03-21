FROM ghcr.io/wiiu-env/devkitppc:20230218

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20230316 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20230316 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libsdutils:20220903 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwuhbutils:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:20221010 /artifacts $DEVKITPRO

WORKDIR project
