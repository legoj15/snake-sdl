#include "bot.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int cell_index(int w, int x, int y) { return y * w + x; }

static int dist_idx(int a, int b, int n) {
  int d = b - a;
  if (d < 0)
    d += n;
  return d;
}

static int dist_fwd(int a, int b, int n) { return dist_idx(a, b, n); }

static bool step_unwrapped(IVec2 pos, Dir d, int w, int h, IVec2 *out) {
  IVec2 q = pos;
  switch (d) {
  case DIR_UP:
    q.y -= 1;
    break;
  case DIR_DOWN:
    q.y += 1;
    break;
  case DIR_LEFT:
    q.x -= 1;
    break;
  case DIR_RIGHT:
    q.x += 1;
    break;
  }
  if (q.x < 0 || q.x >= w || q.y < 0 || q.y >= h)
    return false;
  if (out)
    *out = q;
  return true;
}

static IVec2 wrap_step(IVec2 pos, Dir d, int w, int h) {
  IVec2 q = pos;
  switch (d) {
  case DIR_UP:
    q.y -= 1;
    break;
  case DIR_DOWN:
    q.y += 1;
    break;
  case DIR_LEFT:
    q.x -= 1;
    break;
  case DIR_RIGHT:
    q.x += 1;
    break;
  }
  if (q.x < 0)
    q.x += w;
  else if (q.x >= w)
    q.x -= w;
  if (q.y < 0)
    q.y += h;
  else if (q.y >= h)
    q.y -= h;
  return q;
}

static Dir dir_from_to_wrap(IVec2 a, IVec2 b, int w, int h) {
  int dx = b.x - a.x;
  int dy = b.y - a.y;
  if (dx == 1 || dx == -(w - 1))
    return DIR_RIGHT;
  if (dx == -1 || dx == (w - 1))
    return DIR_LEFT;
  if (dy == 1 || dy == -(h - 1))
    return DIR_DOWN;
  return DIR_UP;
}

static bool is_opposite(Dir a, Dir b) {
  return (a == DIR_UP && b == DIR_DOWN) || (a == DIR_DOWN && b == DIR_UP) ||
         (a == DIR_LEFT && b == DIR_RIGHT) ||
         (a == DIR_RIGHT && b == DIR_LEFT);
}

