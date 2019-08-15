# LOGITacker v0.1.3-beta

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
