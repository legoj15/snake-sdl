from __future__ import annotations

import os
import subprocess
from pathlib import Path
import random

import customtkinter as ctk

from botlib import SnakeBotLib


class ScrollableFrame(ctk.CTkFrame):
    # Custom scroll container so the launcher remains usable on small windows.
    def __init__(self, master: ctk.CTk, **kwargs) -> None:
        super().__init__(master, **kwargs)

        self._canvas = ctk.CTkCanvas(self, highlightthickness=0)
        self._v_scroll = ctk.CTkScrollbar(
            self, orientation="vertical", command=self._canvas.yview
        )

        self._canvas.configure(yscrollcommand=self._v_scroll.set)

        self._canvas.grid(row=0, column=0, sticky="nsew")
        self._v_scroll.grid(row=0, column=1, sticky="ns")

        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(0, weight=1)

        self.frame = ctk.CTkFrame(self)
        self._window = self._canvas.create_window(
            (0, 0), window=self.frame, anchor="nw"
        )

        self.frame.bind("<Configure>", self._on_frame_configure)
        self._canvas.bind("<Configure>", self._on_canvas_configure)

        self._canvas.bind_all("<MouseWheel>", self._on_mousewheel, add="+")
        self._canvas.bind_all("<Button-4>", self._on_linux_scroll_up, add="+")
        self._canvas.bind_all("<Button-5>", self._on_linux_scroll_down, add="+")

        self._sync_colors()

    def _resolve_color(self, color) -> str:
        if isinstance(color, (list, tuple)) and len(color) == 2:
            return color[0] if ctk.get_appearance_mode() == "Light" else color[1]
        return str(color)

    def _sync_colors(self) -> None:
        fg = self.cget("fg_color")
        resolved = self._resolve_color(fg)
        self._canvas.configure(background=resolved)

    def _on_frame_configure(self, _event) -> None:
        self._canvas.configure(
            scrollregion=(0, 0, self.frame.winfo_reqwidth(), self.frame.winfo_reqheight())
        )
        self._clamp_scroll()

    def _on_canvas_configure(self, event) -> None:
        self._canvas.itemconfigure(self._window, width=event.width)
        self._clamp_scroll()

    def _on_mousewheel(self, event) -> None:
        self._canvas.yview_scroll(int(-event.delta / 120), "units")
        self._clamp_scroll()

    def _on_linux_scroll_up(self, _event) -> None:
        self._canvas.yview_scroll(-1, "units")
        self._clamp_scroll()

    def _on_linux_scroll_down(self, _event) -> None:
        self._canvas.yview_scroll(1, "units")
        self._clamp_scroll()

    def _clamp_scroll(self) -> None:
        bbox = self._canvas.bbox("all")
        if not bbox:
            self._canvas.yview_moveto(0.0)
            return
        _, _, _, bottom = bbox
        canvas_h = self._canvas.winfo_height()
        if bottom <= canvas_h:
            self._canvas.yview_moveto(0.0)
            return
        y0, _y1 = self._canvas.yview()
        if y0 < 0.0:
            self._canvas.yview_moveto(0.0)


class ToolTip:
    # Lightweight hover tooltip for slider labels.
    def __init__(self, widget: ctk.CTkBaseClass, text: str) -> None:
        self.widget = widget
        self.text = text
        self._tip: ctk.CTkToplevel | None = None
        self.widget.bind("<Enter>", self._show)
        self.widget.bind("<Leave>", self._hide)

    def _show(self, _event=None) -> None:
        if self._tip is not None or not self.text:
            return
        self._tip = ctk.CTkToplevel(self.widget)
        self._tip.overrideredirect(True)
        self._tip.attributes("-topmost", True)
        label = ctk.CTkLabel(self._tip, text=self.text, justify="left")
        label.pack(padx=8, pady=6)
        x = self.widget.winfo_rootx() + 16
        y = self.widget.winfo_rooty() + self.widget.winfo_height() + 6
        self._tip.geometry(f"+{x}+{y}")

    def _hide(self, _event=None) -> None:
        if self._tip is None:
            return
        self._tip.destroy()
        self._tip = None


