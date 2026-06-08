import argparse
import json
import shutil
import subprocess
import tempfile
import time
from pathlib import Path


SSH_OPTIONS = [
    "-o",
    "StrictHostKeyChecking=no",
    "-o",
    "UserKnownHostsFile=/dev/null",
]

FUTURE_AUGMENTATION_NOTE = (
    "Future iteration placeholder: attempt exact external recovery of the "
    "matching guest vmlinux ELF from provider-specific package/debug/archive "
    "sources so the user does not have to extract and adopt the kernel ELF "
    "manually."
)


class KernelCapture:
    def __init__(
        self,
        machine_config_path: Path,
        ssh_host: str,
        ssh_port: int,
        ssh_user: str,
        ssh_password: str,
    ):
        self.machine_config_path = machine_config_path.resolve()
        self.machine_config = json.loads(
            self.machine_config_path.read_text(encoding="utf-8")
        )
        self.ssh_host = ssh_host
        self.ssh_port = ssh_port
        self.ssh_user = ssh_user
        self.ssh_password = ssh_password
        kernel_dir = self.machine_config.get("kernel_bundle_dir")
        if not kernel_dir:
            raise RuntimeError(
                f"kernel_bundle_dir missing from machine config: {self.machine_config_path}"
            )
        self.kernel_dir = Path(kernel_dir).expanduser().resolve()

    @staticmethod
    def _load_machine_config(machine_config_path: Path) -> dict:
        return json.loads(machine_config_path.read_text(encoding="utf-8"))

    @staticmethod
    def _write_machine_config(machine_config_path: Path, data: dict) -> None:
        machine_config_path.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    @staticmethod
    def _load_manifest(manifest_path: Path) -> dict:
        if not manifest_path.is_file():
            return {"schema_version": 1}
        return json.loads(manifest_path.read_text(encoding="utf-8"))

    @staticmethod
    def _write_manifest(manifest_path: Path, data: dict) -> None:
        manifest_path.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    @classmethod
    def _write_user_action_required_notice_file(
        cls,
        *,
        kernel_dir: Path,
        machine_config_path: Path,
        reason: str,
    ) -> Path:
        kernel_dir.mkdir(parents=True, exist_ok=True)
        notice_path = kernel_dir / "MANUAL_CAPTURE_REQUIRED.txt"
        notice_path.write_text(
            "\n".join(
                [
                    "Kernel augmentation cannot proceed automatically for this lineage.",
                    "",
                    f"Reason: {reason}",
                    "",
                    "The automated workflow stopped in a user-action-required state.",
                    "Provide the matching guest kernel ELF explicitly before",
                    "downstream stages consume this lineage.",
                    "",
                    FUTURE_AUGMENTATION_NOTE,
                    "Required canonical artifact:",
                    "  - matching vmlinux-* ELF",
                    "",
                    "Recommended supporting artifacts:",
                    "  - matching System.map-*",
                    "  - matching config-*",
                    "  - matching vmlinuz-*",
                    "",
                    f"Target directory: {kernel_dir}",
                    f"Machine config: {machine_config_path}",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        return notice_path

    @classmethod
    def mark_user_action_required(
        cls,
        machine_config_path: Path,
        reason: str,
    ) -> Path:
        machine_config_path = machine_config_path.resolve()
        data = cls._load_machine_config(machine_config_path)
        kernel_dir = Path(data["kernel_bundle_dir"]).expanduser().resolve()
        kernel_dir.mkdir(parents=True, exist_ok=True)
        data["kernel_capture_status"] = "user_action_required"
        data["kernel_capture_reason"] = reason
        data["kernel_augmentation_note"] = FUTURE_AUGMENTATION_NOTE
        cls._write_machine_config(machine_config_path, data)
        return cls._write_user_action_required_notice_file(
            kernel_dir=kernel_dir,
            machine_config_path=machine_config_path,
            reason=reason,
        )

    @classmethod
    def mark_capture_failed(
        cls,
        machine_config_path: Path,
        reason: str,
    ) -> None:
        machine_config_path = machine_config_path.resolve()
        data = cls._load_machine_config(machine_config_path)
        data["kernel_capture_status"] = "failed"
        data["kernel_capture_reason"] = reason
        cls._write_machine_config(machine_config_path, data)

    @classmethod
    def seed_existing_kernel(
        cls,
        machine_config_path: Path,
        kernel_path: Path,
        *,
        source_kind: str = "seeded",
        capture_status: str = "ready",
    ) -> Path:
        machine_config_path = machine_config_path.resolve()
        data = cls._load_machine_config(machine_config_path)
        kernel_dir = Path(data["kernel_bundle_dir"]).expanduser().resolve()
        kernel_dir.mkdir(parents=True, exist_ok=True)

        kernel_path = kernel_path.expanduser().resolve()
        if not kernel_path.is_file():
            raise FileNotFoundError(f"Seed kernel not found: {kernel_path}")

        output_kernel = kernel_dir / kernel_path.name
        if kernel_path != output_kernel:
            shutil.copy2(kernel_path, output_kernel)

        manifest_path = kernel_dir / "kernel_manifest.json"
        manifest = cls._load_manifest(manifest_path)
        manifest.update(
            {
                "schema_version": 1,
                "augmentation_note": FUTURE_AUGMENTATION_NOTE,
                "selected_kind": source_kind,
                "selected_kernel": str(output_kernel.resolve()),
            }
        )
        source_paths = dict(manifest.get("source_paths", {}))
        source_paths["selected_kernel"] = str(kernel_path)
        manifest["source_paths"] = source_paths
        copied_artifacts = dict(manifest.get("copied_artifacts", {}))
        copied_artifacts["selected_kernel"] = str(output_kernel.resolve())
        manifest["copied_artifacts"] = copied_artifacts
        cls._write_manifest(manifest_path, manifest)

        notice = kernel_dir / "MANUAL_CAPTURE_REQUIRED.txt"
        if notice.exists():
            notice.unlink()

        data["kernel"] = str(output_kernel.resolve())
        data["kernel_capture_status"] = capture_status
        data["kernel_capture_reason"] = ""
        data["kernel_augmentation_note"] = FUTURE_AUGMENTATION_NOTE
        cls._write_machine_config(machine_config_path, data)
        return manifest_path

    def _run_guest(self, command: str, *, check: bool = True) -> subprocess.CompletedProcess:
        return subprocess.run(
            [
                "sshpass",
                "-p",
                self.ssh_password,
                "ssh",
                *SSH_OPTIONS,
                "-p",
                str(self.ssh_port),
                f"{self.ssh_user}@{self.ssh_host}",
                command,
            ],
            text=True,
            capture_output=True,
            check=check,
        )

    def _scp_from_guest(self, remote_path: str, local_path: Path) -> None:
        local_path.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "sshpass",
                "-p",
                self.ssh_password,
                "scp",
                *SSH_OPTIONS,
                "-P",
                str(self.ssh_port),
                f"{self.ssh_user}@{self.ssh_host}:{remote_path}",
                str(local_path),
            ],
            text=True,
            check=True,
        )

    def wait_until_ready(self, wait_seconds: int, poll_seconds: int = 2) -> None:
        deadline = time.time() + wait_seconds
        while time.time() < deadline:
            probe = self._run_guest("true", check=False)
            if probe.returncode == 0:
                return
            time.sleep(poll_seconds)
        raise RuntimeError(
            f"SSH did not become ready for kernel capture within {wait_seconds} seconds."
        )

    @staticmethod
    def _candidate_suffixes(release: str) -> list[str]:
        suffixes = [release]
        parts = release.split("-")
        if len(parts) > 1:
            suffixes.append(parts[-1])
        return suffixes

    @staticmethod
    def _select_named_candidate(paths: list[str], prefixes: tuple[str, ...], release: str) -> str:
        suffixes = KernelCapture._candidate_suffixes(release)
        names = {Path(path).name: path for path in paths}

        for prefix in prefixes:
            for suffix in suffixes:
                candidate = f"{prefix}{suffix}"
                if candidate in names:
                    return names[candidate]

        for prefix in prefixes:
            matching = [path for path in paths if Path(path).name.startswith(prefix)]
            if len(matching) == 1:
                return matching[0]

        return ""

    def _gather_remote_inventory(self) -> dict:
        script = (
            "release=\"$(uname -r)\"; "
            "printf 'RELEASE=%s\\n' \"$release\"; "
            "if command -v apk >/dev/null 2>&1; then "
            "  printf 'PACKAGE_PROVIDER=%s\\n' apk; "
            "  printf 'PACKAGE_LINES\\n'; "
            "  apk info 2>/dev/null | grep '^linux-' | sort || true; "
            "elif command -v dpkg-query >/dev/null 2>&1; then "
            "  printf 'PACKAGE_PROVIDER=%s\\n' deb; "
            "  printf 'PACKAGE_LINES\\n'; "
            "  dpkg-query -W -f='${Package}=${Version}\\n' 2>/dev/null | grep '^linux-' | sort || true; "
            "elif command -v rpm >/dev/null 2>&1; then "
            "  printf 'PACKAGE_PROVIDER=%s\\n' rpm; "
            "  printf 'PACKAGE_LINES\\n'; "
            "  rpm -qa 'kernel*' 2>/dev/null | sort || true; "
            "else "
            "  printf 'PACKAGE_PROVIDER=%s\\n' unknown; "
            "  printf 'PACKAGE_LINES\\n'; "
            "fi; "
            "printf 'DEBUG_BOOT\\n'; "
            "find /usr/lib/debug/boot -maxdepth 1 -type f 2>/dev/null | sort || true; "
            "printf 'BOOT\\n'; "
            "find /boot -maxdepth 1 -type f 2>/dev/null | sort || true; "
            "printf 'MODULES\\n'; "
            "find /lib/modules -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort || true; "
            "printf 'END\\n'"
        )
        output = self._run_guest(script).stdout
        section = None
        release = ""
        package_provider = "unknown"
        package_lines: list[str] = []
        debug_boot: list[str] = []
        boot: list[str] = []
        module_dirs: list[str] = []
        for raw in output.splitlines():
            line = raw.strip()
            if not line:
                continue
            if line.startswith("RELEASE="):
                release = line.split("=", 1)[1]
                continue
            if line.startswith("PACKAGE_PROVIDER="):
                package_provider = line.split("=", 1)[1]
                continue
            if line in {"PACKAGE_LINES", "DEBUG_BOOT", "BOOT", "MODULES", "END"}:
                section = line
                continue
            if section == "PACKAGE_LINES":
                package_lines.append(line)
            elif section == "DEBUG_BOOT":
                debug_boot.append(line)
            elif section == "BOOT":
                boot.append(line)
            elif section == "MODULES":
                module_dirs.append(line)

        if not release:
            raise RuntimeError("Failed to determine guest kernel release over SSH.")
        return {
            "release": release,
            "package_provider": package_provider,
            "package_lines": package_lines,
            "debug_boot": debug_boot,
            "boot": boot,
            "module_dirs": module_dirs,
        }

    def gather_remote_inventory_with_retry(
        self,
        wait_seconds: int,
        poll_seconds: int = 2,
    ) -> dict:
        deadline = time.time() + wait_seconds
        last_error = ""
        while time.time() < deadline:
            try:
                inventory = self._gather_remote_inventory()
                if inventory.get("release"):
                    return inventory
            except Exception as exc:
                last_error = str(exc)
            time.sleep(poll_seconds)
        raise RuntimeError(
            last_error or
            f"Failed to determine guest kernel inventory within {wait_seconds} seconds."
        )

    def _update_machine_config(
        self,
        *,
        kernel_path: Path | None,
        status: str,
        reason: str,
        provider: str,
    ) -> None:
        data = json.loads(self.machine_config_path.read_text(encoding="utf-8"))
        if kernel_path is not None:
            data["kernel"] = str(kernel_path.resolve())
        data["kernel_bundle_dir"] = str(self.kernel_dir.resolve())
        data["kernel_capture_status"] = status
        data["kernel_capture_reason"] = reason
        data["kernel_provider"] = provider
        data["kernel_augmentation_note"] = FUTURE_AUGMENTATION_NOTE
        self.machine_config_path.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        self.machine_config = data

    def execute(self, *, inventory_wait_seconds: int = 120) -> Path:
        print(
            f"[capture-kernel] start machine_config={self.machine_config_path}",
            flush=True,
        )
        manifest_path = self.kernel_dir / "kernel_manifest.json"
        existing_manifest = {}
        if manifest_path.is_file():
            existing_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        existing_selected_kernel = self.machine_config.get("kernel", "")
        existing_ready = self.machine_config.get("kernel_capture_status", "") == "ready"
        existing_kernel_is_valid = bool(existing_selected_kernel) and Path(
            existing_selected_kernel
        ).expanduser().is_file()

        inventory = self.gather_remote_inventory_with_retry(
            wait_seconds=inventory_wait_seconds
        )
        release = inventory["release"]
        package_provider = inventory["package_provider"]
        print(
            f"[capture-kernel] guest release={release} provider={package_provider}",
            flush=True,
        )
        package_lines = inventory["package_lines"]
        debug_boot = inventory["debug_boot"]
        boot = inventory["boot"]
        module_dirs = inventory["module_dirs"]

        selected_kernel = self._select_named_candidate(
            debug_boot,
            prefixes=("vmlinux-",),
            release=release,
        )
        selected_kind = "debug_vmlinux"
        if not selected_kernel:
            selected_kernel = self._select_named_candidate(
                boot,
                prefixes=("vmlinux-",),
                release=release,
            )
            selected_kind = "boot_vmlinux"

        system_map = self._select_named_candidate(
            boot,
            prefixes=("System.map-",),
            release=release,
        )
        config = self._select_named_candidate(
            boot,
            prefixes=("config-",),
            release=release,
        )
        vmlinuz = self._select_named_candidate(
            boot,
            prefixes=("vmlinuz-",),
            release=release,
        )

        with tempfile.TemporaryDirectory(prefix="qflex-kernel-capture-") as tmpdir:
            tmpdir_path = Path(tmpdir)
            staged_kernel = None
            if selected_kernel:
                staged_kernel = tmpdir_path / Path(selected_kernel).name
                self._scp_from_guest(selected_kernel, staged_kernel)
            staged_map = None
            staged_config = None
            staged_vmlinuz = None
            if system_map:
                staged_map = tmpdir_path / Path(system_map).name
                self._scp_from_guest(system_map, staged_map)
            if config:
                staged_config = tmpdir_path / Path(config).name
                self._scp_from_guest(config, staged_config)
            if vmlinuz:
                staged_vmlinuz = tmpdir_path / Path(vmlinuz).name
                self._scp_from_guest(vmlinuz, staged_vmlinuz)

            self.kernel_dir.mkdir(parents=True, exist_ok=True)
            output_kernel = None
            if staged_kernel is not None:
                output_kernel = self.kernel_dir / f"vmlinux-{release}.elf"
                shutil.copy2(staged_kernel, output_kernel)
            output_map = None
            output_config = None
            output_vmlinuz = None
            if staged_map is not None:
                output_map = self.kernel_dir / staged_map.name
                shutil.copy2(staged_map, output_map)
            if staged_config is not None:
                output_config = self.kernel_dir / staged_config.name
                shutil.copy2(staged_config, output_config)
            if staged_vmlinuz is not None:
                output_vmlinuz = self.kernel_dir / staged_vmlinuz.name
                shutil.copy2(staged_vmlinuz, output_vmlinuz)

        manifest = {
            "schema_version": 1,
            "kernel_release": release,
            "selected_kind": selected_kind if selected_kernel else "",
            "selected_kernel": str(output_kernel.resolve()) if output_kernel else "",
            "augmentation_note": FUTURE_AUGMENTATION_NOTE,
            "preserved_kernel": self.machine_config.get("kernel", ""),
            "package_provider": package_provider,
            "package_lines": package_lines,
            "module_dirs": module_dirs,
            "source_paths": {
                "selected_kernel": selected_kernel or "",
                "system_map": system_map,
                "config": config,
                "vmlinuz": vmlinuz,
            },
            "copied_artifacts": {
                "selected_kernel": str(output_kernel.resolve()) if output_kernel else "",
                "system_map": str(output_map.resolve()) if output_map else "",
                "config": str(output_config.resolve()) if output_config else "",
                "vmlinuz": str(output_vmlinuz.resolve()) if output_vmlinuz else "",
            },
        }
        if existing_manifest:
            manifest["previous_manifest"] = {
                "selected_kind": existing_manifest.get("selected_kind", ""),
                "selected_kernel": existing_manifest.get("selected_kernel", ""),
            }
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        notice_path = self.kernel_dir / "MANUAL_CAPTURE_REQUIRED.txt"
        if notice_path.exists():
            notice_path.unlink()

        if output_kernel is not None:
            print(
                f"[capture-kernel] result=ready source=guest_vmlinux kernel={output_kernel}",
                flush=True,
            )
            self._update_machine_config(
                kernel_path=output_kernel,
                status="ready",
                reason="",
                provider=package_provider,
            )
        elif existing_ready and existing_kernel_is_valid:
            print(
                "[capture-kernel] result=ready source=preserved_existing_kernel "
                f"kernel={existing_selected_kernel}",
                flush=True,
            )
            self._update_machine_config(
                kernel_path=Path(existing_selected_kernel).expanduser(),
                status="ready",
                reason="",
                provider=package_provider,
            )
        else:
            reason = (
                "No matching guest vmlinux artifact found in /usr/lib/debug/boot "
                "or /boot. Boot-side kernel provenance was captured, but qflex "
                "does not yet implement automated external recovery of the "
                "canonical ELF."
            )
            print(
                f"[capture-kernel] result=user_action_required reason={reason}",
                flush=True,
            )
            self._update_machine_config(
                kernel_path=None,
                status="user_action_required",
                reason=reason,
                provider=package_provider,
            )
            self._write_user_action_required_notice_file(
                kernel_dir=self.kernel_dir,
                machine_config_path=self.machine_config_path,
                reason=reason,
            )
        return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Capture lineage kernel artifacts from a running guest over SSH."
    )
    parser.add_argument("--machine-config", required=True)
    parser.add_argument("--ssh-host", default="127.0.0.1")
    parser.add_argument("--ssh-port", type=int, default=2222)
    parser.add_argument("--ssh-user", default="qflex")
    parser.add_argument("--ssh-password", default="qflex")
    parser.add_argument("--wait-seconds", type=int, default=900)
    parser.add_argument("--inventory-wait-seconds", type=int, default=120)
    parser.add_argument("--user-action-required-reason", default="")
    parser.add_argument("--adopt-kernel", default="")
    parser.add_argument("--adopt-kind", default="manual_adopt")
    parser.add_argument("--seed-kernel", default="")
    parser.add_argument("--seed-kind", default="seeded")
    args = parser.parse_args()

    machine_config_path = Path(args.machine_config)
    if args.adopt_kernel:
        print(
            f"[capture-kernel] adopt kernel={args.adopt_kernel} kind={args.adopt_kind}",
            flush=True,
        )
        manifest_path = KernelCapture.seed_existing_kernel(
            machine_config_path=machine_config_path,
            kernel_path=Path(args.adopt_kernel),
            source_kind=args.adopt_kind,
            capture_status="ready",
        )
        print(manifest_path)
        return 0
    if args.seed_kernel:
        print(
            f"[capture-kernel] seed kernel={args.seed_kernel} kind={args.seed_kind}",
            flush=True,
        )
        manifest_path = KernelCapture.seed_existing_kernel(
            machine_config_path=machine_config_path,
            kernel_path=Path(args.seed_kernel),
            source_kind=args.seed_kind,
        )
        print(manifest_path)
        return 0
    if args.user_action_required_reason:
        print(
            f"[capture-kernel] user_action_required reason={args.user_action_required_reason}",
            flush=True,
        )
        notice_path = KernelCapture.mark_user_action_required(
            machine_config_path=machine_config_path,
            reason=args.user_action_required_reason,
        )
        print(notice_path)
        return 0

    try:
        capture = KernelCapture(
            machine_config_path=machine_config_path,
            ssh_host=args.ssh_host,
            ssh_port=args.ssh_port,
            ssh_user=args.ssh_user,
            ssh_password=args.ssh_password,
        )
        capture.wait_until_ready(wait_seconds=args.wait_seconds)
        manifest_path = capture.execute(
            inventory_wait_seconds=args.inventory_wait_seconds
        )
        print(manifest_path)
        return 0
    except FileNotFoundError as exc:
        print(f"[capture-kernel] file_not_found error={exc}", flush=True)
        notice_path = KernelCapture.mark_user_action_required(
            machine_config_path=machine_config_path,
            reason=str(exc),
        )
        print(notice_path)
        return 0
    except Exception as exc:
        print(f"[capture-kernel] failed error={exc}", flush=True)
        KernelCapture.mark_capture_failed(
            machine_config_path=machine_config_path,
            reason=str(exc),
        )
        raise


if __name__ == "__main__":
    raise SystemExit(main())
