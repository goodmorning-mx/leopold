# Leopold computer companion

This local process turns the allowlisted buttons on Leopold's `COMPUTER` screen
into host volume and screenshot actions. When the device is connected by USB,
it uses the direct USB serial path and does not require Device Cloud. If no USB
device is available, it falls back to the Device Cloud path. It does not accept
arbitrary commands.

The companion reads the ignored `device/include/leopold_config.h` file only for
the Device Cloud fallback, starts from the current time so old button presses
are not replayed, and polls the dev API once per second. USB commands are
handled immediately. The serial protocol is computer-independent; the
companion uses macOS, Windows, or Linux host APIs for the actual actions.

```bash
python3 companion/leopold_mac.py
```

To select a specific USB serial port:

```bash
python3 companion/leopold_mac.py --usb-port /dev/cu.usbmodem14101
```

Screenshots are saved to the host's `Desktop/Leopold Screenshots` folder. The
first capture may require the operating system's screen-capture permission or
an installed screenshot utility on Linux.
