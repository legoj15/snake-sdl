#include "botlib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { int x, y; } IVec2;

typedef enum {
  CYCLE_SERPENTINE = 0,
  CYCLE_SPIRAL = 1,
  CYCLE_MAZE = 2,
  CYCLE_SCRAMBLED = 3
} CycleType;

static inline int cell_idx(int w, int x, int y);

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------

static void set_err(char *err, int err_len, const char *msg) {
  if (!err || err_len <= 0)
    return;
  // Ensure null termination.
  snprintf(err, (size_t)err_len, "%s", msg ? msg : "");
}

static int parse_cycle_type(const char *name, CycleType *out, char *err,
                            int err_len) {
  if (!out) {
    set_err(err, err_len, "cycle type output must be non-null");
    return 1;
  }
  if (!name || !*name) {
    *out = CYCLE_MAZE;
    return 0;
  }
  if (strcmp(name, "serpentine") == 0) {
    *out = CYCLE_SERPENTINE;
    return 0;
  }
  if (strcmp(name, "spiral") == 0) {
    *out = CYCLE_SPIRAL;
    return 0;
  }
  if (strcmp(name, "maze") == 0 || strcmp(name, "maze-based") == 0) {
    *out = CYCLE_MAZE;
    return 0;
  }
  if (strcmp(name, "scrambled") == 0) {
    *out = CYCLE_SCRAMBLED;
    return 0;
  }
  set_err(err, err_len, "unknown cycle type");
  return 1;
}

static int dir_to_delta(char c, int *dx, int *dy) {
  switch (c) {
  case 'U':
    *dx = 0;
    *dy = -1;
    return 1;
  case 'D':
    *dx = 0;
    *dy = 1;
    return 1;
  case 'L':
    *dx = -1;
    *dy = 0;
    return 1;
  case 'R':
    *dx = 1;
    *dy = 0;
    return 1;
  default:
    return 0;
  }
}

// Read exactly w*h direction letters from `cycle`, ignoring whitespace.
// Writes normalized uppercase letters into `out_dirs` (size w*h).
static int parse_cycle_letters(int w, int h, const char *cycle, char *out_dirs,
                               char *err, int err_len) {
  if (w <= 0 || h <= 0) {
    set_err(err, err_len, "w and h must be positive");
    return -1;
  }
  if (!cycle || !out_dirs) {
    set_err(err, err_len, "cycle/out_dirs must be non-null");
    return -2;
  }

  const int need = w * h;
  int got = 0;
  for (const char *p = cycle; *p; ++p) {
    if (isspace((unsigned char)*p))
      continue;
    char c = (char)toupper((unsigned char)*p);
    int tdx = 0, tdy = 0;
    if (!dir_to_delta(c, &tdx, &tdy)) {
      set_err(err, err_len,
              "cycle contains invalid direction (expected U/D/L/R)");
      return -3;
    }
    if (got < need) {
      out_dirs[got++] = c;
    } else {
      // Too many non-whitespace chars.
      set_err(err, err_len, "cycle contains more than w*h direction letters");
      return -4;
    }
  }

  if (got != need) {
    set_err(err, err_len,
            "cycle must contain exactly w*h direction letters (whitespace "
            "ignored)");
    return -5;
  }
  return 0;
}

static int dirs_to_next(int w, int h, const char *dirs, bool wrap, int *next,
                        char *err, int err_len) {
  if (!dirs || !next) {
    set_err(err, err_len, "dirs/next must be non-null");
    return 1;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = cell_idx(w, x, y);
      int dx = 0, dy = 0;
      if (!dir_to_delta(dirs[idx], &dx, &dy)) {
        set_err(err, err_len, "cycle contains invalid direction");
        return 2;
      }
      int nx = x + dx;
      int ny = y + dy;
      if (wrap) {
        if (nx < 0)
          nx += w;
        else if (nx >= w)
          nx -= w;
        if (ny < 0)
          ny += h;
        else if (ny >= h)
          ny -= h;
      } else {
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
          set_err(err, err_len, "cycle steps out of bounds");
          return 3;
        }
      }
      next[idx] = cell_idx(w, nx, ny);
    }
  }
  return 0;
}

