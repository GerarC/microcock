/** @file
 * @brief The date and time stored in new and changed directory entries.
 */
#ifndef FAT_STAMP_HPP
#define FAT_STAMP_HPP

#include <stdint.h>

namespace cock::fat {

/**
 * @brief Reads the current time from the clock server in FAT format.
 *
 * Falls back to 1980-01-01 00:00 when the clock server is not available or does not answer.
 *
 * @param date Receives the FAT date.
 * @param time Receives the FAT time.
 */
void currentStamp(uint16_t &date, uint16_t &time);

} // namespace cock::fat

#endif // !FAT_STAMP_HPP
