## Usage
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `homebrew_on_menu.wps` into `sd:/wiiu/environments/[ENVIRONMENT]/plugins`.  
2. Requires the [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
3. Requires the [RPXLoadingModule](https://github.com/wiiu-env/RPXLoadingModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.

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
