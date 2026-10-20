#pragma once
inline int scanStops=0;
inline int esp_wifi_scan_stop(){++scanStops;return 0;} // Completion is deliberately asynchronous.
