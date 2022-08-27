FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/wiiupluginsystem:20220826 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220825 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libsdutils:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libwuhbutils:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220724 /artifacts $DEVKITPRO

WORKDIR project