# LOGITacker v0.2.2-beta

**For updates from older LOGITacker versions the command `erase_flash` has to be ran once, to re-initialize
the flash data storage for the changed data structures. Not doing so likely causes errors during LOGITacker 
operation**

- fix: malformed keyboard reports when USB injection is used, because Logitech checksum was injected
- added Danish keyboard layout `da`
- added python companion script to program, which creates a script with Danish unicode character on LOGITacker 
(programming using USB HID interface)

# LOGITacker v0.2.1-beta

**For updates from older LOGITacker versions the command `erase_flash` has to be ran once, to re-initialize
the flash data storage for the changed data structures. Not doing so likely causes errors during LOGITacker 
operation**

- fix: critical issue with incomplete script storage on flash, due to unintended behavior of `fds_record_find`
see https://devzone.nordicsemi.com/f/nordic-q-a/52297/fds-read-order-fds_record_find-doesn-t-reflect-write-order-fds_record_write-for-multiple-records-with-same-record_key-and-file_id-if-virtual-page-boundaries-are-crossed
- started working on command interface via raw HID (work in progress)
    - COMMAND_SCRIPT_STRING = 0x10
    - COMMAND_SCRIPT_ALTSTRING = 0x11
    - COMMAND_SCRIPT_PRESS = 0x12
    - COMMAND_SCRIPT_DELAY = 0x13
    - COMMAND_SCRIPT_CLEAR = 0x14
    - example (not generic ... directly writing to `/dev/rawhid0`) in `companion2.py`
- some changes to logging for Flash Data Storage operations
- increased log buffers size, to account for dropped messages    
- known issue: If storing a script to flash fails in the middle, because there's no remaining space, partially
written data of the script isn't removed from flash.

# LOGITacker v0.2.0-beta

- covert channel demo implementation (Windows only; tested on Windows 7 - 32bit, Windows 10 - 64bit)
    - `covert_channel deploy <device address>` for client agent deployment
    - `covert_channel run <device address>` to access the remote shell of a target with deployed cover channel agent
    - added covert channel support for G900 receiver (deployment is 8 times faster than Unifying, encrypted)     
    - added covert channel support for G700 receiver (deployment is 8 times faster than Unifying, unencrypted)     
- experimental G700/G700s receiver support (`options global workmode g700`), Note: The mode is basically Unifying 
compatible but required for the `pair device run` command (different pairing parameters). Additionally, keystroke 
injection for G700 receivers is ALWAYS UNENCRYPTED. For covert channel usage with G700, this mode has to be enabled, too.
- removed unpublished speed up for 4-times faster injection on Unifying (doesn't work reliable on all targets)
- reduced debug output during injection
- USB injection works immediately on operating systems which send an USB keyboard LED report to newly attached
devices (Windows/Linux). In this mode, no initial delay is required for USB injection scripts. The behavior could be
enabled with `options global usbtrigger ledupdate`
- fix: no delay between HID reports in USB injection mode (about 9-times faster typing)
- fix: `pair sniff run` uses channel map according to working mode (Unifying / Lightspeed / G700)
- known bug: If USB injection is used with "LED update trigger", attaching an additional device to the USB host could
trigger the injection payload, again, if LOGITacker is still connected (USB SOF event).

# LOGITacker v0.1.6-beta

- not released

# LOGITacker v0.1.5-beta

- experimental USB injection support with `inject target USB`
- introduction of `options global bootmode` to toggle between USB injection and default behavior
- script used for USB injection on boot is set with `options inject default-script <scriptname>`, the respective script
has to be stored with the proper name using `script store <scriptname>`
- fix: script storage, scriptname buffer not trimmed down to new length if new scriptname of successive storage attempts 
gets shorter
- fix: issue #8 (typos for passive enum)
- Note: As the update changes the structure for persistent options `erase_flash` has to be executed once after update

# LOGITacker v0.1.4-beta

- experimental Logitech LIGHTSPEED support (G-Series, tested with G603)
- adjusted device listing with `devices` command (prints more data, especially for devices obtained from sniffed pairing)
- fix: for discovered devices which have already been stored to flash, dongle data is re-loaded along with device data 
(dongle WPID, classification)
- fix: plain keys are printed in passive-enum mode (if AES key known), even if they aren't at first position in the report

## Update instructions

- follow install instructions from readme
- after flashing the new image, run `erase_flash` once and re-plug the LOGITacker dongle (all stored data will be erased
and new default options deployed)

## LIGHTSPEED mode

Lightspeed mode could be enabled with `options global workmode lightspeed` and disabled with 
`options global workmode unifying`. To persist the changes run `options store`.

Lightspeed mode is not compatible to legacy Unifying mode (different channels, different encryption scheme
for injection). In case you changed the mode, be sure to restart the current task (f.e. `discover run` to
restart device discovery in the new mode).

In order to obtain a link encryption key for a LIGHTSPEED device, use the `munifying` tool as described in the 
README (the latest version added support for G-Series LIGHTSPEED devices) 

# LOGITacker v0.1.1-beta

- first public release
