# LOGITacker v0.1.6-beta

- experimental G700/G700s receiver support (`options global workmode g700`), Note: The mode is basically Unifying 
compatible but required for the `pair device run` command (different pairing parameters). Additionally, keystroke 
injection for G700 receivers is ALWAYS UNENCRYPTED.
- (feature disabled, not reliable on windows) up to 4-times faster injection for unifying
- reduced debug output during injection
- USB injection works immediately on operating systems which send an USB keyboard LED report to newly attached
devices (Windows/Linux). In this mode, no initial delay is needed for USB injection scripts. The behavior could be
enabled with `options global usbtrigger ledupdate`
- fix: no delay between HID reports in USB injection mode (about 9-times faster typing)
- fix: `pair sniff run` uses channel map according to working mode (Unifying / Lightspeed / G700)

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
