export PYTHONPATH := $(PWD):$(PYTHONPATH)


serve-docs:
	mkdocs serve -a 0.0.0.0:8888

build-docs:
	mkdocs build --clean

test:
	python -m pytest tests/ -v

# Same as `test` but with -s (don't capture stdout — see prints / dry-run output
# / the alpine ls output live) and longer tracebacks. Combine with QFLEX_REAL_RUN_TESTS=1
# to actually run the docker-based tests in tests/test_real_runs.py:
#     QFLEX_REAL_RUN_TESTS=1 make test-verbose
test-verbose:
	python -m pytest tests/ -v -s --tb=long

# Run a single real-mode test (or file). Sets QFLEX_REAL_RUN_TESTS=1 so the
# real-run gate opens, then runs the pytest nodeid you pass via TEST=...
# Example:
#     make test-real-one TEST=tests/test_real_runs_docker_image.py
#     make test-real-one TEST=tests/test_real_runs_docker_image.py::test_iputils_ping_installed
test-real-one:
ifndef TEST
	$(error TEST is not set. Usage: make test-real-one TEST=tests/test_real_runs_docker_image.py[::test_name])
endif
	QFLEX_REAL_RUN_TESTS=1 python -m pytest -v -s --tb=long $(TEST)

bump-major:
	bump-my-version bump major --allow-dirty

bump-minor:
	bump-my-version bump minor --allow-dirty

bump-patch:
	bump-my-version bump patch --allow-dirty

install-dev-requirements:
	pip install -r requirements.txt && \
	pip install -r requirements.docs.txt && \
	pip install uv && \
	uv tool install bump-my-version

flexus-config:
	conan profile detect --force


build-kraken:
	python3 build-multiple-kraken_vanilla.py && \
	rm -rf ./kraken_out&& \
	mkdir -p ./kraken_out && \
	cp -r out/lib/Release/* ./kraken_out && \
	rm -rf out

flexus-build:
ifndef MODE
	$(error MODE is not set. Usage: make flexus-config MODE=debug|release)
endif
	conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of ./out -b missing && \
	conan build flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of ./out -b missing && \
	conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of ./out && \
	conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of ./out && \
	make build-kraken

flexus-clean-build:
ifndef MODE
	$(error MODE is not set. Usage: make flexus-config MODE=debug|release)
endif
	make flexus-build MODE=$(MODE) && \
	conan cache clean -v && \
	conan remove -c "*"

qemu-config:
ifndef MODE
	$(error MODE is not set. Usage: make qemu-build MODE=debug|release)
endif
	cd qemu && \
	./configure --target-list=aarch64-softmmu       \
	--disable-docs                      \
	--enable-capstone                   \
	--enable-slirp                      \
	--enable-libqflex                   \
	--enable-snapvm-external            \
	--disable-gtk                       \
	$(if $(filter debug,$(MODE)),--enable-debug) && \
	cd ..

	
# TODO this still has some config in it, move it to speed up building

qemu-move-files:
	rm -rf ./qemu-saved && \
	mkdir -p qemu-saved && \
	cp -r ./qemu/pc-bios ./qemu-saved/pc-bios && \
	cp -r ./qemu/build ./qemu-saved/build



qemu-ninja:
	cd qemu && \
	ninja -C build && \
	cd .. && \
	make qemu-move-files

# TODO check if it can be replaced with qemu-ninja
qemu-build:
	make -C qemu -j && \
	make qemu-move-files

parallel-qemu-config:
	cd parallel-qemu && \
	./configure --target-list=aarch64-softmmu --disable-gtk --enable-capstone && \
	cd ..

parallel-qemu-build:
	cd parallel-qemu && \
  	ninja -C build && \
	cd .. && \
	rm -rf parallel-qemu-saved  && mkdir -p parallel-qemu-saved && \
	cp -r parallel-qemu/build parallel-qemu-saved/build && \
	cp -r parallel-qemu/pc-bios parallel-qemu-saved/pc-bios && \
	mv parallel-qemu-saved/build/pc-bios parallel-qemu-saved/pc-bios
