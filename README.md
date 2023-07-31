[![CI-Release](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml)

# Homebrew on menu

This plugin allows you to boot homebrew directly from your Wii U Menu.

## Installation
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `homebrew_on_menu.wps` into `sd:/wiiu/environments/[ENVIRONMENT]/plugins`.  
2. Requires the [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
3. Requires the [RPXLoadingModule](https://github.com/wiiu-env/RPXLoadingModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
4. Requires the [WUHBUtilsModule](https://github.com/wiiu-env/WUHBUtilsModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
5. Requires the [ContentRedirectionModule](https://github.com/wiiu-env/ContentRedirectionModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
6. Requires the [SDHotSwapModule](https://github.com/wiiu-env/SDHotSwapModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
7. Requires the [NotificationModule](https://github.com/wiiu-env/NotificationModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.

## Usage

Place your homebrew (`.rpx` or `.wuhb`) in `sd:/wiiu/apps` or any subdirectory inside `sd:/wiiu/apps`.

Via the plugin config menu (press L, DPAD Down and Minus on the gamepad) you can configure the plugin. The available options are the following:
- **Features**:
  - Hide all homebrew [except Homebrew Launcher]:  (Default is false)
    - Hides all homebrew from the Wii U Menu except the `sd:/wiiu/apps/homebrew_launcher.wuhb` and `sd:/wiiu/apps/homebrew_launcher/homebrew_launcher.wuhb`
    - This config item is called "Hide all homebrew" when no `homebrew_launcher.wuhb` is present.
  - Prefer .wuhb over .rpx (Default is true)
    - Hides a `.rpx` from the Wii U Menu if a `.wuhb` with the same name exists in the same directory.
  - Hide all .rpx (Default is false)
    - Hides all `.rpx` from the Wii U Menu.

## Hide specific apps

A list of executables to hide can be provided via `sd:/wiiu/apps/.ignore`. This file is a text file, each line contains a file/pattern to exclude. 
Every line is considered case-insensitive and relative to `sd:/wiiu/apps/`. Lines starting with "#" will be ignored and can be used for comments. `*` is supported as a wildcard.

Example:
```
# Ignore any .rpx in the my_game sub directory
my_game/*.rpx
# Ignore any executable in the other_game sub directory
other_game/*
# Ignore any executable that starts with "retroarch"
retroarch*
*/retroarch*
# Ignore sd:/wiiu/apps/CoolGame.wuhb
CoolGame.wuhb
```

## Save data redirection
In order to preserve the order of homebrew apps even when you run the Wii U Menu without this plugin, this plugin will redirect the Wii U Menu save data to 
`sd:/wiiu/homebrew_on_menu_plugin/[SerialNumberOfTheConsole]/save`. When no save data is found on the sd card, the current save data is copied from the console,
but after that it's never updated.

**If the plugin is configured to hide any homebrew except a Homebrew Launcher, the redirection is disabled.**

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

`docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./src -i`