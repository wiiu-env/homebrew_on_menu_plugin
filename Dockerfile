FROM ghcr.io/wiiu-env/devkitppc:20231112

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:0.8.0-dev-20240329-562c749 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libsdutils:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwuhbutils:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20230621 /artifacts $DEVKITPRO

WORKDIR project
