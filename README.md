# Project

- **Project Name**: Environmental Sensor
- **Summary**: An environmental sensor designed to measure air temperature and humidity.

## Getting started

Before getting started, make sure you have a proper nRF Connect SDK development environment.
Follow the official
[Getting started guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html).

### Initialization

```shell
# Prepare workspace and virtual env
mkdir ws
python -m venv --copies ws/.venv
. ws/.venv/bin/activate
pip install west

# Initialize workspace
west init -m git@github.com:TAREQ-TBZ/env_sensor.git --mr main ws
cd ws
west update

# Install additional requirements
pip install -r zephyr/scripts/requirements.txt
pip install -r nrf/scripts/requirements.txt
pip install -r bootloader/mcuboot/scripts/requirements.txt
```

### Building and running

To build the main application, run the following command:

```shell
west build --sysbuild -b <YOURBOARD> application/app
```

> **Note**
> This assumes your Zephyr SDK is installed in a well-known location. Otherwise, you might want to
> set `ZEPHYR_SDK_INSTALL_DIR` in your environment.
>
To flash the firmware:

```shell
west flash
```

> [!TIP]
> To flash with openocd you can specify your interface's script name without `.cfg` using
> `-DOPENOCD_NRF5_INTERFACE=jlink` (default if not provided). For a CMSIS-DAP compatible probe, use
> `-DOPENOCD_NRF5_INTERFACE=cmsis-dap`. This has to be done at config time, so, e.g., when calling
> `west build ...` the first time or when calling `west build` with `-p` parameter.

## Building with vscode

Add the board folder and application to NRF Connect in your .vscode/settings.json

```json
{
    "nrf-connect.applications": [
        "${workspaceFolder}/application/app"
    ],
    "nrf-connect.boardRoots": [
        "${workspaceFolder}/application/"
    ]
}
```