static double clampd(double v, double lo, double hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

void apply_preset(Preset p, BotTuning *t) {
  if (!t)
    return;
  switch (p) {
  case PRESET_AGGRESSIVE:
    *t = (BotTuning){.k_progress = 14.0,
                     .k_away = 35.0,
                     .k_skip = 1.2,
                     .k_slack = 3.5,
                     .k_loop = 80.0,
                     .aggression_scale = 1.4,
                     .loop_window = 16,
                     .max_skip_cap = 0};
    break;
  case PRESET_GREEDY_APPLE:
    *t = (BotTuning){.k_progress = 18.0,
                     .k_away = 30.0,
                     .k_skip = 1.0,
                     .k_slack = 4.0,
                     .k_loop = 120.0,
                     .aggression_scale = 1.2,
                     .loop_window = 24,
                     .max_skip_cap = 0};
    break;
  case PRESET_CHAOTIC:
    *t = (BotTuning){.k_progress = 6.0,
                     .k_away = 20.0,
                     .k_skip = 0.5,
                     .k_slack = 2.0,
                     .k_loop = 40.0,
                     .aggression_scale = 0.8,
                     .loop_window = 12,
                     .max_skip_cap = 0};
    break;
  case PRESET_SAFE:
  default:
    *t = (BotTuning){.k_progress = 10.0,
                     .k_away = 50.0,
                     .k_skip = 0.75,
                     .k_slack = 5.0,
                     .k_loop = 100.0,
                     .aggression_scale = 1.0,
                     .loop_window = 24,
                     .max_skip_cap = 0};
    break;
  }
}

bool preset_matches_current(Preset p, const BotTuning *t, double epsilon) {
  if (!t)
    return false;
  BotTuning ref;
  apply_preset(p, &ref);
  if (abs(ref.loop_window - t->loop_window) > 0)
    return false;
  if (abs(ref.max_skip_cap - t->max_skip_cap) > 0)
    return false;
  if (fabs(ref.k_progress - t->k_progress) > epsilon)
    return false;
  if (fabs(ref.k_away - t->k_away) > epsilon)
    return false;
  if (fabs(ref.k_skip - t->k_skip) > epsilon)
    return false;
  if (fabs(ref.k_slack - t->k_slack) > epsilon)
    return false;
  if (fabs(ref.k_loop - t->k_loop) > epsilon)
    return false;
  if (fabs(ref.aggression_scale - t->aggression_scale) > epsilon)
    return false;
  return true;
}

static BotTuning clamp_tuning(const BotTuning *t) {
  BotTuning out = *t;
  out.k_progress = clampd(out.k_progress, 0.0, 50.0);
  out.k_away = clampd(out.k_away, 0.0, 200.0);
  out.k_skip = clampd(out.k_skip, 0.0, 5.0);
  out.k_slack = clampd(out.k_slack, 0.1, 50.0);
  out.k_loop = clampd(out.k_loop, 0.0, 200.0);
  out.aggression_scale = clampd(out.aggression_scale, 0.0, 2.0);
  out.loop_window = clampi(out.loop_window, 1, 200);
  out.max_skip_cap = clampi(out.max_skip_cap, 0, 10000);
  return out;
}

static Dir flip_x_dir(Dir d) {
  if (d == DIR_LEFT)
    return DIR_RIGHT;
  if (d == DIR_RIGHT)
    return DIR_LEFT;
  return d;
}

static bool is_occupied_idx(const Bot *b, int idx, int tail_idx,
                            bool tail_free) {
  if (!b || idx < 0)
    return false;
  if (tail_free && idx == tail_idx)
    return false;
  return b->occupied_idx[idx] != 0;
}

static bool corridor_clear(const Bot *b, int head_idx, int target_idx,
                           int tail_idx, bool tail_free, int n, int max_skip) {
  int d = dist_idx(head_idx, target_idx, n);
  if (d < 1 || d > max_skip)
    return false;
  for (int step = 1; step <= d; step++) {
    int idx = head_idx + step;
    if (idx >= n)
      idx -= n;
    if (idx == tail_idx && tail_free)
      continue;
    if (b->occupied_idx[idx])
      return false;
  }
  return true;
}

static int free_neighbors_after(const Bot *b, IVec2 pos, int tail_idx,
                                bool tail_free) {
  int count = 0;
  const IVec2 dirs[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  for (int i = 0; i < 4; i++) {
    IVec2 q = (IVec2){pos.x + dirs[i].x, pos.y + dirs[i].y};
    if (q.x < 0 || q.x >= b->grid_w || q.y < 0 || q.y >= b->grid_h)
      continue;
    int idx = b->cycle_index[cell_index(b->grid_w, q.x, q.y)];
    if (!is_occupied_idx(b, idx, tail_idx, tail_free))
      count++;
  }
  return count;
}

// Rank a candidate move; higher is better. Safety gates are handled outside.
static double score_move(const Bot *b, const BotTuning *t, int head_idx,
                         int tail_idx, int target, int gap, int max_skip,
                         int len, IVec2 pos, const Apple *a, bool tail_free,
                         int d) {
  if (!t)
    return -1e9;

  int apple_cell = cell_index(b->grid_w, a->pos.x, a->pos.y);
  int apple_idx = b->cycle_index[apple_cell];
  if (apple_idx < 0)
    return -1e9;

  int da = dist_fwd(head_idx, apple_idx, b->n_cells);
  int da2 = dist_fwd(target, apple_idx, b->n_cells);
  int progress = da - da2;

  if (d > 1 && da <= d) {
    if (b->debug_shortcuts) {
      fprintf(stderr, "reject: shortcut passes apple (H=%d A=%d d=%d)\n",
              head_idx, apple_idx, d);
    }
    return -1e9;
  }

  double score = 0.0;
  score += t->k_progress * (double)progress;
  if (progress <= 0)
    score -= t->k_away;

  int manhattan = abs(pos.x - a->pos.x) + abs(pos.y - a->pos.y);
  score -= 0.2 * (double)manhattan;

  double aggression = 1.0 - ((double)len / (double)b->n_cells);
  aggression *= t->aggression_scale;
  aggression = clampd(aggression, 0.0, 1.0);

  if (progress > 0 && d > 1)
    score += t->k_skip * aggression * (double)(d - 1);

  // Penalize tight moves that eat most of the head->tail gap.
  int slack = gap - d;
  if (slack < 0)
    slack = 0;
  score -= t->k_slack / ((double)slack + 1.0);

  int age = (int)(b->tick - (uint32_t)b->last_visit_idx[target]);
  if (age < t->loop_window) {
    if (b->debug_shortcuts) {
      fprintf(stderr, "loop_penalty: idx=%d age=%d\n", target, age);
    }
    score -= t->k_loop / ((double)age + 1.0);
  }

  if (len > 6) {
    int free_n = free_neighbors_after(b, pos, tail_idx, tail_free);
    if (free_n <= 1)
      return -1e9;
  }

  (void)head_idx;
  (void)target;
  (void)max_skip;
  return score;
}

static int idx_of_pos(const Bot *b, IVec2 p) {
  int cell = cell_index(b->grid_w, p.x, p.y);
  return b->cycle_index[cell];
}

// ------------------------------
// Hamiltonian cycle generation
// ------------------------------

static bool build_cycle_grid_base(int w, int h, Dir *out_dirs) {
  if ((w & 1) || (h & 1))
    return false;
  if (w < 4 || h < 4)
    return false;

  for (int i = 0; i < w * h; i++)
    out_dirs[i] = DIR_RIGHT;

  // Start at (0,0) and immediately move right.
  out_dirs[0] = DIR_RIGHT;

  for (int y = 0; y < h; y++) {
    if ((y & 1) == 0) {
      for (int x = 1; x < w; x++) {
        if (x < w - 1)
          out_dirs[y * w + x] = DIR_RIGHT;
        else
          out_dirs[y * w + x] = DIR_DOWN;
      }
    } else {
      for (int x = w - 1; x >= 1; x--) {
        if (y == h - 1 && x == 1)
          out_dirs[y * w + x] = DIR_LEFT;
        else if (x > 1)
          out_dirs[y * w + x] = DIR_LEFT;
        else
          out_dirs[y * w + x] = DIR_DOWN;
      }
    }
  }

  for (int y = h - 1; y >= 1; y--) {
    out_dirs[y * w + 0] = DIR_UP;
  }

  return true;
}

static void build_serpentine_cycle(Bot *b) {
  const int w = b->grid_w;
  const int h = b->grid_h;
  const int n = b->n_cells;

  Dir *tmp = (Dir *)malloc((size_t)n * sizeof(Dir));
  if (!tmp) {
    build_cycle_grid_base(w, h, b->cycle_next_dir);
    return;
  }

  if (!build_cycle_grid_base(w, h, tmp)) {
    free(tmp);
    build_cycle_grid_base(w, h, b->cycle_next_dir);
    return;
  }

  // Flip in X so the cycle's first move from top-right is LEFT.
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int sx = (w - 1 - x);
      Dir d = tmp[y * w + sx];
      b->cycle_next_dir[cell_index(w, x, y)] = flip_x_dir(d);
    }
  }

  free(tmp);
}