static int next_to_dirs(int w, int h, const int *next, bool wrap, char *out_dirs,
                        char *err, int err_len) {
  if (!next || !out_dirs) {
    set_err(err, err_len, "next/out_dirs must be non-null");
    return 1;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = cell_idx(w, x, y);
      int ni = next[idx];
      if (ni < 0 || ni >= w * h) {
        set_err(err, err_len, "cycle contains invalid next index");
        return 2;
      }
      int nx = ni % w;
      int ny = ni / w;
      int dx = nx - x;
      int dy = ny - y;
      if (wrap) {
        if (dx == w - 1)
          dx = -1;
        if (dx == -(w - 1))
          dx = 1;
        if (dy == h - 1)
          dy = -1;
        if (dy == -(h - 1))
          dy = 1;
      }
      char d = '?';
      if (dx == 1 && dy == 0)
        d = 'R';
      else if (dx == -1 && dy == 0)
        d = 'L';
      else if (dx == 0 && dy == 1)
        d = 'D';
      else if (dx == 0 && dy == -1)
        d = 'U';
      if (d == '?') {
        set_err(err, err_len, "invalid edge direction");
        return 3;
      }
      out_dirs[idx] = d;
    }
  }
  return 0;
}

static bool build_cycle_grid_base(int w, int h, char *out_dirs) {
  if ((w & 1) || (h & 1))
    return false;
  if (w < 4 || h < 4)
    return false;
  if (!out_dirs)
    return false;

  for (int i = 0; i < w * h; i++)
    out_dirs[i] = 'R';

  out_dirs[0] = 'R';

  for (int y = 0; y < h; y++) {
    if ((y & 1) == 0) {
      for (int x = 1; x < w; x++) {
        if (x < w - 1)
          out_dirs[y * w + x] = 'R';
        else
          out_dirs[y * w + x] = 'D';
      }
    } else {
      for (int x = w - 1; x >= 1; x--) {
        if (y == h - 1 && x == 1)
          out_dirs[y * w + x] = 'L';
        else if (x > 1)
          out_dirs[y * w + x] = 'L';
        else
          out_dirs[y * w + x] = 'D';
      }
    }
  }

  for (int y = h - 1; y >= 1; y--) {
    out_dirs[y * w + 0] = 'U';
  }

  return true;
}

// ------------------------------------------------------------
// Version
// ------------------------------------------------------------

const char *snakebot_version(void) { return "snakebotlib 1.1"; }

// ------------------------------------------------------------
// Cycle generator (maze-based)
// ------------------------------------------------------------

static inline int cell_idx(int w, int x, int y) { return y * w + x; }

typedef struct Rng {
  uint32_t state;
} Rng;

static uint32_t rng_next(Rng *r) {
  uint32_t x = r->state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  r->state = x ? x : 0xA5A5A5A5U;
  return r->state;
}

static int rng_range(Rng *r, int n) {
  return (int)(rng_next(r) % (uint32_t)n);
}

static void shuffle_dirs(int *dirs, int count, Rng *rng) {
  for (int i = count - 1; i > 0; i--) {
    int j = rng_range(rng, i + 1);
    int tmp = dirs[i];
    dirs[i] = dirs[j];
    dirs[j] = tmp;
  }
}

static bool is_adjacent_nonwrap(IVec2 a, IVec2 b) {
  int dx = a.x - b.x;
  int dy = a.y - b.y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  return (dx + dy) == 1;
}

static bool is_adjacent_wrap(IVec2 a, IVec2 b, int w, int h) {
  int dx = a.x - b.x;
  int dy = a.y - b.y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  if (dx == 1 && dy == 0)
    return true;
  if (dx == 0 && dy == 1)
    return true;
  if (dx == w - 1 && dy == 0)
    return true;
  if (dx == 0 && dy == h - 1)
    return true;
  return false;
}

