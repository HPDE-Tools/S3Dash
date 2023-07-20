# LiligoS3Dash

## Init Git Submodules

We use submodules to manage components. To download the deps, follow the instructions below:

```
git submodule init
git submodule update
```

## Environment setup

Follow [ESP32 installation guide](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/get-started/index.html#installation) to setup your local environment.

The rest of the instructions assume you run `idf.py` commands within the ESP-IDF PowerShell.

## Build image

```
idf.py set-target esp32s3
idf.py build
```

For successive builds, you do not need to reun `set-target`.

## Flash image onto device

Once the build succeeds, we can flash the firmware to device. Plug in the display over USB and find the port through Windows Device Manager. Look in `Ports` and you should see something that looks like `USB Serial Device (COM1)`.

```
idf.py -p COM1 flash
```
