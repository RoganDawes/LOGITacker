# Passive host OS fingerprinting from an USB device during enumeration (Summary by MaMe82)

USB based host OS fingerprinting has been a long requested feature (f.e. for P4wnP1 and P4wnP1 ALOA).
I was involved in various discussions on the topic, mainly on Twitter.

Recently, a tweet from [@AndreaBarisani](https://twitter.com/AndreaBarisani/status/1271444260412444673) evolved to an 
[open discussion](https://twitter.com/AndreaBarisani/status/1272603059097657345) thread and revived the topic.
Andrea was able to come up with some information on Operating-System-sepcific differences during USB device enumeration,
as a side product of his [TamaGo](https://github.com/f-secure-foundry/tamago) development 
([for his results, see here](https://gist.github.com/abarisani/4595a7c535435038e0571237893c81c4)).

Starting from Andrea's data, the idea of a generic approach to distinguish different Operating Systems was developed
(once more discussed on Twitter).

For me the fasted way to implement a prototype was to add a branch to LOGITacker covering this feature.

The goals:

1) Find a way to generate a fingerprint (based on events seen by an USB device) which is
    - sized as small as possible
    - still allows to distinguish as many operating systems as possible (even future ones)
2) Develop logic, which makes a educated guess on the OS used by the USB host, based on the created fingerprint
3) Collect community contributed fingerprints, to refine and extend the "OS guessing" logic

# The approach

I try to explain this as short as possible, without writing down the whole USB specs again.

So before an USB device could be used by the host, its capabilities have to be checked to load the correct drivers
and set it up to work with the host. In order to get a picture on a device's capabilitites, the host has to "ask"
the device about them. This is done with so called "requests", which are answered with so called USB descriptors
(they describe capabilities, right). There are different kinds of descriptors, for different logical parts
of an USB device (f.e. the device itself, configurations, interfaces, endpoints).

Before the host is able to communicate with the device, it has to request some information, to be able to exchange
data, at all. At this point in time, the device isn't even configured to work as intended, it is more or less running
in an unconfigured default mode and thus is only able to answer standard requests from the host.

Those exact standard requests are used, to retrieve the minimal information which is required to get the device configured
and running. Ultimately, each USB host has to request those minimal information (expressed as descriptors). After his has
happened the device is configured in a way which is driven by the host and depends on available configurations (working 
modes) of the device. 

In short words:
- How the initial descriptors are requested mainly depends on the host OS, but is more or less device independent
- How things go on after the device is configured, largely depends on the device and its capabilities itself
and could totally differ between two USB hosts, even if they run the same OS.

So we are interested in the phase, where basic device information is retrieved (before a working configuration is 
selected and the device is used).

Even if all USB hosts have to request the same data (descriptors) from the device, there are per OS differences on how
this is done, because the USB specifications leave enough freedom for unique vendor implementations of different USB
host stacks. 
There are two major differences between various USB host implementations, which could help to distinguish them:

1. The order in which device capabilities are requested by the host (order of device descriptors, configuration 
descriptors, string descriptors and device qualifiers - the latter are used on USB High Speed only)
2. The allowed length for a descriptor in the very first requests (the same descriptor could be requested multiple times
by the host. The first request mostly is meant to fetch a partial descriptor, along with the information on its actual 
size. After the host has prepared a receive buffer of sufficient size, it could request the full descriptor.)

This also implies, that the actual content of the descriptors is out of interested, we are focused on how and when they 
are requested. Also we are only interested in a few descriptor requests from the very initial device enumeration phase,
not from the configuration phase.

In USB terms, we need to track:

- setup requests 
- if they are directed from host to device (bmRequestType 0x80)
- if they request a device descriptor (bRequest 0x06)
- only if they are directed to the "device" (not interface/endpoint/other - again, bmRequestType 0x80)

As we only track these kinds of "GET DESCRIPTOR" requests, there is no need to even store the bmRequestType or bRequest.
Neither do we have to store timestamps their occurrence, as we are only interested in the order of occurrence.
So we just store the remaining descriptor request data in first-in-first-out fashion.

What is the remaining data to track for those "GET DESCRIPTOR" requests?

1) The descriptor type requested (encoded in high byte of wValue of the respective setup request). The requested 
descriptor type could be:
    - Device (0x01)
    - Configuration (0x02)
    - String (0x03)
    - Device Qualifier (0x06, only for High Speed devices)
2) The descriptor index requested (encoded in low byte of wValue of the respective setup request). The index is used
to select the descriptor descriptor, if there exists more than one of the same type (f.e. string descriptors).
3) The language ID, which is set to ZERO if no specific descriptor language is requested. This is mainly used on
string descriptors in our scope. The actual language ID value is encoded in the 16bit wIndex field of the setup request.
We only store the lower byte of this 16bit ID, as we are only want to know if a specific language is requested or not (
not which exact language, as it would be mostly 0x0409 for English
4) The most important value is the requested descriptor length, which is encoded in the 16bit wLength value of the setup 
request. Again we only store the LSB of the length, as the descriptors in scope usually are not requested with a length 
value greater than 255.


