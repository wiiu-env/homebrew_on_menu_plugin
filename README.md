[![CI-Release](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/homebrew_on_menu_plugin/actions/workflows/ci.yml)

## Usage
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `homebrew_on_menu.wps` into `sd:/wiiu/environments/[ENVIRONMENT]/plugins`.  
2. Requires the [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
3. Requires the [RPXLoadingModule](https://github.com/wiiu-env/RPXLoadingModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.

## Save data redirection
In order to preserve the order of homebrew apps even when you run the Wii U Menu without this plugin, this plugin will redirect the Wii U Menu save data to `sd:/wiiu/homebrew_on_menu_plugin`. 
When no save data is found on the sd card, the current save data is copied from the console, but after that it's never updated.

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