// Written by Michele <o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty

#include <NtpClientLib.h>

// ************************************
// processSyncEvent()
//
// manage NTP sync events and warn in case of error
// ************************************
void processSyncEvent(NTPSyncEvent_t ntpEvent) {
  DEBUG_PRINTLN("[DEBUG] processSyncEvent()");
  if (ntpEvent) {
    DEBUG_PRINTLN("[NTP] Time Sync error: ");
    if (ntpEvent == noResponse)
      DEBUG_PRINTLN("[NTP] NTP server not reachable");
    else if (ntpEvent == invalidAddress)
      DEBUG_PRINTLN("[NTP] Invalid NTP server address");
  } else {
    DEBUG_PRINTLN("[NTP] Got NTP time: "+String(NTP.getTimeDateString(NTP.getLastNTPSync())));
  }
}
