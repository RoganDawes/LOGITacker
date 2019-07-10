# Summary / Overview of known Logitech wireless peripheral vulnerabilities

There has been a ton of research on wireless input devices using proprietary 2.4 GHz radio technology.
Lately, I reported some new vulnerabilities for Logitech devices, myself, and noticed that one could easily get
confused by all those issues.

So here is an attempt to clarify some things. 

This document focuses on Logitech devices only, as other vendors haven't been part of my own research.

# General information

In context of attacks on Logitech devices, the device itself is often assumed to be vulnerable. Especially for
keystroke injection attacks, this is not the case. The vulnerable part is the wireless receiver. In fact, in order to 
carry out a keystroke injection attack, the actual input device does not even have to be in range. This is because the 
attacker is communicating with the receiver via RF while impersonating a real device. 

The actual device is only of interest for the questions: 
- Does the receiver allow keyboard input for the impersonated device or not (has the impersonated device keyboard 
capabilities, even if it is not a keyboard)?
- If the receiver accepts keyboard input, does it have to be encrypted (the attacker would need a valid encryption
key to impersonate the device)?

The answer to these questions are known by the receiver, because the data of paired devices is stored. If the receiver
behaves according to those answers, depends not only on the stored device data. It additionally, it depends on the patch
level of the respective receiver. This is because some vulnerabilities discovered by Bastille (Marc Newlin) back in 2016,
exploited misbehavior:

- An unpatched receiver could accept unencrypted keyboard input, even if the impersonated devices should only 
communicate encrypted (plain injection for encrypted devices)
- An unpatched receiver could be forced to register a new device, which always accepts plain keystrokes (forced pairing)

It should be noted, that an impersonated device does not have to be a wireless keyboard. Most mice, all presentation 
clickers and various other devices are able to emit keyboard input. Ultimately, the receiver would accept keystrokes
from an attacker impersonating such a device, f.e. a mouse. 

# Known vulnerabilities (partially undisclosed)

## 1) Plain keystroke injection / plain keystroke injection for encrypted devices

### Description

A remote attacker could inject arbitrary keystrokes into an affected receiver. In most cases (if no additional key 
filters are in place) this directly leads to remote code execution (RCE) and thus full compromise of the host with the 
receiver attached.

Most Logitech presentation clickers accept plain keystrokes (f.e. R400, R700, R800). The only thing needed by an 
attacker to impersonate the actual device is the RF address in use. This address could be discovered by monitoring
RF traffic (pseudo promiscuous mode as proposed by Travis Goodspeed or Software Defined Radio). Once the address is 
obtained, the attack could be directly carried out.

Additionally, older Unifying receiver firmwares accept unencrypted keyboard frames from impersonated devices, which 
should only send encrypted keyboard frames. Again, an attacker only needs to discover an RF address for such a device in
order to carry out the attack.

Those kinds of attack are known since 2016. Still today, Unifying receivers could be bought, which aren't patched 
against those attacks.   

### References

- unencrypted Logitech presentation clickers (reported by SySS GmbH)  
    - R400: https://www.syss.de/fileadmin/dokumente/Publikationen/Advisories/SYSS-2016-074.txt
    - R700: https://www.syss.de/fileadmin/dokumente/Publikationen/Advisories/SYSS-2019-015.txt
- plain injection for encrypted keyboards (reported by Bastille)
    - several devices https://github.com/BastilleResearch/mousejack/blob/master/doc/advisories/bastille-2.logitech.public.txt
    - G900 mouse: https://github.com/BastilleResearch/mousejack/blob/master/doc/advisories/bastille-12.logitech.public.txt
    

### Demos

- SySS GmbH, plain injection, Logitech R400: https://youtu.be/p32o_jRRL2w
- Bastille , Logitech Mouse: https://youtu.be/3NL2lEomB_Y
- MaMe82, Logitech R400: https://twitter.com/mame82/status/1126038501185806336

## 2) Encrypted keystroke injection without key knowledge (patched)

### Description

