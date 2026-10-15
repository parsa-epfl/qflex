export PYTHONPATH := $(PWD):$(PYTHONPATH)


serve-docs:
	uv run --group docs mkdocs serve -a 0.0.0.0:8888

build-docs:
	uv run --group docs mkdocs build --clean

install-dev-requirements:
	uv sync --group docs && \
	uv tool install bump-my-version