static void set_next_idx(int w, int *next, int x, int y, int nx, int ny) {
  next[cell_idx(w, x, y)] = cell_idx(w, nx, ny);
}

static void init_block_cycles(int w, int h, int *next) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      next[cell_idx(w, x, y)] = -1;
    }
  }
  for (int j = 0; j < h / 2; j++) {
    for (int i = 0; i < w / 2; i++) {
      int x = 2 * i;
      int y = 2 * j;
      // tl -> tr -> br -> bl -> tl
      set_next_idx(w, next, x, y, x + 1, y);
      set_next_idx(w, next, x + 1, y, x + 1, y + 1);
      set_next_idx(w, next, x + 1, y + 1, x, y + 1);
      set_next_idx(w, next, x, y + 1, x, y);
    }
  }
}

static void splice_horizontal(int w, int i, int j, int *next) {
  int trLx = 2 * i + 1;
  int trLy = 2 * j;
  int brLx = 2 * i + 1;
  int brLy = 2 * j + 1;
  int tlRx = 2 * (i + 1);
  int tlRy = 2 * j;
  int blRx = 2 * (i + 1);
  int blRy = 2 * j + 1;

  set_next_idx(w, next, trLx, trLy, tlRx, tlRy);
  set_next_idx(w, next, blRx, blRy, brLx, brLy);
}

static void splice_vertical(int w, int i, int j, int *next) {
  int brUx = 2 * i + 1;
  int brUy = 2 * j + 1;
  int blUx = 2 * i;
  int blUy = 2 * j + 1;
  int tlDx = 2 * i;
  int tlDy = 2 * (j + 1);
  int trDx = 2 * i + 1;
  int trDy = 2 * (j + 1);

  set_next_idx(w, next, brUx, brUy, trDx, trDy);
  set_next_idx(w, next, tlDx, tlDy, blUx, blUy);
}

static void build_maze_splices(int mw, int mh, int w, int *next,
                               unsigned int seed) {
  int total = mw * mh;
  uint8_t *vis = (uint8_t *)malloc((size_t)total);
  int *stack = (int *)malloc((size_t)total * sizeof(int));
  if (!vis || !stack) {
    free(vis);
    free(stack);
    return;
  }
  memset(vis, 0, (size_t)total);

  Rng rng = {seed ? seed : 0xC0FFEEU};

  int sp = 0;
  stack[sp++] = 0;
  vis[0] = 1;

  while (sp > 0) {
    int cur = stack[sp - 1];
    int cx = cur % mw;
    int cy = cur / mw;
    int dirs[4] = {0, 1, 2, 3}; // U D L R
    shuffle_dirs(dirs, 4, &rng);
    bool advanced = false;

    for (int k = 0; k < 4; k++) {
      int dir = dirs[k];
      int nx = cx;
      int ny = cy;
      if (dir == 0)
        ny -= 1;
      else if (dir == 1)
        ny += 1;
      else if (dir == 2)
        nx -= 1;
      else
        nx += 1;
      if (nx < 0 || nx >= mw || ny < 0 || ny >= mh)
        continue;
      int ni = ny * mw + nx;
      if (vis[ni])
        continue;

      // Carve passage (splice cycles).
      if (nx == cx + 1)
        splice_horizontal(w, cx, cy, next);
      else if (nx == cx - 1)
        splice_horizontal(w, nx, ny, next);
      else if (ny == cy + 1)
        splice_vertical(w, cx, cy, next);
      else if (ny == cy - 1)
        splice_vertical(w, nx, ny, next);

      vis[ni] = 1;
      stack[sp++] = ni;
      advanced = true;
      break;
    }

    if (!advanced)
      sp--;
  }

  free(vis);
  free(stack);
}

static void stitch_odd_col(int w, int h, int *next) {
  int we = w - (w & 1);
  int mh = h / 2;
  int xstrip = w - 1;
  int xcore = we - 1;
  for (int j = 0; j < mh; j++) {
    int y0 = 2 * j;
    int y1 = 2 * j + 1;
    set_next_idx(w, next, xcore, y0, xstrip, y0);
    set_next_idx(w, next, xstrip, y0, xstrip, y1);
    set_next_idx(w, next, xstrip, y1, xcore, y1);
  }
}

