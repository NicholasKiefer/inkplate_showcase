#include "DSTManager.h"
#include <time.h>

void updateDST(NTPClient &timeClient) {
  time_t now = timeClient.getEpochTime();
  tm *currentTime = gmtime(&now);

  // Calculate if we are in DST (last Sunday of March to last Sunday of October)
  bool isDST = (currentTime->tm_mon > 2 && currentTime->tm_mon < 9) || 
               (currentTime->tm_mon == 2 && (currentTime->tm_mday - currentTime->tm_wday) >= 25) ||
               (currentTime->tm_mon == 9 && (currentTime->tm_mday - currentTime->tm_wday) < 25);

  if (isDST) {
    timeClient.setTimeOffset(7200); // CEST: UTC+2
  } else {
    timeClient.setTimeOffset(3600); // CET: UTC+1
  }
}