static bool build_cycle_mappings(Bot *b) {
  const int w = b->grid_w;
  const int h = b->grid_h;
  const int n = b->n_cells;

  int start_x = 0;
  int start_y = 0;
  for (int i = 0; i < n; i++)
    b->cycle_index[i] = -1;

  IVec2 pos = (IVec2){start_x, start_y};
  for (int i = 0; i < n; i++) {
    int idx = cell_index(w, pos.x, pos.y);
    if (b->cycle_index[idx] != -1)
      return false;
    b->cycle_index[idx] = i;
    b->pos_of_idx[i] = pos;
    b->cycle_dirs[i] = b->cycle_next_dir[idx];
    if (b->cycle_wrap) {
      pos = wrap_step(pos, b->cycle_next_dir[idx], w, h);
    } else {
      if (!step_unwrapped(pos, b->cycle_next_dir[idx], w, h, &pos))
        return false;
    }
  }

  if (!(pos.x == start_x && pos.y == start_y))
    return false;

  for (int i = 0; i < n; i++) {
    b->next_cycle_idx[i] = i + 1;
    if (b->next_cycle_idx[i] >= n)
      b->next_cycle_idx[i] = 0;
  }

  return true;
}

bool Bot_Init(Bot *b, int grid_w, int grid_h) {
  if (!b)
    return false;
  memset(b, 0, sizeof(*b));
  b->grid_w = grid_w;
  b->grid_h = grid_h;
  b->n_cells = grid_w * grid_h;
  if (b->n_cells <= 0)
    return false;

  b->cycle_next_dir = (Dir *)malloc((size_t)b->n_cells * sizeof(Dir));
  b->cycle_index = (int *)malloc((size_t)b->n_cells * sizeof(int));
  b->pos_of_idx = (IVec2 *)malloc((size_t)b->n_cells * sizeof(IVec2));
  b->next_cycle_idx = (int *)malloc((size_t)b->n_cells * sizeof(int));
  b->cycle_dirs = (Dir *)malloc((size_t)b->n_cells * sizeof(Dir));
  b->occupied_idx = (uint8_t *)malloc((size_t)b->n_cells);
  b->last_visit_idx = (int *)malloc((size_t)b->n_cells * sizeof(int));
  if (!b->cycle_next_dir || !b->cycle_index || !b->cycle_dirs ||
      !b->occupied_idx || !b->last_visit_idx || !b->pos_of_idx ||
      !b->next_cycle_idx) {
    Bot_Destroy(b);
    return false;
  }

  b->cycle_pos = -1;
  b->debug_shortcuts = false;
  b->tick = 0;
  b->cycle_wrap = ((grid_w & 1) && (grid_h & 1));
  apply_preset(PRESET_SAFE, &b->tuning);
  b->tuning = clamp_tuning(&b->tuning);
  for (int i = 0; i < b->n_cells; i++)
    b->last_visit_idx[i] = INT_MIN / 2;

  build_serpentine_cycle(b);
  if (!build_cycle_mappings(b)) {
    Bot_Destroy(b);
    return false;
  }

  return true;
}

