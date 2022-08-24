FROM wiiuenv/devkitppc:20220724

COPY --from=wiiuenv/wiiupluginsystem:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libsdutils:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libwuhbutils:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220724 /artifacts $DEVKITPRO

WORKDIR project