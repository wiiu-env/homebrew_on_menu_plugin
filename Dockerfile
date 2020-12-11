FROM wiiuenv/devkitppc:20200810

COPY --from=wiiuenv/wiiupluginsystem:20200829 /artifacts $DEVKITPRO

WORKDIR project