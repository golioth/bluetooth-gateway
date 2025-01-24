from twister_harness.device.device_adapter import DeviceAdapter


def test_boot(dut: DeviceAdapter):
    dut.readlines_until('Booting Zephyr')


def test_device_found(dut: DeviceAdapter):
    dut.readlines_until('Device found')