void Bot_SetTuning(Bot *b, const BotTuning *t) {
  if (!b || !t)
    return;
  b->tuning = clamp_tuning(t);
}

bool Bot_LoadCycleFromFile(Bot *b, const char *path) {
  if (!b || !path)
    return false;

  if (strlen(path) < 6 || strcmp(path + strlen(path) - 6, ".cycle") != 0)
    return false;

  FILE *f = fopen(path, "rb");
  if (!f)
    return false;

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) {
    fclose(f);
    return false;
  }

  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[got] = '\0';

  char *p = buf;
  while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
    p++;
  char *line_end = strpbrk(p, "\r\n");
  if (!line_end) {
    free(buf);
    return false;
  }
  *line_end = '\0';
  if (strcmp(p, "SNAKECYCLE 1") != 0) {
    free(buf);
    return false;
  }

  int meta_w = -1;
  int meta_h = -1;
  int meta_wrap = -1;
  bool have_data = false;
  char *data_start = NULL;

  p = line_end + 1;
  while (*p) {
    while (*p == '\r' || *p == '\n')
      p++;
    if (!*p)
      break;

    char *e = strpbrk(p, "\r\n");
    if (e)
      *e = '\0';

    char *s = p;
    while (*s == ' ' || *s == '\t')
      s++;

    if (*s == '\0' || *s == '#') {
      // skip
    } else if (strcmp(s, "DATA") == 0) {
      have_data = true;
      data_start = e ? (e + 1) : (p + strlen(p));
      break;
    } else {
      char *eq = strchr(s, '=');
      if (eq) {
        *eq = '\0';
        char *key = s;
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t')
          val++;
        int iv = atoi(val);
        if (strcmp(key, "width") == 0)
          meta_w = iv;
        else if (strcmp(key, "height") == 0)
          meta_h = iv;
        else if (strcmp(key, "wrap") == 0)
          meta_wrap = iv;
      }
    }

    if (!e)
      break;
    p = e + 1;
  }

  if (!have_data || !data_start) {
    free(buf);
    return false;
  }
  if (meta_w != -1 && meta_w != b->grid_w) {
    free(buf);
    return false;
  }
  if (meta_h != -1 && meta_h != b->grid_h) {
    free(buf);
    return false;
  }

  int need = b->n_cells;
  int have = 0;
  for (char *q = data_start; *q && have < need; q++) {
    char c = *q;
    Dir d = DIR_UP;
    bool ok = true;
    switch (c) {
    case 'U':
    case 'u':
      d = DIR_UP;
      break;
    case 'D':
    case 'd':
      d = DIR_DOWN;
      break;
    case 'L':
    case 'l':
      d = DIR_LEFT;
      break;
    case 'R':
    case 'r':
      d = DIR_RIGHT;
      break;
    default:
      ok = false;
      break;
    }
    if (!ok)
      continue;
    b->cycle_next_dir[have] = d;
    have++;
  }

  free(buf);
  if (have != need)
    return false;

  b->cycle_pos = -1;
  if (meta_wrap >= 0)
    b->cycle_wrap = (meta_wrap != 0);
  else
    b->cycle_wrap = ((b->grid_w & 1) && (b->grid_h & 1));
  if (!build_cycle_mappings(b))
    return false;

  return true;
}

void Bot_Destroy(Bot *b) {
  if (!b)
    return;
  free(b->cycle_next_dir);
  free(b->cycle_index);
  free(b->pos_of_idx);
  free(b->next_cycle_idx);
  free(b->cycle_dirs);
  free(b->occupied_idx);
  free(b->last_visit_idx);
  memset(b, 0, sizeof(*b));
}

