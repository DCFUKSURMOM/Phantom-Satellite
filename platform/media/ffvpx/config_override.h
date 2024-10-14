// This file contains overrides for config.h, that can be platform-specific.

#ifdef MOZ_LIBAV_FFT
#undef CONFIG_FFT
#undef CONFIG_RDFT
#define CONFIG_FFT 0
#define CONFIG_RDFT 1
#endif