static void stitch_odd_row(int w, int h, int *next) {
  int he = h - (h & 1);
  int mw = w / 2;
  int ystrip = h - 1;
  int ycore = he - 1;
  for (int i = 0; i < mw; i++) {
    int x0 = 2 * i;
    int x1 = 2 * i + 1;
    set_next_idx(w, next, x1, ycore, x1, ystrip);
    set_next_idx(w, next, x1, ystrip, x0, ystrip);
    set_next_idx(w, next, x0, ystrip, x0, ycore);
  }
}

static int stitch_corner_odd_odd(int w, int h, int *next) {
  int corner = cell_idx(w, w - 1, h - 1);
  if (next[corner] != -1)
    return 0;
  IVec2 c = {w - 1, h - 1};
  int n = w * h;
  for (int i = 0; i < n; i++) {
    if (i == corner)
      continue;
    int v = next[i];
    if (v < 0 || v >= n)
      continue;
    IVec2 u = {i % w, i / w};
    IVec2 vv = {v % w, v / w};
    if (is_adjacent_wrap(u, c, w, h) && is_adjacent_wrap(c, vv, w, h)) {
      next[i] = corner;
      next[corner] = v;
      return 0;
    }
  }
  return 1;
}

static int verify_cycle(const int *next, int w, int h, bool wrap) {
  int n = w * h;
  int *indeg = (int *)malloc((size_t)n * sizeof(int));
  uint8_t *vis = (uint8_t *)malloc((size_t)n);
  if (!indeg || !vis) {
    free(indeg);
    free(vis);
    return 0;
  }
  for (int i = 0; i < n; i++) {
    indeg[i] = 0;
    vis[i] = 0;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = cell_idx(w, x, y);
      int ni = next[idx];
      if (ni < 0 || ni >= n) {
        free(indeg);
        free(vis);
        return 0;
      }
      IVec2 a = {x, y};
      IVec2 b = {ni % w, ni / w};
      if (wrap) {
        if (!is_adjacent_wrap(a, b, w, h)) {
          free(indeg);
          free(vis);
          return 0;
        }
      } else {
        if (!is_adjacent_nonwrap(a, b)) {
          free(indeg);
          free(vis);
          return 0;
        }
      }
      indeg[ni]++;
    }
  }
  for (int i = 0; i < n; i++) {
    if (indeg[i] != 1) {
      free(indeg);
      free(vis);
      return 0;
    }
  }

  int cur = 0;
  for (int step = 0; step < n; step++) {
    if (vis[cur]) {
      free(indeg);
      free(vis);
      return 0;
    }
    vis[cur] = 1;
    cur = next[cur];
  }
  if (cur != 0) {
    free(indeg);
    free(vis);
    return 0;
  }
  for (int i = 0; i < n; i++) {
    if (!vis[i]) {
      free(indeg);
      free(vis);
      return 0;
    }
  }
  free(indeg);
  free(vis);
  return 1;
}

static int gen_cycle_maze_next(int w, int h, unsigned int seed, bool wrap,
                               int *next, char *err, int err_len) {
  int n = w * h;
  if (n <= 0 || n > 16384) {
    set_err(err, err_len, "generator supports up to 16384 cells");
    return 1;
  }
  if (!next) {
    set_err(err, err_len, "next buffer is required");
    return 2;
  }

  int we = w - (w & 1);
  int he = h - (h & 1);
  if (we < 2 || he < 2) {
    set_err(err, err_len, "grid too small for 2x2 blocks");
    return 3;
  }

  init_block_cycles(we, he, next);
  build_maze_splices(we / 2, he / 2, we, next, seed);

  if (w & 1)
    stitch_odd_col(w, he, next);
  if (h & 1)
    stitch_odd_row(w, h, next);
  if ((w & 1) && (h & 1)) {
    if (stitch_corner_odd_odd(w, h, next) != 0) {
      set_err(err, err_len, "failed to stitch odd×odd corner");
      return 4;
    }
  }

  if (!verify_cycle(next, w, h, wrap)) {
    set_err(err, err_len, "maze cycle verification failed");
    return 5;
  }

  set_err(err, err_len, "");
  return 0;
}

