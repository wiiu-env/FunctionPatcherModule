## Usage
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `FunctionPatcherModule.wms` into `sd:/wiiu/environments/[ENVIRONMENT]/modules`.  
2. Requires the [WUMSLoader](https://github.com/wiiu-env/WUMSLoader) in `sd:/wiiu/environments/[ENVIRONMENT]/modules/setup`.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t functionpatchermodule-builder

# make 
docker run -it --rm -v ${PWD}:/project functionpatchermodule-builder make

# make clean
docker run -it --rm -v ${PWD}:/project functionpatchermodule-builder make clean
```