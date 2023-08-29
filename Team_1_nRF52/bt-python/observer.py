#!.venv/bin/python3
from concurrent.futures import ThreadPoolExecutor
from time import sleep
from typing import cast

from bluepy.btle import UUID, DefaultDelegate, ScanEntry, Scanner

from logger import Logger
from mqtt import MqttHandler
from config import device_name

log = Logger(__name__)

ScanData = list[tuple[int, str, str | None]]

envSensService = UUID(0x181a)
executor = ThreadPoolExecutor(2)


def readAdData(ad: ScanData):
    for adType, desc, data in ad:
        log.debug(f'{type(adType)}, {type(desc)}, {type(data)}')
        if adType == ScanEntry.SERVICE_DATA_16B and isinstance(data, str):
            if len(data) <= 4:
                continue

            serviceHex = bytes.fromhex(data[:4])
            dataHex = bytes.fromhex(data[4:])

            service = int.from_bytes(serviceHex, 'little')
            if envSensService == service:
                temp = int.from_bytes(dataHex, 'little')
                log.info(f'Temp {temp / 100}')

        else:
            log.info(f'{adType}, {data}')


class ScannerDelegate(DefaultDelegate):
    def __init__(self):
        super().__init__()

    def handleDiscovery(self, dev: ScanEntry, isNewDev, isNewData):
        if dev.getValueText(ScanEntry.COMPLETE_LOCAL_NAME) != device_name:
            return
        if isNewDev:
            log.info("Discovered device %s (%s)",
                     dev.getValueText(ScanEntry.COMPLETE_LOCAL_NAME), dev.addr)
        if isNewData:
            log.info("Received new data from device %s", dev.addr)
            if dev.getValueText(ScanEntry.COMPLETE_LOCAL_NAME) == device_name:
                executor.submit(readAdData, cast(ScanData, dev.getScanData()))


class Bluetooth:
    scanner = Scanner().withDelegate(ScannerDelegate())

    def process(self):
        self.scanner.scan()


if __name__ == "__main__":
    ble = Bluetooth()
    try:
        with MqttHandler() as mqtt:
            log.info("Start main")
            log.info(f"Looking for {device_name}")
            while not mqtt.exit:
                ble.process()
                sleep(1)
    finally:
        log.info("Start main")
        log.info(f"Looking for {device_name}")
        while True:
            ble.process()
            sleep(1)
