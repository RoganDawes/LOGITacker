# LOGITacker

LOGITacker is a hardware tool to enumerate and test vulnerabilities of Logitech Wireless Input devices via RF.
In contrast to available tooling, it is designed as stand-alone tool. This means not only the low level RF part, 
but also the application part is running on dedicated hardware, which could provides Command Line Interface (CLI)
via USB serial connection. 

Keeping hardware from other vendors (not Logitech) out of scope, allowed further optimizations and improvements for
low level stuff like RF device discovery.

LOGITacker is developed as firmware for a low cost **Nordic nRF52840 dongle**.

Support for the following additional boards is **planned**:
- MakerDiary MDK Dongle
- MakerDiary MDK

AprBrother BLE dongle (nRF52840) won't receive support, as I only received 1 out of 3 payed devices, when I ordered
from them.

# feature overview (excerpt)

- Discovery of Logitech devices on air (optimized pseudo promiscuous mode)
- Device management (store/delete devices from/to flash, auto re-load parameters - like link encryption key - from flash
 when a stored device is discovered again)
- Passive Enumeration of discovered devices (sniffing, automatic update of device capabilities based on received RF frames)
- Active Enumeration of discovered devices (test receiver for additional valid device addresses, test for plain keystroke
injection vulnerability)
- Injection (Inject keystrokes into vulnerable devices, inject encrypted for devices with known key, bypass alpha 
character filter of receivers for newer devices - like R500 presentation clicker)
- Scripting (create/store/delete injections scripts on internal flash, auto load a script on boot, auto inject a script
on device discovery, support for DE and US keyboard layout)
- CLI (integrated help, tab completion, inline help)  
- Sniff pairing (AES link encryption keys get stored to flash, encrypted injection and live decryption of keyboard traffic
gets possible)
- Device Pairing / forced pairing: a virtual device could be paired to a Unifying dongle in pairing mode or to an 
arbitrary RF address (in order to test for the forced pairing vulnerability presented by Marc Newlin in 2016) 
- Device management on flash: Discovered devices could be stored/deleted to/from flash persistently. This comes handy if 
a device has an AES link encryption key associated. If the dongle is power-cycled and the respective device is discovered 
again, the associated data (including the AES key) is restored from flash.
- Live decryption: In passive enumeration mode, encrypted keyboard RF frames are automatically decrypted if the link
Encryption key is known (could be added manually or obtained from sniffed pairing). This could be combined nicely with 
USB pass-thorugh modes.
- USB pass-through: An USB serial based CLI is not the best choice, when it comes to processing of raw or decrypted RF
data. To circumvent this, LOGITacker supports the following pass-through modes:
    - USB keyboard pass-through: If enabled, received RF keyboard frames are forwarded to LOGITacker's USB keyboard 
    interface. Key presses of the currently sniffed wireless keyboard are ultimately mirrored to the host which has 
    LOGITacker connected. For encrypted keyboards, the decrypted data is forwarded (in case the AES key is known)
    - USB mouse  pass-through: Same as keyboard pass-through, but for mouse reports.
    - RAW pass-through: Beside the USB serial, USB mouse and USB keyboard interface, LOGITacker provides a raw HID 
    interface. Passing keyboard reports directly via USB isn't always a good idea (f.e. if you sniff a keyboard and
    the user presses ALT+F4). In order to allow further processing raw incoming data could be forwarded to the USB host, 
    using the raw interface (data format includes: LOGITacker working mode, channel, RSSI, device address, raw payload). 
- Automation / stand alone use: Beside devices data and scripts, several options could be stored persistently to flash. 
Those option allow to control LOGITacker's behavior at boot time. An **example set of persistent options, which could be
used for headless auto-injection** (LOGITacker could be power supplied from a battery) would look like this:
    - boot in discovery mode (detect devices on air)
    - if a device is discovered, automatically enter injection mode
    - in injection mode, load a stored default script and execute the script with default language layout
    - if the injection succeeded, return to discovery mode
    - enter injection mode not more than 5-times per discovered device
    
There are still many ToDo's. The whole project is in **experimental state**.    

# Installation

`nRF Connect` software by Nordic provides a `Programmer` app, which could be used to flash the firmware from this repository
to a Nordic nRF52840 dongle. After flashing the firmware, the dongle provides 4 new interfaces (USB serial, USB mouse, 
USB keyboard and USB HID raw). The serial interface could be accessed using `PuTTY` or `screen` on Linux.

Reference: "Terminal Settings" section of nRF5 SDK documentation - https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v15.0.0/lib_cli.html#lib_cli_terminal_settings

# Video usage examples (Twitter)

Note: The videos have been created throughout development and command syntax has likely changed (and will change). 
Please use tab completion and CLI inline help. All examples are included in current firmware.

## Discover a device 

Video: https://twitter.com/mame82/status/1126038501185806336

Note: Discovery, especially of presentation clickers, has been improved since this test video.

## Sniff pairing and eavesdropping for an encrypted keyboard

Video: https://twitter.com/mame82/status/1128036281936642051

To sniff a pairing attempt use `pai sniff run`. Successfully devices (and keys) of successfully sniffed pairing are
stored to flash automatically. The `options pair-sniff` command subset could be used to alter this behavior.

The `options store` command could be used to persistently store options changed at runtime (new option survive reboot
after storing).

If not done automatically after successful pair-sniffing, the respective keyboard has to be sniffed using
`passive_enum AA:BB:CC:DD:EE:FF`, where `AA:BB:CC:DD:EE:FF` has to be replaced by the device address obtained during
pair sniff (available via tab complete).

In order to forward (decrypted) keyboard RF frames to the USB keyboard interface, like shown in the video, the 
respective option has to be enabled with `options passive-enum pass-through-keyboard on`.

## Inject keystrokes (encrypted device)

Video: https://twitter.com/mame82/status/1136992913714491392

The video utilizes "pair-sniffing" to obtain the device link encryption key. This, of course, isn't needed for unencrypted
devices (f.e. presentation clickers like R400, R700, R800).

The injection mode uses encrypted injection for devices with known AES key and falls back to plain injection if no
key is known or the device isn't encrypted.

In order to create, store and delete scripts use the `script` sub-command set (again inline help and tab complete).

In order to start a injection enter injection mode for a specific device with `inject target AA:BB:CC:DD:EE` where
`AA:BB:CC:DD:EE` has to be replaced by the respective device address (tab complete if already discovered).

To execute the injection, issue `inject execute`.

The behavior after injection depends on the setting of `options inject onsuccess` and `options inject onfail`.
If LOGITacker is configured to leave injection mode it has to be entered again, using `inject target AA:BB:CC:DD:EE`, in
order to do further injections.

The current injection script could be stored to flash with `script store "scriptname"`.
It is not allowed to overwrite stored scripts (same name). In order to do so, the respective script has to be deleted 
first, with `script remove "scriptname"`.

Stored scripts could be listed with `script list`.

The current script could be printed with `script show`.

To set a script as default script on boot (could be used for auto-injection), the following option has to be altered:
`options inject default-script "scriptname"`. To persist the new default-script option don't forget to run `options store`.

## Using `altstring` feature in scripts (enter characters using ALT+NUMPAD on Windows targets)

Video demo 1 (Mouse MX Anywhere 2S): https://twitter.com/mame82/status/1139671585042915329

Video demo 2 (encrypted presentation clicker R500): https://twitter.com/mame82/status/1143093313924452353


# CLI documentation

t.b.d.

use issues for questions
  
  