static int gen_cycle_maze(int w, int h, unsigned int seed, bool wrap,
                          char *out_dirs, char *err, int err_len) {
  int n = w * h;
  int *next = (int *)malloc((size_t)n * sizeof(int));
  if (!next) {
    set_err(err, err_len, "out of memory");
    return 2;
  }
  int rc = gen_cycle_maze_next(w, h, seed, wrap, next, err, err_len);
  if (rc != 0) {
    free(next);
    return rc;
  }
  rc = next_to_dirs(w, h, next, wrap, out_dirs, err, err_len);
  free(next);
  return rc;
}

// Generate into out_dirs (size w*h). Returns 0 on success.
// Apply valid edge swaps to mutate an existing Hamiltonian cycle.
static int scramble_cycle(int w, int h, int *next, bool wrap,
                          unsigned int seed, char *err, int err_len) {
  int n = w * h;
  if (!next) {
    set_err(err, err_len, "next buffer is required");
    return 1;
  }
  Rng rng = {seed ? seed : 0xC0FFEEU};
  int target_swaps = (n > 8) ? (n / 8) : 1;
  int applied = 0;
  int attempts = 0;
  int max_attempts = n * 50;

  while (applied < target_swaps && attempts < max_attempts) {
    attempts++;
    int a = rng_range(&rng, n);
    int c = rng_range(&rng, n);
    if (a == c)
      continue;
    int b = next[a];
    int d = next[c];
    if (b == c || d == a || b == d)
      continue;

    IVec2 pa = {a % w, a / w};
    IVec2 pb = {b % w, b / w};
    IVec2 pc = {c % w, c / w};
    IVec2 pd = {d % w, d / w};

    bool adj_ad = wrap ? is_adjacent_wrap(pa, pd, w, h)
                       : is_adjacent_nonwrap(pa, pd);
    bool adj_cb = wrap ? is_adjacent_wrap(pc, pb, w, h)
                       : is_adjacent_nonwrap(pc, pb);
    if (!adj_ad || !adj_cb)
      continue;

    int old_b = b;
    int old_d = d;
    next[a] = old_d;
    next[c] = old_b;
    if (verify_cycle(next, w, h, wrap)) {
      applied++;
    } else {
      next[a] = old_b;
      next[c] = old_d;
    }
  }

  if (applied == 0) {
    set_err(err, err_len, "scramble failed to apply any swaps");
    return 2;
  }
  set_err(err, err_len, "");
  return 0;
}

static int gen_cycle_by_type(int w, int h, unsigned int seed, bool wrap,
                             CycleType type, char *out_dirs, char *err,
                             int err_len) {
  int n = w * h;
  if (n <= 0 || n > 16384) {
    set_err(err, err_len, "generator supports up to 16384 cells");
    return 1;
  }

  if (type == CYCLE_SERPENTINE) {
    static char dirs[16384];
    if (!build_cycle_grid_base(w, h, dirs)) {
      set_err(err, err_len, "serpentine cycle requires even w/h >= 4");
      return 2;
    }
    int *next = (int *)malloc((size_t)n * sizeof(int));
    if (!next) {
      set_err(err, err_len, "out of memory");
      return 3;
    }
    int rc = dirs_to_next(w, h, dirs, wrap, next, err, err_len);
    if (rc == 0 && !verify_cycle(next, w, h, wrap)) {
      set_err(err, err_len, "serpentine cycle verification failed");
      rc = 4;
    }
    if (rc == 0)
      rc = next_to_dirs(w, h, next, wrap, out_dirs, err, err_len);
    free(next);
    return rc;
  }

  if (type == CYCLE_SPIRAL) {
    unsigned int spiral_seed = seed ^ 0x5A5A5A5AU;
    return gen_cycle_maze(w, h, spiral_seed, wrap, out_dirs, err, err_len);
  }

  if (type == CYCLE_MAZE) {
    return gen_cycle_maze(w, h, seed, wrap, out_dirs, err, err_len);
  }

  if (type == CYCLE_SCRAMBLED) {
    int *next = (int *)malloc((size_t)n * sizeof(int));
    if (!next) {
      set_err(err, err_len, "out of memory");
      return 5;
    }
    int rc = gen_cycle_maze_next(w, h, seed, wrap, next, err, err_len);
    if (rc == 0)
      rc = scramble_cycle(w, h, next, wrap, seed ^ 0xA5A5A5A5U, err, err_len);
    if (rc == 0)
      rc = next_to_dirs(w, h, next, wrap, out_dirs, err, err_len);
    free(next);
    return rc;
  }

  set_err(err, err_len, "unknown cycle type");
  return 6;
}

