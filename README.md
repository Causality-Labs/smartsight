# smartsight Firmware

Cortex-M7 firmware for the i.MX8M Plus (FRDM-IMX8MP), built with the MCUXpresso SDK.
The SDK has no FRDM board, so we build for `evkmimx8mp` with FRDM-adapted board files.

```
firmware/
├── docker/           # build environment
├── m7_app/           # our application (freestanding SDK project)
└── mcuxpresso-sdk/   # MCUXpresso SDK (not committed, don't edit)
```

## Docker

The image contains the Arm GNU toolchain, CMake/Ninja, west and the SDK's Python dependencies.
The `firmware/` folder is mounted at `/workspace`, so files are shared with the host.

```bash
./docker/dev.sh            # build the image (first run) and open a shell
./docker/dev.sh --rebuild  # rebuild after changing the Dockerfile
```

- `docker/sdk-requirements.txt` is a copy of `mcuxsdk/scripts/requirements-base.txt`. Re-copy it and rebuild after updating the SDK.
- `SdkRootDirPath` is set to `/workspace/mcuxpresso-sdk/mcuxsdk`. Update the Dockerfile if the SDK moves.

## SDK setup (once)

Inside the container:

```bash
cd /workspace
west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests mcuxpresso-sdk
cd mcuxpresso-sdk
west update_board --set board evkmimx8mp
```

## Building the application

Inside the container, from the SDK folder:

```bash
cd /workspace/mcuxpresso-sdk/mcuxsdk

# First build, or after changing config / adding files
west build -b evkmimx8mp /workspace/m7_app -d /workspace/m7_app/build --config release -p always

# Rebuild after code changes
west build -d /workspace/m7_app/build
```

Output in `m7_app/build/`:
- `.elf` for Linux remoteproc
- `.bin` for U-Boot `bootaux`

Use `--config release` or `debug` to run from TCM, and `ddr_release` or `ddr_debug` to run from DDR.

## Running on the target

The M7 is started from Linux with remoteproc. The M7 debug console is UART4, so UART4 must be disabled in the Linux device tree.

1. Copy the `.elf` to the target. remoteproc loads firmware from `/lib/firmware`:

   ```bash
   # On the host
   scp m7_app/build/smartsight_firmware_cm7.elf root@10.10.10.28:/lib/firmware/
   ```

2. Tell remoteproc which firmware file to load:

   ```bash
   # On the target
   echo -n smartsight_firmware_cm7.elf > /sys/class/remoteproc/remoteproc0/firmware
   ```

3. Check that the M7 isn't already running. If the state is `running`, stop it first (step 5):

   ```bash
   cat /sys/class/remoteproc/remoteproc0/state   # should print: offline
   ```

4. Start the M7:

   ```bash
   echo start > /sys/class/remoteproc/remoteproc0/state
   cat /sys/class/remoteproc/remoteproc0/state   # should print: running
   ```

   The firmware's output appears on the M7 debug UART (UART4).

5. Stop the M7:

   ```bash
   echo stop > /sys/class/remoteproc/remoteproc0/state
   cat /sys/class/remoteproc/remoteproc0/state   # should print: offline
   ```

To load a new build, stop the M7, copy the new `.elf` over the old one, and start it again. Step 2 is only needed when the file name changes.

## Where to change things

| What | Where |
|---|---|
| Application code | `m7_app/main.c` |
| Enabled drivers/components | `m7_app/prj.conf` |
| Board setup (FRDM changes) | `m7_app/evkmimx8mp/evkmimx8mp/` |
| Pins, startup, debug UART | `m7_app/evkmimx8mp/remote/` |