class App(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()

        self.title("Snake Launcher")
        self.geometry("900x980")

        ctk.set_appearance_mode("System")
        ctk.set_default_color_theme("blue")

        self._lib: SnakeBotLib | None = None

        root = ScrollableFrame(self)
        root.pack(fill="both", expand=True, padx=14, pady=14)

        title = ctk.CTkLabel(
            root.frame, text="Snake Launcher", font=("Segoe UI", 18, "bold")
        )
        title.pack(anchor="w", padx=12, pady=(10, 6))

        tabs = ctk.CTkTabview(root.frame)
        tabs.pack(fill="both", expand=True, padx=12, pady=(0, 10))
        human_root = tabs.add("Human mode")
        bot_root = tabs.add("Bot mode")
        tabs.set("Human mode")

        # --- Grid config ---
        grid = ctk.CTkFrame(bot_root)
        grid.pack(fill="x", padx=12, pady=(6, 10))

        self.w_var = ctk.StringVar(value="40")
        self.h_var = ctk.StringVar(value="30")
        self.out_var = ctk.StringVar(value=str(self._default_out_path()))

        # --- Bot options ---
        self.bot_tps_steps = [
            7,
            12,
            20,
            30,
            50,
            60,
            100,
            120,
            180,
            200,
            240,
            360,
            500,
            720,
            1000,
            2000,
            5000,
            7000,
        ]
        self.bot_tps_index = ctk.IntVar(value=self.bot_tps_steps.index(20))
        self.seed_var = ctk.StringVar(value="")
        self.cycle_type_var = ctk.StringVar(value="Maze-based")
        self.human_w_var = ctk.StringVar(value="40")
        self.human_h_var = ctk.StringVar(value="30")
        self.human_seed_var = ctk.StringVar(value="")

        self.tuning_presets = {
            "Safe": {
                "k_progress": 10.0,
                "k_away": 50.0,
                "k_skip": 0.75,
                "k_slack": 5.0,
                "k_loop": 100.0,
                "loop_window": 24,
                "aggression_scale": 1.0,
                "max_skip_cap": 0,
            },
            "Aggressive": {
                "k_progress": 14.0,
                "k_away": 35.0,
                "k_skip": 1.2,
                "k_slack": 3.5,
                "k_loop": 80.0,
                "loop_window": 16,
                "aggression_scale": 1.4,
                "max_skip_cap": 0,
            },
            "Greedy Apple": {
                "k_progress": 18.0,
                "k_away": 30.0,
                "k_skip": 1.0,
                "k_slack": 4.0,
                "k_loop": 120.0,
                "loop_window": 24,
                "aggression_scale": 1.2,
                "max_skip_cap": 0,
            },
            "Chaotic": {
                "k_progress": 6.0,
                "k_away": 20.0,
                "k_skip": 0.5,
                "k_slack": 2.0,
                "k_loop": 40.0,
                "loop_window": 12,
                "aggression_scale": 0.8,
                "max_skip_cap": 0,
            },
        }
        self.tuning_display_names = {
            "k_progress": "Progress reward",
            "k_away": "Away penalty",
            "k_skip": "Skip bonus",
            "k_slack": "Slack penalty",
            "k_loop": "Loop penalty",
            "loop_window": "Loop window (ticks)",
            "aggression_scale": "Aggression scale",
            "max_skip_cap": "Max skip cap",
        }
        self.tuning_tooltips = {
            "k_progress": "Rewards moves that advance along the cycle toward the apple.",
            "k_away": "Penalty when a move reduces progress toward the apple.",
            "k_skip": "Bonus for safe multi-step skips ahead on the cycle.",
            "k_slack": "Penalty for consuming too much head-to-tail slack.",
            "k_loop": "Penalty for revisiting recent cycle positions.",
            "loop_window": "How many ticks a position stays 'recent' for loop penalty.",
            "aggression_scale": "Scales how aggressive early-game shortcutting is.",
            "max_skip_cap": "Hard cap on skip distance (0 = no cap).",
        }
        self.preset_var = ctk.StringVar(value="Safe")
        self.last_preset = "Safe"

        self.k_progress_var = ctk.DoubleVar(value=self.tuning_presets["Safe"]["k_progress"])
        self.k_away_var = ctk.DoubleVar(value=self.tuning_presets["Safe"]["k_away"])
        self.k_skip_var = ctk.DoubleVar(value=self.tuning_presets["Safe"]["k_skip"])
        self.k_slack_var = ctk.DoubleVar(value=self.tuning_presets["Safe"]["k_slack"])
        self.k_loop_var = ctk.DoubleVar(value=self.tuning_presets["Safe"]["k_loop"])
        self.loop_window_var = ctk.IntVar(value=self.tuning_presets["Safe"]["loop_window"])
        self.aggression_scale_var = ctk.DoubleVar(
            value=self.tuning_presets["Safe"]["aggression_scale"]
        )
        self.max_skip_cap_var = ctk.IntVar(value=self.tuning_presets["Safe"]["max_skip_cap"])

        ctk.CTkLabel(grid, text="Grid width").grid(
            row=0, column=0, sticky="w", padx=10, pady=(10, 2)
        )
        ctk.CTkEntry(grid, textvariable=self.w_var, width=110).grid(
            row=1, column=0, sticky="w", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(grid, text="Grid height").grid(
            row=0, column=1, sticky="w", padx=10, pady=(10, 2)
        )
        ctk.CTkEntry(grid, textvariable=self.h_var, width=110).grid(
            row=1, column=1, sticky="w", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(grid, text="Output cycle file").grid(
            row=0, column=2, sticky="w", padx=10, pady=(10, 2)
        )
        ctk.CTkEntry(grid, textvariable=self.out_var).grid(
            row=1, column=2, sticky="ew", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(grid, text="Seed (blank = random)").grid(
            row=2, column=0, sticky="w", padx=10, pady=(0, 2)
        )
        ctk.CTkEntry(grid, textvariable=self.seed_var, width=110).grid(
            row=3, column=0, sticky="w", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(grid, text="Cycle type").grid(
            row=2, column=1, sticky="w", padx=10, pady=(0, 2)
        )
        ctk.CTkOptionMenu(
            grid,
            variable=self.cycle_type_var,
            values=["Serpentine", "Spiral", "Maze-based", "Scrambled"],
            width=140,
        ).grid(row=3, column=1, sticky="w", padx=10, pady=(0, 10))

        grid.grid_columnconfigure(2, weight=1)

        # --- Bot config ---
        bot_cfg = ctk.CTkFrame(bot_root)
        bot_cfg.pack(fill="x", padx=12, pady=(0, 10))

        ctk.CTkLabel(
            bot_cfg,
            text="Bot settings",
            font=("Segoe UI", 13, "bold"),
        ).grid(row=0, column=0, columnspan=3, sticky="w", padx=10, pady=(10, 6))

        ctk.CTkLabel(bot_cfg, text="Bot speed (TPS)").grid(
            row=1, column=0, sticky="w", padx=10
        )
        self.bot_tps_value = ctk.CTkLabel(
            bot_cfg, text=str(self.bot_tps_steps[self.bot_tps_index.get()])
        )
        self.bot_tps_value.grid(row=1, column=1, sticky="e", padx=10)

        ctk.CTkSlider(
            bot_cfg,
            from_=0,
            to=len(self.bot_tps_steps) - 1,
            variable=self.bot_tps_index,
            number_of_steps=len(self.bot_tps_steps) - 1,
            command=self._on_bot_tps_change,
        ).grid(row=2, column=0, columnspan=2, sticky="ew", padx=10, pady=(0, 10))

        bot_cfg.grid_columnconfigure(0, weight=1)
        bot_cfg.grid_columnconfigure(1, weight=0, minsize=60)

        # --- Bot tuning ---
        tuning = ctk.CTkFrame(bot_root)
        tuning.pack(fill="x", padx=12, pady=(0, 10))

        ctk.CTkLabel(
            tuning,
            text="Bot behavior presets",
            font=("Segoe UI", 13, "bold"),
        ).grid(row=0, column=0, columnspan=4, sticky="w", padx=10, pady=(10, 6))

        ctk.CTkLabel(tuning, text="Preset").grid(
            row=1, column=0, sticky="w", padx=10, pady=(0, 4)
        )
        ctk.CTkOptionMenu(
            tuning,
            variable=self.preset_var,
            values=["Safe", "Aggressive", "Greedy Apple", "Chaotic", "Custom"],
            command=self._on_preset_change,
            width=160,
        ).grid(row=2, column=0, sticky="w", padx=10, pady=(0, 10))

        self.reset_preset_btn = ctk.CTkButton(
            tuning, text="Reset to preset", command=self._reset_to_preset
        )
        self.reset_preset_btn.grid(row=2, column=1, sticky="w", padx=10, pady=(0, 10))

        self.reset_defaults_btn = ctk.CTkButton(
            tuning, text="Reset to defaults (Safe)", command=self._reset_to_defaults
        )
        self.reset_defaults_btn.grid(row=2, column=2, sticky="w", padx=10, pady=(0, 10))

        ctk.CTkLabel(
            tuning,
            text="Tuning sliders",
            font=("Segoe UI", 12, "bold"),
        ).grid(row=3, column=0, columnspan=4, sticky="w", padx=10, pady=(4, 6))

        self._tuning_labels: dict[str, ctk.CTkLabel] = {}

        row = 4
        row = self._add_tuning_slider(
            tuning,
            row,
            "k_progress",
            self.k_progress_var,
            0.0,
            30.0,
            120,
            "{:.2f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "k_away",
            self.k_away_var,
            0.0,
            120.0,
            120,
            "{:.1f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "k_skip",
            self.k_skip_var,
            0.0,
            2.0,
            120,
            "{:.2f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "k_slack",
            self.k_slack_var,
            0.5,
            10.0,
            95,
            "{:.2f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "k_loop",
            self.k_loop_var,
            0.0,
            200.0,
            100,
            "{:.1f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "loop_window",
            self.loop_window_var,
            1,
            80,
            79,
            "{:d}",
            is_int=True,
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "aggression_scale",
            self.aggression_scale_var,
            0.0,
            2.0,
            100,
            "{:.2f}",
        )
        row = self._add_tuning_slider(
            tuning,
            row,
            "max_skip_cap",
            self.max_skip_cap_var,
            0,
            200,
            200,
            "{:d}",
            is_int=True,
        )

        tuning.grid_columnconfigure(3, weight=1)
        self._refresh_tuning_labels()

        # --- Human mode ---
        human = ctk.CTkFrame(human_root)
        human.pack(fill="x", padx=12, pady=(6, 10))

        ctk.CTkLabel(
            human,
            text="Human mode launcher",
            font=("Segoe UI", 13, "bold"),
        ).grid(row=0, column=0, columnspan=3, sticky="w", padx=10, pady=(10, 6))

        ctk.CTkLabel(human, text="Grid width").grid(
            row=1, column=0, sticky="w", padx=10, pady=(0, 2)
        )
        ctk.CTkEntry(human, textvariable=self.human_w_var, width=110).grid(
            row=2, column=0, sticky="w", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(human, text="Grid height").grid(
            row=1, column=1, sticky="w", padx=10, pady=(0, 2)
        )
        ctk.CTkEntry(human, textvariable=self.human_h_var, width=110).grid(
            row=2, column=1, sticky="w", padx=10, pady=(0, 10)
        )

        ctk.CTkLabel(human, text="Seed (blank = random)").grid(
            row=1, column=2, sticky="w", padx=10, pady=(0, 2)
        )
        ctk.CTkEntry(human, textvariable=self.human_seed_var, width=140).grid(
            row=2, column=2, sticky="w", padx=10, pady=(0, 10)
        )

        self.launch_human_btn = ctk.CTkButton(
            human, text="Launch game (human)", command=self.on_launch_human
        )
        self.launch_human_btn.grid(
            row=3, column=0, columnspan=2, sticky="w", padx=10, pady=(0, 10)
        )

        human.grid_columnconfigure(3, weight=1)

        # --- Buttons ---
        btns = ctk.CTkFrame(bot_root)
        btns.pack(fill="x", padx=12, pady=(0, 10))

        self.gen_btn = ctk.CTkButton(btns, text="Generate", command=self.on_generate)
        self.gen_btn.pack(side="left", padx=(10, 8), pady=10)

        self.launch_btn = ctk.CTkButton(
            btns, text="Launch game (bot)", command=self.on_launch
        )
        self.launch_btn.pack(side="left", padx=8, pady=10)

        self.open_btn = ctk.CTkButton(
            btns, text="Open output folder", command=self.on_open_folder
        )
        self.open_btn.pack(side="left", padx=8, pady=10)

        # --- Status ---
        self.status = ctk.CTkTextbox(bot_root, height=180)
        self.status.pack(fill="both", expand=True, padx=12, pady=(0, 12))
        self.status.configure(state="disabled")

        self.status_human = ctk.CTkTextbox(human_root, height=180)
        self.status_human.pack(fill="both", expand=True, padx=12, pady=(0, 12))
        self.status_human.configure(state="disabled")

        self.log(
            "Ready. Build snakebot shared library, place it next to this GUI, then click Generate/Launch."
        )

    def _default_out_path(self) -> Path:
        here = Path(__file__).resolve().parent
        return here / "generated" / "cycle.cycle"

    def _require_cycle_ext(self, p: Path) -> None:
        if p.suffix.lower() != ".cycle":
            raise ValueError("Refusing to use this path: file must end with .cycle")

    def log(self, msg: str) -> None:
        for box in (self.status, self.status_human):
            box.configure(state="normal")
            box.insert("end", msg + "\n")
            box.see("end")
            box.configure(state="disabled")

    def _get_lib(self) -> SnakeBotLib:
        if self._lib is None:
            # Load from the same directory as this script (or Nuitka exe)
            self._lib = SnakeBotLib()
            self.log(f"Loaded: {self._lib.path}")
            self.log(f"Library version: {self._lib.version()}")
        return self._lib

    def _parse_dims_vars(self, w_var: ctk.StringVar, h_var: ctk.StringVar) -> tuple[int, int]:
        w = int(w_var.get().strip())
        h = int(h_var.get().strip())
        return w, h

    def _parse_seed(self) -> int:
        seed_txt = self.seed_var.get().strip()
        return int(seed_txt) if seed_txt else random.randint(1, 2**31 - 1)

    def _parse_optional_seed(self, var: ctk.StringVar) -> tuple[int, bool]:
        seed_txt = var.get().strip()
        if seed_txt:
            return int(seed_txt), True
        return 0, False

    def _on_bot_tps_change(self, _value: float) -> None:
        idx = int(round(self.bot_tps_index.get()))
        idx = max(0, min(idx, len(self.bot_tps_steps) - 1))
        self.bot_tps_index.set(idx)
        self.bot_tps_value.configure(text=str(self.bot_tps_steps[idx]))

    def _window_for_grid(self, w: int, h: int) -> tuple[int, int]:
        # Match in-game sizing rules: 20px base cell size with 1080p cap.
        base_cell = 20
        max_dim = max(w, h)
        cell = base_cell
        if max_dim > 0 and max_dim * cell > 1080:
            cell = max(1, 1080 // max_dim)
        return w * cell, h * cell

    def _add_tuning_slider(
        self,
        parent: ctk.CTkFrame,
        row: int,
        key: str,
        var: ctk.Variable,
        from_: float,
        to: float,
        steps: int,
        fmt: str,
        is_int: bool = False,
    ) -> int:
        display_name = self.tuning_display_names.get(key, key)
        label = ctk.CTkLabel(parent, text=display_name)
        label.grid(row=row, column=0, sticky="w", padx=10, pady=(0, 4))
        tooltip = self.tuning_tooltips.get(key, "")
        if tooltip:
            ToolTip(label, tooltip)

        value_label = ctk.CTkLabel(parent, text=fmt.format(var.get()))
        value_label.grid(row=row, column=1, sticky="w", padx=10, pady=(0, 4))
        self._tuning_labels[key] = value_label

        def on_change(value: float) -> None:
            if is_int:
                iv = int(round(value))
                var.set(iv)
                value_label.configure(text=fmt.format(iv))
            else:
                fv = float(value)
                var.set(fv)
                value_label.configure(text=fmt.format(fv))
            self._on_tuning_change()

        ctk.CTkSlider(
            parent,
            from_=from_,
            to=to,
            variable=var,
            number_of_steps=steps,
            command=on_change,
        ).grid(row=row + 1, column=0, columnspan=3, sticky="ew", padx=10, pady=(0, 6))

        return row + 2

    def _current_tuning(self) -> dict[str, float]:
        return {
            "k_progress": float(self.k_progress_var.get()),
            "k_away": float(self.k_away_var.get()),
            "k_skip": float(self.k_skip_var.get()),
            "k_slack": float(self.k_slack_var.get()),
            "k_loop": float(self.k_loop_var.get()),
            "loop_window": int(self.loop_window_var.get()),
            "aggression_scale": float(self.aggression_scale_var.get()),
            "max_skip_cap": int(self.max_skip_cap_var.get()),
        }

    def _apply_preset(self, name: str) -> None:
        preset = self.tuning_presets.get(name)
        if not preset:
            return
        self.k_progress_var.set(preset["k_progress"])
        self.k_away_var.set(preset["k_away"])
        self.k_skip_var.set(preset["k_skip"])
        self.k_slack_var.set(preset["k_slack"])
        self.k_loop_var.set(preset["k_loop"])
        self.loop_window_var.set(preset["loop_window"])
        self.aggression_scale_var.set(preset["aggression_scale"])
        self.max_skip_cap_var.set(preset["max_skip_cap"])
        self._refresh_tuning_labels()

    def _refresh_tuning_labels(self) -> None:
        self._tuning_labels["k_progress"].configure(
            text=f"{self.k_progress_var.get():.2f}"
        )
        self._tuning_labels["k_away"].configure(text=f"{self.k_away_var.get():.1f}")
        self._tuning_labels["k_skip"].configure(text=f"{self.k_skip_var.get():.2f}")
        self._tuning_labels["k_slack"].configure(text=f"{self.k_slack_var.get():.2f}")
        self._tuning_labels["k_loop"].configure(text=f"{self.k_loop_var.get():.1f}")
        self._tuning_labels["loop_window"].configure(
            text=f"{int(self.loop_window_var.get())}"
        )
        self._tuning_labels["aggression_scale"].configure(
            text=f"{self.aggression_scale_var.get():.2f}"
        )
        self._tuning_labels["max_skip_cap"].configure(
            text=f"{int(self.max_skip_cap_var.get())}"
        )

    def _preset_matches_current(self, name: str, epsilon: float = 1e-3) -> bool:
        preset = self.tuning_presets.get(name)
        if not preset:
            return False
        current = self._current_tuning()
        for key, value in preset.items():
            if isinstance(value, int):
                if int(current[key]) != value:
                    return False
            else:
                if abs(float(current[key]) - float(value)) > epsilon:
                    return False
        return True

    def _on_preset_change(self, name: str) -> None:
        if name == "Custom":
            return
        self.last_preset = name
        self._apply_preset(name)

    def _reset_to_preset(self) -> None:
        name = self.preset_var.get()
        if name == "Custom":
            name = self.last_preset
        self.preset_var.set(name)
        self._apply_preset(name)

    def _reset_to_defaults(self) -> None:
        self.preset_var.set("Safe")
        self.last_preset = "Safe"
        self._apply_preset("Safe")

    def _on_tuning_change(self) -> None:
        name = self.preset_var.get()
        if name != "Custom" and not self._preset_matches_current(name):
            self.preset_var.set("Custom")

    def _find_game_executable(self) -> Path:
        here = Path(__file__).resolve().parent
        if os.name == "nt":
            cand = here / "game" / "snake.exe"
        else:
            cand = here / "game" / "snake"
        if cand.exists():
            return cand.resolve()
        raise FileNotFoundError(f"Cannot find game executable at: {cand}")

    def on_generate(self) -> None:
        try:
            w, h = self._parse_dims_vars(self.w_var, self.h_var)
            out_path = Path(self.out_var.get()).expanduser().resolve()
            self._require_cycle_ext(out_path)
            out_path.parent.mkdir(parents=True, exist_ok=True)

            lib = self._get_lib()
            seed = self._parse_seed()
            win_w, win_h = self._window_for_grid(w, h)
            cycle_type = self.cycle_type_var.get().strip().lower()
            if cycle_type == "maze-based":
                cycle_type = "maze"

            if seed <= 0:
                raise ValueError("Seed must be a positive integer")
            if w < 2 or h < 2:
                raise ValueError("Grid width/height must be >= 2")
            if win_w <= 0 or win_h <= 0:
                raise ValueError("Window width/height must be positive")
            if (win_w % w) != 0 or (win_h % h) != 0:
                raise ValueError("Window size must be divisible by grid size")

            cycle_file = lib.build_cycle_file(
                w,
                h,
                win_w,
                win_h,
                seed=seed,
                cycle_type=cycle_type,
            )
            try:
                fw, fh = lib.validate_cycle_file(cycle_file)
            except Exception as e:
                raise RuntimeError(f"Generated cycle invalid: {e}") from e
            if (fw, fh) != (w, h):
                raise RuntimeError(
                    f"Library returned mismatched dimensions: {fw}x{fh}"
                )

            out_path.write_text(cycle_file, encoding="ascii")
            self.log(f"Generated valid .cycle for {w}x{h}: {out_path}")
            self.log(f"Seed used: {seed}")
            self.log("Tip: Odd×odd grids use wrapping for the cycle.")
        except Exception as e:
            self.log(f"ERROR: {e}")

    def on_launch(self) -> None:
        try:
            out_path = Path(self.out_var.get()).expanduser().resolve()
            self._require_cycle_ext(out_path)
            if not out_path.exists():
                raise FileNotFoundError(
                    "Cycle file does not exist. Click Generate first."
                )
            idx = int(round(self.bot_tps_index.get()))
            if idx < 0 or idx >= len(self.bot_tps_steps):
                raise ValueError("Bot TPS selection out of range")
            tps = self.bot_tps_steps[idx]

            # Safety: verify file content before launching.
            lib = self._get_lib()
            try:
                lib.validate_cycle_file(
                    out_path.read_text(encoding="ascii", errors="ignore")
                )
            except Exception as e:
                raise RuntimeError(f"Cycle file invalid: {e}") from e

            game = self._find_game_executable()
            if not game.exists():
                raise FileNotFoundError(f"Cannot find game executable at: {game}")

            cmd = [
                str(game),
                "--bot",
                "--bot-gui",
                "--bot-cycle",
                str(out_path),
                "--bot-tps",
                str(tps),
            ]
            tuning = self._current_tuning()
            cmd += [
                "--bot-k-progress",
                str(tuning["k_progress"]),
                "--bot-k-away",
                str(tuning["k_away"]),
                "--bot-k-skip",
                str(tuning["k_skip"]),
                "--bot-k-slack",
                str(tuning["k_slack"]),
                "--bot-k-loop",
                str(tuning["k_loop"]),
                "--bot-loop-window",
                str(tuning["loop_window"]),
                "--bot-aggression-scale",
                str(tuning["aggression_scale"]),
                "--bot-max-skip-cap",
                str(tuning["max_skip_cap"]),
            ]
            self.log("Launching: " + " ".join(cmd))
            subprocess.Popen(cmd, cwd=str(game.parent))
        except Exception as e:
            self.log(f"ERROR: {e}")

    def on_launch_human(self) -> None:
        try:
            w, h = self._parse_dims_vars(self.human_w_var, self.human_h_var)
            if w < 2 or h < 2:
                raise ValueError("Grid width/height must be >= 2")

            seed, seed_set = self._parse_optional_seed(self.human_seed_var)
            if seed_set and seed <= 0:
                raise ValueError("Seed must be a positive integer")

            game = self._find_game_executable()
            if not game.exists():
                raise FileNotFoundError(f"Cannot find game executable at: {game}")

            cmd = [str(game), "--grid-w", str(w), "--grid-h", str(h)]
            if seed_set:
                cmd += ["--seed", str(seed)]

            self.log("Launching: " + " ".join(cmd))
            subprocess.Popen(cmd, cwd=str(game.parent))
        except Exception as e:
            self.log(f"ERROR: {e}")

    def on_open_folder(self) -> None:
        try:
            out_path = Path(self.out_var.get()).expanduser().resolve()
            folder = out_path.parent
            folder.mkdir(parents=True, exist_ok=True)
            if os.name == "nt":
                subprocess.Popen(["explorer", str(folder)])
            elif os.uname().sysname == "Darwin":  # type: ignore[attr-defined]
                subprocess.Popen(["open", str(folder)])
            else:
                subprocess.Popen(["xdg-open", str(folder)])
        except Exception as e:
            self.log(f"ERROR: {e}")


def main() -> None:
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