Ultimately we end up with a table, representing GET_DESCRIPTOR requests to the device, which is ordered and consumes 4 
byte per entry. 

In addition a table entry is generated, if the host resets the device after a single request to the device 
descriptor. This is common host behavior, the goal of this (sometimes partial) read of the device descriptor is needed
to determine the maximum allowed packet size for lower protocol layers. The device gets reset afterwards, to avoid 
misbehavior caused by a possibly incomplete device descriptor read. A reset table entry if represented by 
[0xff 0xff 0xff 0xff] to preserve the 4-byte-alignment.

# Dissecting a fingerprint generated by LOGITacker

A fingerprint generated by LOGITacker while being attached to a Linux host looks like this:

```
LOGITacker (discover) $ finger_usb 
<info> LOGITACKER_USB: USB fingerprint
<info> LOGITACKER_USB:  Each GET DESCRIPTOR request is represented with 4 byte [type, idx, lang_id low byte, length low byte]
<info> LOGITACKER_USB:  Relevant types: 0x01 device, 0x02 config, 0x03 string, 0x06 dev qualifier (high speed only)
<info> LOGITACKER_USB:  'FF FF FF FF' marks a device reset after the initial request for the device descriptor
<info> LOGITACKER_USB:  01 00 00 40 FF FF FF FF|...@....
<info> LOGITACKER_USB:  01 00 00 12 06 00 00 0A|........
<info> LOGITACKER_USB:  06 00 00 0A 06 00 00 0A|........
<info> LOGITACKER_USB:  02 00 00 09 02 00 00 96|........
<info> LOGITACKER_USB:  03 00 00 FF 03 01 09 FF|........
<info> LOGITACKER_USB:  03 01 09 FF 03 03 09 FF|........
<info> LOGITACKER_USB:  03 04 09 FF 03 03 09 FF|........
<info> LOGITACKER_USB:  03 03 09 FF 03 03 09 FF|........
<info> LOGITACKER_USB:  00 00 00 00 00 00 00 00|........
<info> LOGITACKER_USB:  00 00 00 00 00 00 00 00|........
<info> LOGITACKER_USB: Guessed OS: Linux
```

The current fingerprint table of the LOGITacker implementation is 80 bytes in size and thus stores up to 20 entries
(4 bytes each). The table is reverted, if a device reset is issued by the host **unless there was only a single table 
entry for a device descriptor request**. In the latter case, this entry is preserved and a FF FF FF FF entry gets 
appended to the table (marks a reset, after an initial device descriptor read).

The table translates to the following GET_DESCRIPTOR requests in order of occurrence:

```
01 00 00 40     // 0x01 - request device descriptor, read length 0x40 bytes 
FF FF FF FF     // device reset
01 00 00 12     // 0x01 - 2nd read of device descriptor, read length 0x12 bytes (18 bytes == default device descriptor size)
06 00 00 0A     // 0x06 - request device qualifier, 10 bytes
06 00 00 0A     // 0x06 - request device qualifier, 10 bytes
06 00 00 0A     // 0x06 - request device qualifier, 10 bytes
02 00 00 09     // 0x02 - request configuration descriptor, 9 bytes (up to the value which contains the configuration total length)
02 00 00 96     // 0x06 - request configuration descriptor and successive descriptors (up to total length, which is device specific)
03 00 00 FF     // 0x03 - request string descriptor, string index 0x00, no language ID, read length 255
03 01 09 FF     // 0x03 - request string descriptor, string index 0x01, language ID 0x0409 (only 0x09 stored), read length 255
03 01 09 FF     // 0x03 - request string descriptor, string index 0x01, language ID 0x0409 (only 0x09 stored), read length 255
03 03 09 FF     // 0x03 - request string descriptor, string index 0x03, language ID 0x0409 (only 0x09 stored), read length 255
03 04 09 FF     // 0x03 - request string descriptor, string index 0x04, language ID 0x0409 (only 0x09 stored), read length 255 
03 03 09 FF     // 0x03 - request string descriptor, string index 0x03, language ID 0x0409 (only 0x09 stored), read length 255
03 03 09 FF     // 0x03 - request string descriptor, string index 0x03, language ID 0x0409 (only 0x09 stored), read length 255
03 03 09 FF     // 0x03 - request string descriptor, string index 0x03, language ID 0x0409 (only 0x09 stored), read length 255
00 00 00 00     // empty table entry (no event logged) 
00 00 00 00     // empty table entry (no event logged) 
00 00 00 00     // empty table entry (no event logged) 
00 00 00 00     // empty table entry (no event logged) 
```

In summary, this kind of fingerprint is a generic and not too memory consuming way, to store represent and store the
behavior of an USB host during early device enumeration phase.

It provides enough information to recognize specifics of different host implementations. A hexadecimal string 
representation allows easy comparison of different fingerprints, collection in public databases or regex based analysis.

The size of 80 bytes has be chosen based on initial tests.

# Derive the OS from the fingerprint

As already mentioned, Andrea Barisani tracked down some [OS specific differences while the host requests descriptors](https://gist.github.com/abarisani/4595a7c535435038e0571237893c81c4)

To already some up a few:

- only on macOS the first device descriptor request is issued with a length of 18 (which is the actual size of an device 
descriptor)
- For Windows and Linux the first device descriptor request is send with a request length of 64 bytes, while the 2nd device
descriptor request fetches the real size (18 bytes)
- still Windows and Linux could be distinguished, based on the first occurring configuration descriptor request. On Linux 
the respective request length is 9 bytes, while 255 bytes are requested by Windows

Up to this point, only information published by A. Barisani has been used. Obviously it is crucial to collect more 
fingerprints, to find more reliable indicators - which could help to clealy identify a USB host implementation.

For example community feedback from @SymbianSyMoh helped to work out the following fact:

- On a system running "Parallels" device enumeration is always done by the host OS (not guest OS). Parallels is
unique, because the very first device descriptor request has a length of 8 bytes (not 18 or 64 bytes).

From my own tests, it seems that Android 10 behaves exactly like Debian, still there was a minor difference which
allowed to distinguish it:
- on Linux all string descriptors are requested with a length of 255 bytes
- on Android everything is the same, but there are additional requests for the string at index 0x00 without language ID,
**but length of 254 bytes** (which does not happen on Linux)

# Summary

The fingerprinting technique seems to be sufficient to distinguish USB host implementations of major operating
systems. More fingerprints on from various OS's could helpt to improve the "OS guessing" logic.

It is also worth mentioning, that the string descriptor request seem to occur in random order for the same OS,
while order of device and configuration descriptor requests seems to be strict. Still the string descriptor request
length could give useful hints (see Android detection)

- 1st goal (building a usable fingerprint): achieved
- 2nd goal (derive OS from fingerprint): achieved for major OS's
- 3rd goal (fingerprints contributed by community): t.b.d.