# Golioth Bluetooth Gateway

## Setup

```
west init -m https://github.com/golioth/bluetooth-gateway.git
west update
west patch apply
```

## Build and run

### nrf52_bsim with sysbuild

Sysbuild is a generic multi-component build (and flash) infrastructure
in Zephyr. In context of `nrf52_bsim` it is used to build several
BabbleSim components. The main component is gateway. Additional sysbuild
components are BabbleSim programs:

  * 2.4G PHY implementation (coordinator of the whole simulation)
  * Bluetooth peripherals: either generic samples from
    `zephyr/bluetooth/peripheral*` or toothfairy nodes like
    `pouch/examples/toothfairy`)
  * (optional) BabbleSim handbrake

Default sysbuild configuration builds following:

  * `bluetooth-gateway/gateway` (main component)
  * `pouch/examples/zephyr/toothfairy` (toothfairy node)
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

Default example of running gateway with toothfairy node (and required
BabbleSim components) is done with following commands:

```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -Dperipheral_toothfairy_example_0_CONFIG_POUCH_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"' \
  -DEXTRA_CONF_FILE=$(west topdir)/gw-001.conf
west flash
```

where credentials are located in top west workspace directory in
`gw-001.conf`:

```
CONFIG_GOLIOTH_COAP_HOST_URI="coaps://coap.golioth.dev"
CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS=n
CONFIG_GOLIOTH_SAMPLE_PSK_ID="my-psk-id@my-project"
CONFIG_GOLIOTH_SAMPLE_PSK="my-psk"
```

It is possible to include more toothfairy nodes in simulation. Example of
running 2 nodes:
```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -DSB_CONFIG_PERIPHERAL_TOOTHFAIRY_EXAMPLE_NUM=2 \
  -Dperipheral_toothfairy_example_0_CONFIG_POUCH_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"' \
  -Dperipheral_toothfairy_example_1_CONFIG_POUCH_DEVICE_ID='"bbbbbbbbbbbbbbbbbbbbbbbb"' \
  -DEXTRA_CONF_FILE=$(west topdir)/gw-001.conf
west flash
```

It is even possible to include other non-toothfairy nodes. So far there
is support for vanilla Zephyr peripheral sample
`zephyr/samples/bluetooth/peripheral`:

```
west build -p -b nrf52_bsim bluetooth-gateway/gateway --sysbuild -- \
  -DSB_CONFIG_PERIPHERAL_ZEPHYR=y
  -Dperipheral_toothfairy_example_0_CONFIG_POUCH_DEVICE_ID='"aaaaaaaaaaaaaaaaaaaaaaaa"' \
  -DEXTRA_CONF_FILE=$(west topdir)/gw-001.conf
west flash
```

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
