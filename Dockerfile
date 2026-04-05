FROM ghcr.io/wiiu-env/devkitppc:20260225

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:abifix-dev-20260403-fa90de3 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20260329 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libsdutils:20260331 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwuhbutils:20260329 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:abisafe-dev-20260403-502a496 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20260331 /artifacts $DEVKITPRO

WORKDIR project
