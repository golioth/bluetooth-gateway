# Golioth Bluetooth Gateway

## Setup

NCS for `thingy91x` and `nrf9160dk`:

```
west init -m https://github.com/golioth/bluetooth-gateway.git --mf west-ncs.yml
west update
west patch apply
```

Vanilla Zephyr for `nrf52_bsim` and `native_sim`:
```
west init -m https://github.com/golioth/bluetooth-gateway.git --mf west-zephyr.yml
west update
west patch apply
```

Quick switch between Zephyr and NCS:

```
west config manifest.file west-zephyr.yml && west update && west patch apply
west config manifest.file west-ncs.yml && west update && west patch apply
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

### nrf52_bsim with sysbuild

Sysbuild is a generic multi-component build (and flash) infrastructure
in Zephyr. In context of `nrf52_bsim` it is used to build several
BabbleSim components. The main component is gateway. Additional sysbuild
components are BabbleSim programs:

  * 2.4G PHY implementation (coordinator of the whole simulation)
  * Bluetooth peripherals: either generic samples from
    `zephyr/bluetooth/peripheral*` or BLE GATT nodes like
    `pouch/examples/ble_gatt`)
  * (optional) BabbleSim handbrake

Default sysbuild configuration builds following:

  * `bluetooth-gateway/gateway` (main component)
  * `pouch/examples/zephyr/ble_gatt` (BLE GATT node)
  * `tools/bsim/bin/bs_2G4_phy_v1` (2.4G PHY BabbleSim coordinator)
  * `tools/bsim/bin/bs_device_handbrake` (BabbleSim handbrake, used to
    slow down simulation to almost realtime)

Besides building several components sysbuild support flashing all of
them sequentially (when talking about hardware), which in most common
use case is bootloader followed by application. In case of `nrf52_bsim`
flashing means running. Running BabbleSim components sequentially does
not make sense, since all programs need to be run in parallel. That is
handled automatically by custom runners implemented in
`scripts/runners/bsim_*.py`, which collect information about simulation
and run all programs.

Default example of running gateway with BLE GATT node (and required
BabbleSim components) is done with following commands:

```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -Dgateway_CONFIG_GOLIOTH_COAP_HOST_URI='"coaps://coap.golioth.dev"' \
  -Dperipheral_ble_gatt_example_0_CONFIG_EXAMPLE_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"'
west flash
```

It is possible to include more BLE GATT nodes in simulation. Example of
running 2 nodes:
```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -Dgateway_CONFIG_GOLIOTH_COAP_HOST_URI='"coaps://coap.golioth.dev"' \
  -DSB_CONFIG_PERIPHERAL_BLE_GATT_EXAMPLE_NUM=2 \
  -Dperipheral_ble_gatt_example_0_CONFIG_POUCH_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"' \
  -Dperipheral_ble_gatt_example_1_CONFIG_POUCH_DEVICE_ID='"bbbbbbbbbbbbbbbbbbbbbbbb"'
west flash
```

It is even possible to include other non-BLE GATT nodes. So far there
is support for vanilla Zephyr peripheral sample
`zephyr/samples/bluetooth/peripheral`:

```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -Dgateway_CONFIG_GOLIOTH_COAP_HOST_URI='"coaps://coap.golioth.dev"' \
  -DSB_CONFIG_PERIPHERAL_ZEPHYR=y \
  -Dperipheral_ble_gatt_example_0_CONFIG_POUCH_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"'
west flash
```

### native_sim with USB Bluetooth dongle

It is possible to communicate with real (physical) Bluetooth devices,
while still maintaining almost all benefits of `native_sim` platform,
such as development speed, host debugging capabilities, infinite
resources, etc. This is achieved by Bluetooth HCI (Host Controller
Interface), which can be implemented on various transport layers (UART,
SPI, USB, Linux kernel HCI, ...).

When building `native_sim` with Bluetooth support on Linux both UART and
Linux kernel HCI transports are supported. Both work almost the same,
since they operate on stream of data. See [native_sim in
Zephyr](https://docs.zephyrproject.org/4.1.0/boards/native/native_sim/doc/index.html)
for details.

Recommended setup for testing is [nRF52840
Dongle](https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dongle/doc/index.html)
with Bluetooth controller available through HCI UART. The main advantage
is that controller will run the same firmware (Zephyr HCI UART) as
gateway running on physical devices like nRF9160-DK (there is even the
same nRF52840 Bluetooth Controller chip) or Thingy:91 X (which contains
slightly more powerful nRF5340 as Bluetooth Controller).

Build Bluetooth Controller firmware with HCI UART over USB CDC-ACM for
nRF52840 Dongle:

```
west build -p -b nrf52840dongle/nrf52840 bluetooth-gateway/controller
```

See [Programming and
Debugging](https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dongle/doc/index.html#programming-and-debugging)
page on how to flash that on the dongle.

After connecting this dongle to host PC it is not required to connect
that to `native_sim`. First setup a proxy using `socat` so it will be
possible to access HCI UART data stream over TCP (port 12345 is used as
example):

```
while :; do socat -x -dd /dev/serial/by-id/usb-ZEPHYR_Zephyr_HCI_UART_sample_98AE46C3F73B765A-if00,rawer,b115200 tcp-listen:12345,reuseaddr; done
```

then run `native_sim` with following command:

```
build/zephyr/zephyr.exe -bt-dev=127.0.0.1:12345
```

At this stage `native_sim` will connect to cloud using native networking
(via Native Simulator Offloaded Sockets) and still be able to find local
Bluetooth devices, communicate with them and send requested pouches.

## Useful options for debugging and development

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
