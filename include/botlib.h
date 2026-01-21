#pragma once

// Shared-library (DLL/.so) API for generating and validating Snake bot
// Hamiltonian cycles.
//
// Design goals:
//   - No heap allocations (easy ctypes use)
//   - Plain C ABI
//   - Caller provides output/error buffers

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #if defined(SNAKEBOT_EXPORTS)
    #define SNAKEBOT_API __declspec(dllexport)
  #else
    #define SNAKEBOT_API __declspec(dllimport)
  #endif
#else
  #define SNAKEBOT_API __attribute__((visibility("default")))
#endif

// Returns a static version string.
SNAKEBOT_API const char* snakebot_version(void);

// Generate a Hamiltonian cycle for the grid.
//
// Parameters:
//   w, h        Grid dimensions. Supports odd sizes (odd×odd uses wrap).
//   out         Output buffer for direction letters (U/D/L/R), one per cell
//               in row-major order (y*w + x). Null-terminated.
//   out_len     Must be >= w*h + 1.
//   err         Optional error buffer for a human-readable message.
//   err_len     Length of err buffer.
//
// Returns:
//   0 on success, non-zero on failure.
SNAKEBOT_API int snakebot_generate_cycle(
    int w, int h,
    char* out, int out_len,
    char* err, int err_len);

// Validate a cycle string.
//
// Parameters:
//   w, h        Grid dimensions.
//   cycle       Direction letters (U/D/L/R), at least w*h chars.
//               Whitespace is ignored.
//   err, err_len Optional error buffer.
//
// Returns:
//   0 if valid, non-zero if invalid.
SNAKEBOT_API int snakebot_validate_cycle(
    int w, int h,
    const char* cycle,
    char* err, int err_len);

// Build a complete .cycle container file as ASCII text (null-terminated).
//
// The file includes metadata (dimensions + seed) so the game can load it
// safely and deterministically.
//
// Required out_len is roughly:
//   (header+meta) ~ 256 bytes + (w*h + newlines)
// Use something like: out_len >= (w*h + 512).
SNAKEBOT_API int snakebot_build_cycle_file(
    int w, int h,
    int window_w, int window_h,
    unsigned int seed,
    char* out, int out_len,
    char* err, int err_len);

// Build a complete .cycle container file with a selectable cycle type.
//
// cycle_type options:
//   "serpentine", "spiral", "maze", "scrambled"
SNAKEBOT_API int snakebot_build_cycle_file_ex(
    int w, int h,
    int window_w, int window_h,
    unsigned int seed,
    const char* cycle_type,
    char* out, int out_len,
    char* err, int err_len);

// Validate a .cycle container text. If valid, writes detected width/height
// into out_w/out_h when those pointers are non-null.
SNAKEBOT_API int snakebot_validate_cycle_file(
    const char* cycle_file_text,
    int* out_w,
    int* out_h,
    char* err, int err_len);

#ifdef __cplusplus
}
#endif