Wireless Logitech keyboards encrypt keystrokes before sending them to the receiver. A custom AES CTR implementation
is used to prevent an attacker from injecting arbitrary keystrokes. The implementation of Unifying receivers with 
outdated firmware have multiple issues:
- The receiver does not enforce incrementation of the AES CTR counter for successive RF frames. This allows replay 
attacks and *reuse of the counter with a modified cipher text*
- If the plaintext of an encrypted keyboard RF frame is known, an attacker could use this to recover the key material
used to encrypt the frame with this specific counter. Ultimately, the attacker is able to modify the respective RF
frame with new plaintext (other key presses). In combination with the ability to re-use the counter, the attacker could
inject arbitrary keystrokes.
- Encrypted key release frames are easy to identify while monitoring RF transmissions and could be used for a known
plaintext attack.  

Note: The issue exists for Unifying receivers not patched against the respective vulnerability, which was called
"KeyJack" by Bastille. So this is a good example for a vulnerability not directly depending on the device. 

### References

- encrypted injection (reported by Bastille)
    - https://github.com/BastilleResearch/keyjack/blob/master/doc/advisories/bastille-13.logitech.public.txt
- CVE-2016-10761 (https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2016-10761)  

## 3) Encrypted keystroke injection without key knowledge (no patch from vendor)

### Description

Logitech provided patches for the issue "encrypted keystroke injection without key knowledge":

- https://github.com/Logitech/fw_updates/blob/master/RQR12/RQR12.08/RQR12.08_B0030.txt
- https://github.com/Logitech/fw_updates/blob/master/RQR24/RQR24.06/RQR24.06_B0030.txt

Beside the fact, that not all dongles in market have the respective patches applied, they could be bypassed by an 
attacker. No patches exist for the extended version of the attack and Logitech confirmed, that no patch will be 
provided for this new vulnerability.

In contrast to the existing version of the attack, the new version requires that an attacker gets one-time physical
access to the wireless device, in order to enforce arbitrary key-presses. The goal of the attacker is to generate more
known plaintext, while capturing cryptographic data from RF. The collected cryptographic data than could be used, to carry out an 
attack similar to the one described above, but additionally bypass the AES counter re-use protection applied with the 
latest patches. **Physical access is only required one time. Once the data has been collected, arbitrary keystrokes 
could be injected, when and as often as the an attacker likes.**
 
A possible attacker only needs some seconds to generate the key-presses needed to break encryption (12 to 20 times
pressing the same key). 
 
*Note on presentation clickers: The goal of the physical key presses is to generate a sufficient amount of known 
plaintext (which could be derived from information leaks during RF transmissions by an attacker). In case of encrypted
presentation clickers, this step usually gets obsolete, because an attacker has other possibilities to get to know of
the plaintext for encrypted keyboard reports (Visual identification of pressed key, by watching the presentation 
controlled with the clicker. F.e. if next slide is shown, it is clear which plain key was pressed on the clicker before
encryption took place). Because of this fact, the attack could be simplified to a remote approach, only.* 

### References

- CVE-2019-13053
- vendor report (will be released soon)

### Demos

- Marcus Mengs
    - Demo of attack: https://twitter.com/mame82/status/1095272849705783296
    - Demo, usage to deploy a covert channel: https://twitter.com/mame82/status/1128392333165256706

## 4) Passively obtain Logitech Unifying link encryption keys by capture of pairing (RF only, no patch from vendor)

### Description

Weak key exchange and encryption allow an attacker to derive the per-device link-encryption keys, if the attacker is
able to capture a pairing between the device and receiver from RF. Additionally, an attacker with physical access to
device and receiver could manually initiate a re-pairing of an already paired device to the receiver, in order to obtain
the link-encryption key. There exists no possibility for the user, to notice that the respective key has been compromised.

Thus, beside being a passive remote attack (RF), the attack could be modified to a drive-by approach or supply chain
attack.

With the stolen key, the attacker is able to inject arbitrary keystrokes (active), as well as to eavesdrop and live
decrypt keyboard input remotely (passive). This applies to all encrypted Unifying devices with keyboard capabilities
(f.e. MX Anywhere 2S mouse).

Logitech confirmed, that no patch will be provided for this new vulnerability.

### References

- CVE-2019-13052
- vendor report (will be released soon)

### Demos

