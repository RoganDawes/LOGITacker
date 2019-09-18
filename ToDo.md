# Initial note

The firmware evolves as needed by myself (on-stage demos, experiments). **No feature requests, PRs welcome**

# development / features
- adaptive timeout on passive enum (keep alive / set keep alive adoption)
- more language layouts (currently DE and US)
- [done] key `press` method for injection (handling shortcuts)
- [done] flash storage of device data
- auto delete devices from list (not spotted for a while / dongle not reachable in active enum)
- [partially, `options` command] automation: active enum --> passive enum --> discover (default)
- abort conditions for passive enum (too many non-Unifying frames, no frames after timeout, enough frames of interesting types received, interesting != keep-alive)
- [done for keyboard] mirror input reports to HID keyboard/mouse interface
- [done] pass sniffed frames on HID raw
- [done] passive enum keyboard reports to string: modifiers have to be added
- [done] cli: auto complete for `devices store load` and `devices store delete` (maybe option to store full dongle)
- flash storage automation: optional auto store for
    1) [done] devices with sniffed pairing and encryption capability (chances are low to capture pairing in flight --> must have)
    2) [done] devices which respond to plain keystroke injection during active enum
    3) [done] devices for which plain key reports have been captured during passive enum (f.e. R400 doesn't reveal injection vuln during active enum, but send plain keyboard reports)
- [done, fixed issue] extensive testing of device flash storage, definition of upper limits (nobody needs 1000 devices on the dongle, as they could be barely handled interactively)
- maybe: Introduce user provided meta data, like "site name" for discover and custom device name, to make it easier to re-identify specific devices stored on flash
(raw RF addresses require noting down additional info) - this is low prio, as it requires additional relationships for data stored on flash, which means runtime-reference-creation
and thus brings all nice errors of pointer arithmetic (missing Golang here)
- account for re-transmits from legit device (analyze PID of ESB PCF ... for passive enum, pair sniffing) - **must have for on-stage demos**
- [partially] BSP: proper LED driving and button interaction for various modes
- [done, `script`command] injection: allow entering inject mode without given RF address (block execution unless target RF address is set, instead)
- [done] storing scripts
- [done] loading scripts
- [done] script FDS: deletion, list stored scripts
- injection: abort command for long typing scripts (work around for now `discover run`)
- [done] command for manual device creation (avoid the need of discover, allow adding keys e.g. R500 presentation clicker)
- [done] command for flash erase
- [done] remove test commands
- emulation of dongle in pairing mode
- [done, without triggers] utilizing scripts for "classical" USB injection instead of RF (bonus: trigger from RF, f.e. presentation clicker)
- check options to ship stored FDS data with hex image for firmware (pre-built scripts, down&exec demo as default)
- remove either key or raw key data from device struct, to save space (one could be derived from the other, as "key generation" is no one-way function)
- [done] rework `options show`
- implement `options pair-sniff pass-through-raw`
- [done] remove unneeded modules in root folder (refactoring)
- [done] add GCC build scripts for MDK and MDK dongle
- RF based control of USB injection mode with Logitech devices (f.e. presentation clicker to iterate over payloads and
execute on-demand --> use PRX mode)

# bugs / issues

- if storing a script to flash fails in the middle, because there's no remaining space, partially written data of the 
script isn't removed from flash. Logic has to be added, to delete already written flash recors in error case.

# further analysis
- capabilities to send in new HID++ messages (maybe re-write device capabilities)
- [dead end, newer fw sets a bit if pairing address is enabled, only this allows response for pairing phase 2] forced pairing on "patched" dongles
- [not doable] check if there's a way to use a "sub-shell" in nrf_cli to emulate an interactive shell for the Unifying backdoor
- firmware attacks via keystroke injection (payload to transmit keys on RF after USB extraction)

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
