from __future__ import annotations

import ctypes
import os
from pathlib import Path


def _default_lib_name() -> str:
    if os.name == "nt":
        return "snakebot.dll"
    if os.uname().sysname == "Darwin":  # type: ignore[attr-defined]
        return "libsnakebot.dylib"
    return "libsnakebot.so"


class SnakeBotLib:
    def __init__(self, dll_path: str | os.PathLike[str] | None = None) -> None:
        if dll_path is None:
            # Prefer loading from the GUI executable directory (Nuitka onefile/standalone)
            here = Path(__file__).resolve().parent
            dll_path = here / _default_lib_name()

        self.path = Path(dll_path).resolve()
        if not self.path.exists():
            raise FileNotFoundError(f"Cannot find snakebot library at: {self.path}")

        self._lib = ctypes.CDLL(str(self.path))

        self._lib.snakebot_version.restype = ctypes.c_char_p

        self._lib.snakebot_generate_cycle.argtypes = [
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
        ]
        self._lib.snakebot_generate_cycle.restype = ctypes.c_int

        self._lib.snakebot_validate_cycle.argtypes = [
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_int,
        ]
        self._lib.snakebot_validate_cycle.restype = ctypes.c_int

        self._lib.snakebot_build_cycle_file_ex.argtypes = [
            ctypes.c_int, ctypes.c_int,
            ctypes.c_int, ctypes.c_int,
            ctypes.c_uint,
            ctypes.c_char_p,
            ctypes.c_char_p, ctypes.c_int,
            ctypes.c_char_p, ctypes.c_int,
        ]
        self._lib.snakebot_build_cycle_file_ex.restype = ctypes.c_int

        self._lib.snakebot_validate_cycle_file.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            ctypes.c_char_p, ctypes.c_int,
        ]
        self._lib.snakebot_validate_cycle_file.restype = ctypes.c_int

    def version(self) -> str:
        return self._lib.snakebot_version().decode("utf-8", errors="replace")

    def generate_cycle(self, w: int, h: int) -> str:
        out_len = (w * h + 1)
        out = ctypes.create_string_buffer(out_len)
        err = ctypes.create_string_buffer(512)
        rc = self._lib.snakebot_generate_cycle(w, h, out, out_len, err, len(err))
        if rc != 0:
            raise RuntimeError(err.value.decode("utf-8", errors="replace") or f"generate failed rc={rc}")
        return out.value.decode("ascii")

    def build_cycle_file(
        self,
        w: int,
        h: int,
        window_w: int,
        window_h: int,
        seed: int,
        cycle_type: str,
    ) -> str:
        # Conservative sizing: header/meta + grid + newlines.
        out_len = w * h + 1024
        out = ctypes.create_string_buffer(out_len)
        err = ctypes.create_string_buffer(512)
        rc = self._lib.snakebot_build_cycle_file_ex(
            w, h,
            int(window_w),
            int(window_h),
            ctypes.c_uint(int(seed) & 0xFFFFFFFF),
            cycle_type.encode("ascii", errors="ignore"),
            out, out_len,
            err, len(err),
        )
        if rc != 0:
            raise RuntimeError(err.value.decode("utf-8", errors="replace") or f"build_cycle_file failed rc={rc}")
        return out.value.decode("ascii")

    def validate_cycle_file(self, cycle_file_text: str) -> tuple[int, int]:
        err = ctypes.create_string_buffer(512)
        ow = ctypes.c_int(0)
        oh = ctypes.c_int(0)
        rc = self._lib.snakebot_validate_cycle_file(
            cycle_file_text.encode("ascii", errors="ignore"),
            ctypes.byref(ow),
            ctypes.byref(oh),
            err, len(err)
        )
        if rc != 0:
            raise RuntimeError(err.value.decode("utf-8", errors="replace") or f"validate_cycle_file failed rc={rc}")
        return (int(ow.value), int(oh.value))

    def validate_cycle(self, w: int, h: int, cycle: str) -> None:
        err = ctypes.create_string_buffer(512)
        rc = self._lib.snakebot_validate_cycle(w, h, cycle.encode("ascii", errors="ignore"), err, len(err))
        if rc != 0:
            raise RuntimeError(err.value.decode("utf-8", errors="replace") or f"validate failed rc={rc}")
