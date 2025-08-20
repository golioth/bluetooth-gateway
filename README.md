# Golioth Bluetooth Gateway

Golioth Bluetooth Gateway is a device capable of connecting to Golioth
cloud and serving as proxy for Bluetooth-only devices (nodes). All
communication between Bluetooth nodes and cloud is end-to-end encrypted
and authenticated. This means that gateway serves as untrusted two-way
channel between Bluetooth devices and Golioth cloud.

## Flash precompiled binaries

### nRF9160 DK

Bluetooth controller is running on nRF52840. This means that proper
firmware needs to be flashed (HCI controller over UART) in order to
access Bluetooth from nRF9160 chip.

This is done by by changing `SWD` switch (`SW10`) from `nRF91` to
`nRF52` on development kit, then downloading and flashing firmware with:

```
wget https://github.com/golioth/bluetooth-gateway/releases/latest/download/nrf9160dk_nrf52840.hex -O nrf9160dk_nrf52840.hex
nrfutil device program --firmware nrf9160dk_nrf52840.hex --x-family nrf52
```

Gateway firmware runs on nRF9160 chip. There is direct access to LTE
modem and also Bluetooth Host stack, which communicates with Bluetooth
Controller over UART. Download and flash it with:

```
wget https://github.com/golioth/bluetooth-gateway/releases/latest/download/nrf9160dk_nrf9160.hex -O nrf9160dk_nrf9160.hex
nrfutil device program --firmware nrf9160dk_nrf9160.hex --x-family nrf91
```

## Setup

Setup repo with NCS:

```
west init -m https://github.com/golioth/bluetooth-gateway.git --mf west-ncs.yml
west update
west patch apply
```

## Build and run

### Thingy:91 X

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

### nRF9160 DK

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

## Useful options for debugging and development

### Running gateway with simulator or USB dongle

See [Running Bluetooth Gateway with
simulator](doc/simulator_or_usb_dongle.md).

### `CONFIG_GATEWAY_CLOUD`

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
