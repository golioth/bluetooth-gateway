# Golioth Bluetooth Gateway

Golioth Bluetooth Gateway is a device capable of connecting to Golioth
cloud and serving as proxy for Bluetooth-only devices (nodes). All
communication between Bluetooth nodes and cloud is end-to-end encrypted
and authenticated. This means that gateway serves as untrusted two-way
channel between Bluetooth devices and Golioth cloud.

## Getting Started

### Supported Hardware

This application should work on any board with support in Zephyr or nRF Connect
SDK that has Bluetooth and Internet capability, though some custom
configuration is likely required. The easiest way to get started is to use one
of the following targets that are already configured for this application:

- NXP FRDM-RW612
- Nordic Thingy:91 X
- Nordic nRF9160DK

### Release Binaries

Pre-built firmware binaries for the above hardware are available in
[GitHub](https://github.com/golioth/bluetooth-gateway/releases/latest). We
recommend using these binaries as the fastest way to get started.

<details>

<summary>Flashing the FRDM-RW612</summary>

1. Install
[JLink Commander](https://www.segger.com/products/debug-probes/j-link/tools/j-link-commander/).
2. Program the FRDM-RW612 Gateway Firmware.

    a. Connect to the device with JLink Commander
    ```
    <JLink Commander Executable> -device rw612 -if swd -speed 4000 -autoconnect 1
    ```
    where `<JLink Commander Executable>` is `JLink.exe` on Windows and `JLinkExe`
    on Linux and MacOS.

    b. Issue the following commands in JLink Commander
    ```
    loadfile frdm_rw612.hex
    reset
    exit
    ```

</details>

<details>

<summary>Flashing the Thingy:91 X</summary>

1. Install the
[`nrfutil`](https://www.nordicsemi.com/Products/Development-tools/nRF-Util)
CLI tool.
2. Program the nRF5340 Bluetooth Controller Firmware

    a. Position the SWD selection switch (`SW2`) to `nRF53`

    b. Issue the following command:
    ```
    nrfutil device program --firmware thingy91x_nrf5340.hex --x-family nrf53
    ```
3. Program the nRF9151 Gateway Firmware

    a. Power cycle the device and position the SWD selection switch (`SW2`) to
    `nRF91`

    b. Issue the following command:
    ```
    nrfutil device program --firmware thingy91x_nrf9151.hex --x-family nrf91
    ```

</details>

<details>

<summary>Flashing the nRF9160DK</summary>

1. Install the
[`nrfutil`](https://www.nordicsemi.com/Products/Development-tools/nRF-Util)
CLI tool.
2. Program the nRF52840 Bluetooth Controller Firmware

    a. Position the SWD selection switch (`SW10`) to `nRF52`

    b. Issue the following command:
    ```
    nrfutil device program --firmware nrf9160dk_nrf52840.hex --x-family nrf52
    ```
3. Program the nRF9160 Gateway Firmware

    a. Power cycle the device and position the SWD selection switch (`SW10`) to
    `nRF91`

    b. Issue the following command:
    ```
    nrfutil device program --firmware nrf9160dk_nrf9160.hex --x-family nrf91
    ```

</details>

### Provisioning

To use the Gateway, it needs to be provisioned with credentials for Golioth.
For demonstration purposes, the Gateway firmware uses Pre-Shared Keys (PSK). In
production settings, Golioth recommends using certificates issued from customer
owned Public Key Infrastructure (PKI).

1. [Create a Device](https://docs.golioth.io/getting-started/console/register#creating-a-new-device)
in your Golioth Project.
2. Connect to your Gateway over serial and issue the following commands:
```
settings set golioth/psk-id <psk-id>
settings set golioth/psk <psk>
```
where `<psk-id>` and `<psk>` are the device credentials you created in step 1.

## Building from Source

### Setup

Setup repo with NCS:

```
west init -m https://github.com/golioth/bluetooth-gateway.git --mf west-ncs.yml
west update
west patch apply
```

### Build and run

#### Thingy:91 X

Controller on nRF5340 NET core does not work yet with newest NCS
version (for unknown reason yet), so it is required to use older version
of NCS which is specified in `west-thingy91x-controller.yml`. In order
to checkout proper revision and apply required patches type:

```
west config manifest.file west-thingy91x-controller.yml && west update
west forall zephyr -c "git am $(west topdir)/bluetooth-gateway/zephyr/patches/thingy91x-controller/zephyr/*.patch"
```

Bluetooth controller is running on the nRF5340 NET core. This means that proper
firmware needs to be flashed (HCI controller over UART) in order to
access Bluetooth from nRF9151 chip.

This is done by by changing `SWD` switch (`SW2`) from `nRF91` to `nRF53`
on Thingy, then building and flashing firmware with:

```
west build -p -b thingy91x/nrf5340/cpunet bluetooth-gateway/controller --sysbuild -- -DSB_CONF_FILE=sysbuild/nrf5340_cpuapp.conf
west flash
```

Gateway firmware runs on nRF9151 chip. There is direct access to LTE
modem and also Bluetooth Host stack, which communicates with Bluetooth
Controller over UART. Build and flash it with:

```
west build -p -b thingy91x/nrf9151/ns bluetooth-gateway/gateway --sysbuild
west flash
```

#### nRF9160 DK

Bluetooth controller is running on nRF52840. This means that proper
firmware needs to be flashed (HCI controller over UART) in order to
access Bluetooth from nRF9160 chip.

This is done by by changing `SWD` switch (`SW10`) from `nRF91` to
`nRF52` on development kit, then building and flashing firmware with:

```
west build -p -b nrf9160dk/nrf52840 bluetooth-gateway/controller
west flash
```

Gateway firmware runs on nRF9160 chip. There is direct access to LTE
modem and also Bluetooth Host stack, which communicats with Bluetooth
Controller over UART. Build and flash it with:

```
west build -p -b nrf9160dk/nrf9160/ns bluetooth-gateway/gateway --sysbuild
west flash
```

### Useful options for debugging and development

#### Running gateway with simulator or USB dongle

See [Running Bluetooth Gateway with
simulator](doc/simulator_or_usb_dongle.md).

#### `CONFIG_GATEWAY_CLOUD`

It is possible to disable communication with cloud, so that only
Bluetooth part is tested. In context of `nrf52_bsim` this allows to run
without BabbleSim handbrake, which normally slows down simulation to
respect communication timeouts enforced in SDK. In case of cellular
connected platforms (nRF91) there is no latency because of cellular
network infrastructure.

`CONFIG_GATEWAY_CLOUD` is available both on the gateway applicaton level
(`bluetooth-gateway/gateway`) as well as in sysbuild (mainly for ease of
use) as `SB_CONFIG_GATEWAY_CLOUD`.

Running `nrf52_bsim` simulaton without cloud communication can be done
with:

```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -DSB_CONFIG_GATEWAY_CLOUD=n
west flash
```
