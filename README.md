Ultimate Alarm Clock
====================

Everybody who gets into Arduino makes a clock, and this one is mine. The unique attributes of my clock are:
- Time is synchronized with UTC via a GPS module
- Location is determined from the GPS, and the local time zone is identified (no internet connection needed)
- If the current location respects daylight savings time, it is correctly applied
- The 7-Segment LED display is automatically dimmed based on ambient light level. The min and max brightness are tunable with knobs
- The alarm time is set using rotary switches, which lets you directly set the time instead of repeatedly pressing a button
- It uses a custom PCB that sits like a mini-shield on top of an Arduino Mega 2560. There is a second micro-shield as an audio amplifier to correct the low audio volume supplied by the main board.
- The whole package is in a custom laser cut enclosure with painted, engraved labels
- It has a funny missile-switch toggle for turning the alarm on and off
