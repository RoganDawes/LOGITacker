# development
- adaptive timeout on passive enum (keep alive / set keep alive adoption)
- more language layouts
- [done] key `press` method for injection (handling shortcuts)
- [done] flash storage of device data
- auto delete devices from list (not spotted for a while / dongle not reachable in active enum)
- automation: active enum --> passive enum --> discovery (default)
- abort conditions for passive enum (too many non-Unifying frames, no frames after timeout, enough frames of interesting types received, interesting != keep-alive)
- [done for keyboard] mirror input reports to HID keyboard/mouse interface
- pass sniffed frames on HID raw
- passive enum keyboard reports to string: modifiers have to be added, change logging to use single line
- [done] cli: auto complete for `devices store load` and `devices store delete` (maybe option to store full dongle)
- flash storage automation: optional auto store for
    1) [done] devices with sniffed pairing and encryption capability (chances are low to capture pairing in flight --> must have)
    2) [done] devices which respond to plain keystroke injection during active enum
    3) [done] devices for which plain key reports have been captured during passive enum (f.e. R400 doesn't reveal injection vuln during active enum, but send plain keyboard reports)
- extensive testing of device flash storage, definition of upper limits (nobody needs 1000 devices on the dongle, as they could be barely handled interactively)
- maybe: Introduce user provided meta data, like "site name" for discovery and custom device name, to make it easier to re-identify specific devices stored on flash
(raw RF addresses require noting down additional info) - this is low prio, as it requires additional relationships for data stored on flash, which means runtime-reference-creation
and thus brings all nice errors of pointer arithmetics (missing Golang here)
- account for re-transmits (passive enum, pair sniffing) - **must have for on-stage demos**
- BSP: proper LED driving and button interaction for various modes
- injection: allow entering inject mode without given RF address (block execution unless target RF address is set, instead)
- [done] storing scripts
- [done] loading scripts
- script FDS: deletion, list stored scripts
- injection: abort command for long typing scripts

# Analysis
- capabilities to send in new HID++ messages (maybe re-write device capabilities)
- [dead end, newer fw sets a bit if pairing address is enabled, only this allows response for pairing phase 2] forced pairing on "patched" dongles
- check if there's a way to use a "sub-shell" in nrf_cli to emulate an interactive shell for the Unifying backdoor


# excerpt features / general notes
- done: UTF8 string parser
- done: UTF8 string to hid report translator
- done: hid report to RF frame translator (only plain, encrypted is ToDo)
- done: flash storage of global options
- done: pairing emulation (usable for forced pairing), Note: active enum "forced pairing" test returns false positives, because it opts out too early (has to test phase 2)
- done: pair sniffing and proper device creation
- done: flash device storage (and dongle storage with m:n data relation)
- done: UTF8 keyboard combo string to HID report translator (`press` command)
- done: refactor all "sub state machines" to logitacker_processor interface (discover, pair_sniff, pair_device, active_ennum, passive_enum, inject)
- done: build custom ringbuf based on nrf_ringbuf (no pointer based access to backing array, as this is problematic if
payload wraps around max length; peeking into buffer is needed as it should be used for script tasks)