- Marcus Mengs
    - "K400+" keyboard, demo key sniffing + eavesdropping: https://youtu.be/GRJ7i2J_Y80
    - Keyboard eavesdropping, Demo 2: https://youtu.be/1UEc8K_vwJo
    - "MX Anywhere 2S" Mouse, demo key sniffing + encrypted injection: https://twitter.com/mame82/status/1139671585042915329

## 5) Actively obtain link encryption keys by dumping them from receiver of Unifying devices (physical access, patch will be supplied)

### Description

Due to undocumented vendor commands and improper data protection of some Logitech Unifying receivers, an attacker with
physical access could extract link encryption keys of all paired devices in less than a second.

Logitech is going to provide a patch for this issue in August 2019.  

With the stolen key, the attacker is able to inject arbitrary keystrokes (active), as well as to eavesdrop and live
decrypt keyboard input remotely (passive). This applies to all encrypted Unifying devices with keyboard capabilities
(f.e. MX Anywhere 2S mouse). Additionally there is no need to discover the device "on air" to carry out a keystroke
injection attack, as the address is pre-known from the extraction (targeted attack possible, actual device doesn't have 
to be in range - only the receiver) 

Logitech confirmed, that no patch will be provided for this new vulnerability in August 2019.

### References

- CVE-2019-13055
- vendor report (will be released once patch is available)

### Demos

- Marcus Mengs
    - "K360" keyboard, demo key extraction + eavesdropping: https://twitter.com/mame82/status/1101635558701436928

## 6) Actively obtain link encryption keys by dumping them from receiver of encrypted presentation clickers (physical access, patch will be supplied)

### Description

Due to undocumented vendor commands and improper data protection of some Logitech presentation clicker receivers, 
an attacker with physical access could extract link encryption keys of all paired devices in less than a second.

The exact same attack vector described in CVE-2019-13055 applies, thus this is assumed to be fixed along with the 
respective Unifying vulnerability in August 2019 (vendor has been informed on the issue, which is technically the same).
 
With the stolen key, the attacker is able to inject a subset of possible keystrokes (active). Additionally there is no 
need to discover the device "on air" to carry out a keystroke injection attack, as the address is pre-known from the 
extraction (targeted attack possible, actual device doesn't have to be in range - only the receiver).

In contrast to Logitech Unifying devices, there is no user accessible functionality to exchange the AES key of the
presentation clicker, once it has been compromised. 

In addition to applying encryption, the receiver of affected presentation remotes filters out some keys, like A to Z,
otherwise the devices act as standard keyboard.

On Microsoft Windows operating systems, this "key blacklisting" protection could be bypassed, using (not filtered) 
shortcuts, which produce arbitrary ASCII characters as output. From an attacker's perspective this eliminates the need
to obtain the keyboard layout used by the target, as the shortcut based approach is language independent.  

Devices known to be affected are:
- Logitech R500
- Logitech SPOTLIGHT


### References

- CVE-2019-13054
- vendor report (will be released once patch is available)

### Demos

- Marcus Mengs
    - "Logitech R500": https://twitter.com/mame82/status/1143093313924452353
    - "Logitech SPOTLIGHT" on Win7: https://twitter.com/mame82/status/1144917952254369793
    - "Logitech SPOTLIGHT", automated repetition for same device on Win10: https://twitter.com/mame82/status/1144578129811386368


## 7) Forced Pairing


### Description

A remote attacker could pair a new device to a Logitech Unifying receiver, even if the user has not put the dongle
into pairing mode. This newly paired device could be used by the attacker to inject keystrokes into the host which has
the Unifying dongle connected. The new device doesn't necessarily have to be presented to the user as keyboard, as
other devices (f.e. mice) could be created with keyboard input capabilities, too.

This issue does not lead to eavesdropping of already paired keyboards.

### Reference 

- Logitech Unifying devices, Bastille  
    - https://github.com/BastilleResearch/mousejack/blob/master/doc/advisories/bastille-1.logitech.public.txt
    - https://github.com/BastilleResearch/mousejack/blob/master/doc/advisories/bastille-3.logitech.public.txt

### Demos

- Marcus Mengs
    - Forced pairing: https://twitter.com/mame82/status/1086266411549364224
    - Pair flooding: https://twitter.com/mame82/status/1086253615168344069
  