void Bot_OnTick(Bot *b, Snake *s, const Apple *a) {
  if (!b || !s || !a)
    return;

  s->has_q1 = false;
  s->has_q2 = false;

  if (b->n_cells <= 0)
    return;

  // Refresh occupancy each tick (O(L) where L is snake length) for safety checks.
  memset(b->occupied_idx, 0, (size_t)b->n_cells);
  for (int i = 0; i < s->len; i++) {
    int seg_i = cell_index(b->grid_w, s->seg[i].x, s->seg[i].y);
    int idx = b->cycle_index[seg_i];
    if (idx >= 0 && idx < b->n_cells)
      b->occupied_idx[idx] = 1;
  }

  IVec2 head = s->seg[0];
  int head_i = cell_index(b->grid_w, head.x, head.y);
  int pos = b->cycle_index[head_i];
  if (pos < 0)
    return;

  int tail_i = cell_index(b->grid_w, s->seg[s->len - 1].x,
                          s->seg[s->len - 1].y);
  int tail_idx = b->cycle_index[tail_i];
  int gap = dist_idx(pos, tail_idx, b->n_cells) - 1;
  if (s->len == 1)
    gap = b->n_cells - 1;
  if (gap < 0)
    gap = 0;

  double aggression = 1.0 - ((double)s->len / (double)b->n_cells);
  aggression *= b->tuning.aggression_scale;
  aggression = clampd(aggression, 0.0, 1.0);

  int max_skip = 1;
  if (gap > 1) {
    int extra = (int)(aggression * (double)(gap - 1));
    if (extra < 0)
      extra = 0;
    max_skip += extra;
  }
  if (b->tuning.max_skip_cap > 0 && max_skip > b->tuning.max_skip_cap)
    max_skip = b->tuning.max_skip_cap;
  if (gap > 0 && max_skip > gap)
    max_skip = gap;
  if (max_skip < 1)
    max_skip = 1;

  Dir best_dir = b->cycle_next_dir[head_i];
  double best_score = -1e9;
  bool have_choice = false;

  const Dir dirs[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
  for (int i = 0; i < 4; i++) {
    Dir cand_dir = dirs[i];
    if (s->len > 1 && is_opposite(s->dir, cand_dir))
      continue;
    IVec2 cand_pos = wrap_step(head, cand_dir, b->grid_w, b->grid_h);

    int cand_cell = cell_index(b->grid_w, cand_pos.x, cand_pos.y);
    int target = b->cycle_index[cand_cell];
    if (target < 0)
      continue;

    bool will_grow = (cand_pos.x == a->pos.x && cand_pos.y == a->pos.y);
    bool tail_free = !will_grow;
    if (is_occupied_idx(b, target, tail_idx, tail_free))
      continue;

    int d = dist_idx(pos, target, b->n_cells);
    if (d < 1 || d > max_skip)
      continue;
    if (!corridor_clear(b, pos, target, tail_idx, tail_free, b->n_cells,
                        max_skip))
      continue;

    double score = score_move(b, &b->tuning, pos, tail_idx, target, gap,
                              max_skip, s->len, cand_pos, a, tail_free, d);
    if (score > best_score) {
      best_score = score;
      best_dir = cand_dir;
      have_choice = true;
    }
  }

  if (!have_choice) {
    // No safe shortcut; fall back to the Hamiltonian ordering.
    int next_idx = b->next_cycle_idx[pos];
    IVec2 next_pos = b->pos_of_idx[next_idx];
    best_dir = dir_from_to_wrap(head, next_pos, b->grid_w, b->grid_h);
  }

  IVec2 best_pos;
  bool have_best_pos = true;
  best_pos = wrap_step(head, best_dir, b->grid_w, b->grid_h);
  int best_target = -1;
  if (have_best_pos) {
    best_target = idx_of_pos(b, best_pos);
  }

  bool shortcut_taken = (best_dir != b->cycle_next_dir[head_i]);
  if (shortcut_taken && b->debug_shortcuts) {
    if (best_target >= 0) {
      int d = dist_idx(pos, best_target, b->n_cells);
      fprintf(stderr,
              "shortcut: H=%d -> T=%d d=%d gap=%d max_skip=%d\n",
              pos, best_target, d, gap, max_skip);
    }
  }

  if (best_target >= 0) {
    b->last_visit_idx[best_target] = (int)b->tick;
    b->tick++;
  }

  if (is_opposite(s->dir, best_dir)) {
    if (s->len <= 1)
      s->dir = best_dir;
    return;
  }

  Snake_QueueDir(s, best_dir);
}
