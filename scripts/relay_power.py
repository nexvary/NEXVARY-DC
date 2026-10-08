"""Bounded DCTTech USBRelay power cycle (16c0:05df, USBRelay2/4/8).
Wire only source +5V/+12V through isolated NO contacts, never host/destination.
Protocol reference: OSC stable v2.5.0 usbrelay.c and usbrelay.h (NOTICE.md).
"""
import time


class RelayPower:
    def __init__(self, device, relay_id, rails, max_cycles=1, off_seconds=2, settle_seconds=5, sleep=time.sleep):
        if not isinstance(relay_id,str) or len(relay_id) != 5 or not relay_id.isascii():
            raise ValueError('Expected five-character hardware relay ID')
        if not isinstance(rails,dict) or set(rails) != {'5v','12v'} or len(set(rails.values())) != 2 or any(type(c) is not int or not 1 <= c <= 8 for c in rails.values()):
            raise ValueError('Two distinct source-only +5V/+12V relay channels required')
        if type(max_cycles) is not int or not 1 <= max_cycles <= 3 or not 2 <= off_seconds <= 10 or not 3 <= settle_seconds <= 30:
            raise ValueError('Power cycle settings exceed safe bounds')
        self.device, self.relay_id, self.rails = device, relay_id, rails
        self.max_cycles, self.cycles = max_cycles, 0
        self.off_seconds, self.settle_seconds, self.sleep = off_seconds, settle_seconds, sleep
        state = self.state()
        if any(not state & (1 << (channel-1)) for channel in rails.values()):
            raise ValueError('Source channels must already be on before rescue')

    def state(self):
        report = bytes(self.device.ctrl_transfer(0xa0,1,0x300,0,8,timeout=2000))
        if len(report) != 8 or report[:5].decode('ascii') != self.relay_id:
            raise ValueError('Relay identity changed or invalid feature report')
        return report[7]

    def switch(self, channel, on):
        self.state()  # identity revalidated immediately before every write
        payload = bytes([0xff if on else 0xfd,channel,0,0,0,0,0,0])
        if self.device.ctrl_transfer(0x20,9,0x300,0,payload,timeout=2000) != 8:
            raise OSError('Short relay write')
        if bool(self.state() & (1 << (channel-1))) != on:
            raise OSError('Relay state did not reach requested value')

    def cycle(self):
        if self.cycles >= self.max_cycles:
            raise OSError('Power-cycle budget exhausted')
        before = self.state()
        if any(not before & (1 << (c-1)) for c in self.rails.values()):
            raise ValueError('Unexpected relay state; power cycle refused')
        self.cycles += 1
        try:
            for channel in self.rails.values(): self.switch(channel,False)
            self.sleep(self.off_seconds)
        finally:
            # Attempt both rails even if a USB transfer failed. Report any failure,
            # never silently continue reading after uncertain power restoration.
            errors = []
            for channel in self.rails.values():
                try: self.switch(channel,True)
                except Exception as exc: errors.append(str(exc))
            if errors: raise RuntimeError('Power restoration failed: '+'; '.join(errors))
        if self.state() != before: raise RuntimeError('Unrelated relay state changed')
        self.sleep(self.settle_seconds)

    @classmethod
    def connect(cls, config):
        if config.get('sourceOnlyWiringConfirmed') is not True:
            raise ValueError('Confirm isolated source-only +5V/+12V wiring in relay config')
        import usb.core
        import usb.util
        matches = []
        for device in usb.core.find(find_all=True,idVendor=0x16c0,idProduct=0x05df):
            product = usb.util.get_string(device,device.iProduct)
            if product not in ('USBRelay2','USBRelay4','USBRelay8'): continue
            report = bytes(device.ctrl_transfer(0xa0,1,0x300,0,8,timeout=2000))
            if len(report) == 8 and report[:5].decode('ascii') == config['id']:
                if max(config['rails'].values()) > int(product[-1]): raise ValueError('Channel exceeds relay capacity')
                matches.append(device)
        if len(matches) != 1: raise ValueError('Relay not found or duplicate hardware ID')
        return cls(matches[0],config['id'],config['rails'],config.get('maxCycles',1),
                   config.get('offSeconds',2),config.get('settleSeconds',5))
