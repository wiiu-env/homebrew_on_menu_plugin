FROM ghcr.io/wiiu-env/devkitppc:20220806

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20220903 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libsdutils:20220903 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwuhbutils:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:20220903 /artifacts $DEVKITPRO

WORKDIR project
