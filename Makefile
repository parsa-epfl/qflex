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

qemu-build:
ifndef MODE
	$(error MODE is not set. Usage: make qemu-build MODE=debug|release)
endif
	conan profile detect --force && \
	conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of ./out -b missing && \
	conan build flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of ./out -b missing && \
	conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of ./out && \
	conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of ./out && \
	conan cache clean -v && \
	conan remove -c "*" && \
	./build cq ${MODE} && \
	python3 build-multiple-kraken_vanilla.py
	mkdir -p ./kraken_out && \
	cp -r out/lib/Release/* ./kraken_out && \
	rm -rf out && \
	mkdir -p qemu-saved && \
	cp -r ./qemu/pc-bios ./qemu-saved/pc-bios && \
	cp -r ./qemu/build ./qemu-saved/build

parallel-qemu-build:
	cd parallel-qemu && \
	./configure --target-list=aarch64-softmmu --disable-gtk --enable-capstone && \
  	ninja -C build && \
	cd .. && \
	rm -rf parallel-qemu-saved  && mkdir -p parallel-qemu-saved && \
	cp -r parallel-qemu/build parallel-qemu-saved/build && \
	mv parallel-qemu-saved/build/pc-bios parallel-qemu-saved/pc-bios
