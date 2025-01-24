from typing import Generator

import pytest

from twister_harness.device.device_adapter import DeviceAdapter


def determine_scope(fixture_name, config):
    if dut_scope := config.getoption("--dut-scope", None):
        return dut_scope
    return 'function'


@pytest.fixture(scope=determine_scope)
def dut(request: pytest.FixtureRequest, device_object: DeviceAdapter) -> Generator[DeviceAdapter, None, None]:
    """Return launched device - with run application."""
    device_object.initialize_log_files(request.node.name)
    try:
        # Override direct 'zephyr.exe' execution with invocation of 'west flash -d application_dir',
        # which supports executing launching all domains (BabbleSim components).
        device_object.command = [device_object.west, 'flash', '-d', str(device_object.device_config.build_dir)]
        device_object.process_kwargs['cwd'] = str(device_object.device_config.build_dir)
        device_object.launch()
        yield device_object
    finally:  # to make sure we close all running processes execution
        device_object.close()
