export PYTHONPATH := $(PWD):$(PYTHONPATH)


serve-docs:
	mkdocs serve -a 0.0.0.0:8888

build-docs:
	mkdocs build --clean

install-dev-requirements:
	pip install -r requirements.txt && \
	pip install -r requirements.docs.txt && \
	pip install uv && \
	uv tool install bump-my-version
