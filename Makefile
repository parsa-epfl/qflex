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

qemu-config:
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

build-kraken:
	python3 build-multiple-kraken_vanilla.py && \
	rm -rf ./kraken_out&& \
	mkdir -p ./kraken_out && \
	cp -r out/lib/Release/* ./kraken_out && \
	rm -rf out


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
	cd .. && \

parallel-qemu-build:
	cd parallel-qemu && \
  	ninja -C build && \
	cd .. && \
	rm -rf parallel-qemu-saved  && mkdir -p parallel-qemu-saved && \
	cp -r parallel-qemu/build parallel-qemu-saved/build && \
	cp -r parallel-qemu/pc-bios parallel-qemu-saved/pc-bios && \
	mv parallel-qemu-saved/build/pc-bios parallel-qemu-saved/pc-bios
