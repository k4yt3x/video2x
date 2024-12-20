#pragma once

#ifdef _WIN32
#ifdef LIBVIDEO2X_EXPORTS
#define LIBVIDEO2X_API __declspec(dllexport)
#else
#define LIBVIDEO2X_API __declspec(dllimport)
#endif
#else
#define LIBVIDEO2X_API
#endif
