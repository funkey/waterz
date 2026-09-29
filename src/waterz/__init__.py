from __future__ import annotations

from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("waterz")
except PackageNotFoundError:  # pragma: no cover
    __version__ = "uninstalled"

from ._agglomerate import agglomerate
from .evaluate import evaluate

__all__ = ["agglomerate", "evaluate"]
