# development
- adaptive timeout on passive enum (keep alive / set keep alive adoption)
- more language layouts
- key `press` method for injection (handling shortcuts
- flash storage of device data
- auto delete devices from list (not spotted for a while / dongle not reachable in active enum)
- automation: active enum --> passive enum --> discovery (default)
- abort conditions for passive enum (too many non-Unifying frames, no frames after timeout, enough frames of interesting types received, interesting != keep-alive)
- mirror input reports to HID keyboard/mouse interface

# Analysis
- capabilities to send in new HID++ messages (maybe re-write device capabilities)
- forced pairing on "patched" dongles