static int gen_cycle_letters(int w, int h, unsigned int seed, CycleType type,
                             bool wrap, char *out_dirs, char *err,
                             int err_len) {
  return gen_cycle_by_type(w, h, seed, wrap, type, out_dirs, err, err_len);
}

// Try non-wrapping first; if invalid, retry with wrapping and return that.
static int gen_cycle_letters_with_fallback(int w, int h, unsigned int seed,
                                           CycleType type, bool *out_wrap,
                                           char *out_dirs, char *err,
                                           int err_len) {
  if (out_wrap)
    *out_wrap = false;
  int rc = gen_cycle_letters(w, h, seed, type, false, out_dirs, err, err_len);
  if (rc == 0)
    return 0;
  rc = gen_cycle_letters(w, h, seed, type, true, out_dirs, err, err_len);
  if (rc == 0 && out_wrap)
    *out_wrap = true;
  return rc;
}

int snakebot_generate_cycle(int w, int h, char *out, int out_len, char *err,
                            int err_len) {
  if (w <= 0 || h <= 0) {
    set_err(err, err_len, "w and h must be positive");
    return 1;
  }
  if (!out || out_len <= 0) {
    set_err(err, err_len, "out buffer is required");
    return 2;
  }

  const int need = w * h + 1;
  if (out_len < need) {
    set_err(err, err_len, "out_len must be >= w*h + 1");
    return 3;
  }

  int rc = gen_cycle_letters_with_fallback(w, h, 0U, CYCLE_MAZE, NULL, out, err,
                                           err_len);
  if (rc != 0)
    return 4;

  out[w * h] = '\0';
  set_err(err, err_len, "");
  return 0;
}

// ------------------------------------------------------------
// Cycle validator
// ------------------------------------------------------------

static int validate_cycle_with_wrap(int w, int h, const char *cycle, bool wrap,
                                    char *err, int err_len) {
  if (w <= 0 || h <= 0) {
    set_err(err, err_len, "w and h must be positive");
    return 1;
  }
  if (!cycle) {
    set_err(err, err_len, "cycle must be non-null");
    return 2;
  }

  const int n = w * h;
  // Parse normalized letters.
  // Use a stack VLA for small boards; otherwise fall back to static cap.
  // (This is fine for typical Snake sizes.)
  if (n > 1000000) {
    set_err(err, err_len, "grid too large to validate");
    return 3;
  }
  char dirs[/*n*/ 1];
  // Can't do VLA in MSVC reliably; use a fixed max instead.
  // We'll validate typical sizes (<= 1024*1024); for larger, bail.
  (void)dirs;

  // Allocate a fixed buffer on heap? We promised no heap.
  // So we validate using a rolling read: parse into a local static buffer
  // with a conservative cap.
  if (n > 16384) {
    set_err(err, err_len,
            "validation supports up to 16384 cells (e.g., 128x128). Reduce "
            "board size.");
    return 4;
  }

  static char norm[16384];
  int pr = parse_cycle_letters(w, h, cycle, norm, err, err_len);
  if (pr != 0)
    return 5;

  // Visit by following directions starting at (0,0).
  // Mark visited cells.
  static unsigned char seen[16384];
  memset(seen, 0, (size_t)n);

  int x = 0, y = 0;
  for (int step = 0; step < n; ++step) {
    const int idx = y * w + x;
    if (seen[idx]) {
      set_err(err, err_len, "cycle repeats a cell before visiting all cells");
      return 6;
    }
    seen[idx] = 1;
    int dx = 0, dy = 0;
    if (!dir_to_delta(norm[idx], &dx, &dy)) {
      set_err(err, err_len, "cycle contains invalid direction");
      return 7;
    }
    x += dx;
    y += dy;
    if (wrap) {
      if (x < 0)
        x += w;
      else if (x >= w)
        x -= w;
      if (y < 0)
        y += h;
      else if (y >= h)
        y -= h;
    } else {
      if (x < 0 || x >= w || y < 0 || y >= h) {
        set_err(err, err_len,
                "cycle steps out of bounds (no wrapping allowed)");
        return 8;
      }
    }
  }

  if (!(x == 0 && y == 0)) {
    set_err(err, err_len, "cycle does not return to the start after w*h steps");
    return 9;
  }

  set_err(err, err_len, "");
  return 0;
}

