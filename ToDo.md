# development
- adaptive timeout on passive enum (keep alive / set keep alive adoption)
- more language layouts
- key `press` method for injection (handling shortcuts
- flash storage of device data
- auto delete devices from list (not spotted for a while / dongle not reachable in active enum)
- automation: active enum --> passive enum --> discovery (default)
- abort conditions for passive enum (too many non-Unifying frames, no frames after timeout, enough frames of interesting types received, interesting != keep-alive)
- [done for keyboard] mirror input reports to HID keyboard/mouse interface
- pass sniffed frames on HID raw

# Analysis
- capabilities to send in new HID++ messages (maybe re-write device capabilities)
- [dead end, newer fw sets a bit if pairing address is enabled, only this allows response for pairing phase 2] forced pairing on "patched" dongles