.PHONY: install clean

install:
	uv sync --force-reinstall

clean:
	rm -rf build dist src/*.egg-info
	rm -f src/waterz/*.so src/waterz/*.pyd src/waterz/_prebuilt/*.so src/waterz/_prebuilt/*.pyd

test:
	uv run pytest
