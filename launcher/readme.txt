Snake Launcher

The launcher is the recommended way to run the game. It supports:
- Human mode with custom grid size and optional seed
- Bot mode with cycle generation, TPS control, and tuning presets

The launcher uses the snakebot shared library for cycle generation.

Build output layout

build/
  launcher(.exe)
  <standalone GUI deps>
  libsnakebot.so | snakebot.dll
  game/
    snake(.exe)

Run launcher(.exe) from the build/ directory. The launcher will look for the
snake executable under build/game/.

Using the launcher

Human mode
- Set Grid width and Grid height to your desired board size.
- Optionally set a Seed to make the run deterministic.
- Click Launch game (human).

If no seed is provided, the game uses random entropy each run.

Bot mode
- Set Grid width and Grid height.
- Choose a Cycle type and Seed for deterministic generation.
- Click Generate to create a .cycle file.
- Adjust Bot speed (TPS) and any tuning values.
- Click Launch game (bot).

Bot tuning constants (detailed)

These values change the scoring weights used by the bot when deciding whether
it should take a shortcut off the Hamiltonian cycle. They do not disable
safety checks; collisions, corridor checks, tail exceptions, and cycle fallback
are always enforced.

Progress reward (k_progress):
  Rewards moves that advance the head forward along the cycle toward the apple.
  Higher values make the bot prefer aggressive forward progress.
  Example: increasing from 10 to 18 makes the bot chase apples more directly.

Away penalty (k_away):
  Penalty applied when a candidate move makes progress worse or negative.
  Higher values make the bot more conservative and likely to stay on cycle.
  Example: set to 80 and the bot will reject small detours unless progress is positive.

Skip bonus (k_skip):
  Extra reward for taking shortcuts that skip ahead by multiple cycle steps
  when it is safe. Higher values make the bot willing to jump over more of the
  cycle when the corridor is clear.
  Example: set to 1.5 and the bot may take long skips early in the game.

Slack penalty (k_slack):
  Penalty for consuming too much head-to-tail slack. Larger values push the bot
  to preserve safety buffer and avoid tight cuts.
  Example: set to 8 and the bot avoids shortcuts that leave a tiny gap.

Loop penalty (k_loop):
  Penalty for revisiting cycle positions too soon. This reduces tight looping
  behavior. Higher values discourage oscillation near the apple.
  Example: set to 150 and the bot will spread its path out more.

Loop window (ticks) (loop_window):
  Time window (in ticks) for applying the loop penalty. Smaller values allow
  tighter looping. Example: set to 8 to let the bot circle nearby apples more.

Aggression scale (aggression_scale):
  Scales how strongly the bot ramps up its shortcut behavior when the snake is
  short. Higher values make early-game behavior more aggressive; lower values
  make it more cycle-following.
  Example: set to 1.6 for aggressive early shortcuts, 0.6 for conservative play.

Max skip cap (max_skip_cap):
  Hard cap on the maximum skip distance. A value of 0 means no cap. Lower values
  limit how far the bot can jump ahead even if the corridor is clear.
  Example: set to 10 to keep skips short and controlled.

Presets provide sane combinations of these values. Changing any slider after
selecting a preset switches it to Custom.

Run the launcher (dev mode)

python -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python main.py

When running in dev mode, make sure libsnakebot.so / snakebot.dll is in the
same directory as main.py.

Nuitka standalone packaging (example)

From inside launcher/:

python -m nuitka --standalone --enable-plugin=tk-inter --include-package=customtkinter --output-dir=dist main.py

Then copy snakebot.dll / libsnakebot.so next to the produced executable.
