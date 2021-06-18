import sys
from pathlib import Path

# waterz is not locally importable;
# this snippet forces the tests to use the installed version.
# See here for more details:
# https://stackoverflow.com/questions/67176036/how-to-prevent-pytest-using-local-module
project_dir = str(Path(__file__).resolve().parent.parent)
sys.path = [p for p in sys.path if not p.startswith(project_dir)]
