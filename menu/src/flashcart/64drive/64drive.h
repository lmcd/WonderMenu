/**
 * @file 64drive.h
 * @brief 64drive flashcart support
 * @ingroup flashcart 
 */

#ifndef FLASHCART_64DRIVE_H__
#define FLASHCART_64DRIVE_H__


#include "../flashcart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup 64drive
 * @{
 */

flashcart_t *d64_get_flashcart (void);

/** @} */ /* 64drive */

#ifdef __cplusplus
}
#endif

#endif
