#ifndef LIBVIDEO2X_MULTIVERSION_H
#define LIBVIDEO2X_MULTIVERSION_H

// Function multi-versioning helper.
//
// Several hot functions in libvideo2x are annotated with
// [[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]] so the
// compiler emits multiple variants and dispatches on the host CPU at runtime.
// Those arch strings are only valid when targeting x86-64 with a compiler that
// supports GCC-style function multi-versioning. On other architectures -- most
// notably arm64 (Apple Silicon, Raspberry Pi, NVIDIA Jetson) -- and on Apple's
// Clang (which does not implement target_clones), the attribute is rejected and
// the build fails. Expand it only where it is known to be supported; elsewhere
// it expands to nothing and the plain "default" code path is used.
#if (defined(__x86_64__) || defined(_M_X64)) \
    && (defined(__GNUC__) || defined(__clang__)) \
    && !defined(__APPLE__)
#  define VIDEO2X_TARGET_CLONES \
      [[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
#else
#  define VIDEO2X_TARGET_CLONES
#endif

#endif  // LIBVIDEO2X_MULTIVERSION_H
