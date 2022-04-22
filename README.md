[![CI-Release](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml)

## Usage
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `homebrew_on_menu.wps` into `sd:/wiiu/environments/[ENVIRONMENT]/plugins`.  
2. Requires the [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
3. Requires the [RPXLoadingModule](https://github.com/wiiu-env/RPXLoadingModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
4. Requires the [WUHBUtilsModule](https://github.com/wiiu-env/WUHBUtilsModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
5. Requires the [ContentRedirectionModule](https://github.com/wiiu-env/ContentRedirectionModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
6. Requires the [SDHotSwapModule](https://github.com/wiiu-env/SDHotSwapModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.

## Save data redirection
In order to preserve the order of homebrew apps even when you run the Wii U Menu without this plugin, this plugin will redirect the Wii U Menu save data to `sd:/wiiu/homebrew_on_menu_plugin`. 
When no save data is found on the sd card, the current save data is copied from the console, but after that it's never updated.

## Buildflags

### Logging
Building via `make` only logs errors (via OSReport). To enable logging via the [LoggingModule](https://github.com/wiiu-env/LoggingModule) set `DEBUG` to `1` or `VERBOSE`.

`make` Logs errors only (via OSReport).  
`make DEBUG=1` Enables information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).  
`make DEBUG=VERBOSE` Enables verbose information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).

If the [LoggingModule](https://github.com/wiiu-env/LoggingModule) is not present, it'll fallback to UDP (Port 4405) and [CafeOS](https://github.com/wiiu-env/USBSerialLoggingModule) logging.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t homebrew_on_menu_plugin-builder

# make 
docker run -it --rm -v ${PWD}:/project homebrew_on_menu_plugin-builder make

# make clean
docker run -it --rm -v ${PWD}:/project homebrew_on_menu_plugin-builder make clean
```

## Format the code via docker

`docker run --rm -v ${PWD}:/src wiiuenv/clang-format:13.0.0-2 -r ./src -i`