int snakebot_validate_cycle(int w, int h, const char *cycle, char *err,
                            int err_len) {
  bool wrap = ((w & 1) && (h & 1));
  return validate_cycle_with_wrap(w, h, cycle, wrap, err, err_len);
}

// ------------------------------------------------------------
// .cycle container builder/validator
// ------------------------------------------------------------

static const char *skip_ws_lines(const char *p) {
  while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
    p++;
  return p;
}

static int parse_int_kv(const char *key, const char *line, int *out) {
  size_t klen = strlen(key);
  if (strncmp(line, key, klen) != 0)
    return 0;
  if (line[klen] != '=')
    return 0;
  const char *v = line + klen + 1;
  while (*v == ' ' || *v == '\t')
    v++;
  *out = atoi(v);
  return 1;
}

int snakebot_build_cycle_file_ex(int w, int h, int window_w, int window_h,
                                 unsigned int seed, const char *cycle_type,
                                 char *out, int out_len, char *err,
                                 int err_len) {
  if (w <= 0 || h <= 0) {
    set_err(err, err_len, "w and h must be positive");
    return 1;
  }
  if (w < 2 || h < 2) {
    set_err(err, err_len, "w and h must be >= 2");
    return 2;
  }
  if (window_w <= 0 || window_h <= 0) {
    set_err(err, err_len, "window_w and window_h must be positive");
    return 4;
  }
  if ((window_w % w) != 0 || (window_h % h) != 0) {
    set_err(err, err_len, "window size must be divisible by grid size");
    return 6;
  }
  if (!out || out_len <= 0) {
    set_err(err, err_len, "out buffer is required");
    return 7;
  }

  const int n = w * h;
  if (n > 16384) {
    set_err(err, err_len,
            "supports up to 16384 cells (e.g., 128x128). Reduce board size.");
    return 8;
  }

  static char dirs[16384];
  bool wrap_used = false;
  CycleType type = CYCLE_MAZE;
  if (parse_cycle_type(cycle_type, &type, err, err_len) != 0)
    return 9;
  int grc = gen_cycle_letters_with_fallback(w, h, seed, type, &wrap_used, dirs,
                                            err, err_len);
  if (grc != 0)
    return 9;

  // Compose header + metadata.
  int off = 0;
  int wrote = snprintf(out + off, (size_t)out_len - (size_t)off,
                       "SNAKECYCLE 1\n"
                       "width=%d\n"
                       "height=%d\n"
                       "window_w=%d\n"
                       "window_h=%d\n"
                       "seed=%u\n"
                       "cycle_type=%s\n"
                       "wrap=%d\n"
                       "DATA\n",
                       w, h, window_w, window_h, seed,
                       cycle_type ? cycle_type : "maze",
                       wrap_used ? 1 : 0);
  if (wrote < 0) {
    set_err(err, err_len, "snprintf failed");
    return 5;
  }
  off += wrote;
  if (off >= out_len) {
    set_err(err, err_len, "out_len too small for header");
    return 6;
  }

  // Write directions as a neat grid for human-readability.
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (off + 2 >= out_len) { // room for char + newline + null
        set_err(err, err_len, "out_len too small for cycle data");
        return 7;
      }
      out[off++] = dirs[y * w + x];
    }
    out[off++] = '\n';
  }
  out[off] = '\0';

  set_err(err, err_len, "");
  return 0;
}

