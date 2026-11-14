/**
 * @file board_select.h
 * @brief Selects the active board descriptor based on PlatformIO build flags.
 */

#pragma once

#if defined(BOARD_WS_C6_AMOLED_164)
#  include "boards/board_ws_c6_amoled_164.h"
#elif defined(BOARD_WS_S3_AMOLED_175C)
#  include "boards/board_ws_s3_amoled_175c.h"
#elif defined(BOARD_WS_S3_AMOLED_180)
#  include "boards/board_ws_s3_amoled_180.h"
#elif defined(BOARD_WS_S3_AMOLED_206)
#  include "boards/board_ws_s3_amoled_206.h"
#elif defined(BOARD_WS_S3_AMOLED_216)
#  include "boards/board_ws_s3_amoled_216.h"
#elif defined(BOARD_WS_S3_AMOLED_241)
#  include "boards/board_ws_s3_amoled_241.h"
#else
// Default fallback for legacy esp32c6_tracker environment
#  define BOARD_WS_C6_AMOLED_164 1
#  include "boards/board_ws_c6_amoled_164.h"
#endif
