# Pouch Gateway application

This application is using Pouch Gateway library and implements default
logic around Bluetooth scanning, device selection and connection
management.

During scanning Bluetooth peripherals need to follow specific criteria
in order to initiate connection to them:
- advertise Pouch Service UUID (0xFC49 or
  89a316ae-89b7-4ef6-b1d3-5c9a6e27d272 for backward compatibility) with
  compatible version and "sync request" flag set

Bluetooth connection is maintained just for the time Pouch
synchronizatio takes place:
- scan
- connect
- Pouch sync
- disconnect

## Building and flashing

The example should be built with west:

```bash
$ west build -b <board> gateway
$ west flash
```

## Provisioning

```sh
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:-$ kernel reboot
```
