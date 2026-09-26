# Leopold Mac companion

This local process turns the allowlisted buttons on Leopold's `COMPUTER` screen
into macOS volume and screenshot actions. It does not accept arbitrary commands.

The companion reads the ignored `device/include/leopold_config.h` file, starts
from the current time so old button presses are not replayed, and polls the dev
API once per second.

```bash
python3 companion/leopold_mac.py
```

Screenshots are saved to `~/Desktop/Leopold Screenshots`. The first capture may
prompt macOS to grant Screen Recording permission to the terminal or Codex host.