int snakebot_build_cycle_file(int w, int h, int window_w, int window_h,
                              unsigned int seed, char *out, int out_len,
                              char *err, int err_len) {
  return snakebot_build_cycle_file_ex(w, h, window_w, window_h, seed, "maze",
                                      out, out_len, err, err_len);
}

int snakebot_validate_cycle_file(const char *cycle_file_text, int *out_w,
                                 int *out_h, char *err, int err_len) {
  if (!cycle_file_text) {
    set_err(err, err_len, "cycle_file_text must be non-null");
    return 1;
  }

  const char *p = skip_ws_lines(cycle_file_text);
  const char *line_end = strpbrk(p, "\r\n");
  if (!line_end) {
    set_err(err, err_len, "missing header line");
    return 2;
  }
  size_t hl = (size_t)(line_end - p);
  if (hl != strlen("SNAKECYCLE 1") || strncmp(p, "SNAKECYCLE 1", hl) != 0) {
    set_err(err, err_len, "invalid header (expected 'SNAKECYCLE 1')");
    return 3;
  }

  int w = -1, h = -1;
  int window_w = -1, window_h = -1;
  int wrap = -1;
  int have_data = 0;
  const char *data_start = NULL;

  p = line_end;
  while (*p == '\r' || *p == '\n')
    p++;

  // Parse metadata lines until DATA.
  while (*p) {
    const char *e = strpbrk(p, "\r\n");
    size_t len = e ? (size_t)(e - p) : strlen(p);

    // Copy line into a small buffer for parsing.
    char line[256];
    if (len >= sizeof(line))
      len = sizeof(line) - 1;
    memcpy(line, p, len);
    line[len] = '\0';

    // Trim leading spaces.
    char *s = line;
    while (*s == ' ' || *s == '\t')
      s++;

    if (*s == '\0' || *s == '#') {
      // skip
    } else if (strcmp(s, "DATA") == 0) {
      have_data = 1;
      data_start = e ? (e + 1) : (p + len);
      break;
    } else {
      (void)parse_int_kv("width", s, &w);
      (void)parse_int_kv("height", s, &h);
      (void)parse_int_kv("window_w", s, &window_w);
      (void)parse_int_kv("window_h", s, &window_h);
      (void)parse_int_kv("wrap", s, &wrap);
    }

    if (!e)
      break;
    p = e;
    while (*p == '\r' || *p == '\n')
      p++;
  }

  if (!have_data || !data_start) {
    set_err(err, err_len, "missing DATA section");
    return 4;
  }
  if (w <= 0 || h <= 0) {
    set_err(err, err_len, "missing/invalid width or height metadata");
    return 5;
  }
  if (window_w <= 0 || window_h <= 0) {
    set_err(err, err_len, "missing/invalid window size metadata");
    return 6;
  }
  if ((window_w % w) != 0 || (window_h % h) != 0) {
    set_err(err, err_len, "window size must be divisible by grid size");
    return 7;
  }
  if (w * h > 16384) {
    set_err(err, err_len, "validation supports up to 16384 cells");
    return 8;
  }

  if (out_w)
    *out_w = w;
  if (out_h)
    *out_h = h;

  // Validate the cycle directions within the DATA section.
  bool use_wrap = (wrap >= 0) ? (wrap != 0) : ((w & 1) && (h & 1));
  int vrc = validate_cycle_with_wrap(w, h, data_start, use_wrap, err, err_len);
  if (vrc != 0)
    return 10;

  set_err(err, err_len, "");
  return 0;
}
