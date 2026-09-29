/**
 * Framework build switches. Defaults keep diagnostic code out of the runtime.
 * ESP-IDF maps sdkconfig CONFIG_L2D_* onto these macros in the port.
 * Shared sources must not read CONFIG_* themselves.
 */
#ifndef L2D_CONFIG_H
#define L2D_CONFIG_H

#ifndef L2D_CFG_PROFILE_TIMING
#define L2D_CFG_PROFILE_TIMING 0
#endif
#ifndef L2D_CFG_PROFILE_FINE
#define L2D_CFG_PROFILE_FINE 0
#endif
#ifndef L2D_CFG_PROFILE_VISUAL
#define L2D_CFG_PROFILE_VISUAL 0
#endif
#ifndef L2D_CFG_PROFILE_DETAIL
#define L2D_CFG_PROFILE_DETAIL 0
#endif
#ifndef L2D_CFG_PROFILE_ROI_DIAG
#define L2D_CFG_PROFILE_ROI_DIAG 0
#endif
#ifndef L2D_CFG_SRM_ROI
#define L2D_CFG_SRM_ROI 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define L2D_NOINLINE __attribute__((noinline))
#define L2D_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define L2D_NOINLINE
#define L2D_ALWAYS_INLINE inline
#endif

#endif
