#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import sys


CGCOMP_PATH = "/usr/local/ps3dev/bin/cgcomp"


class ShaderCompiler:
    def __init__(self, project_root: Path) -> None:
        self._project_root: Path = project_root.resolve()
        self._shaders_dir: Path = self._project_root / "shaders"
        self._cg_output_dir: Path = self._project_root / "build" / "shaders" / "cg"
        self._glsl_output_dir: Path = self._project_root / "build" / "shaders" / "glsl"

    def _compile_slang(self) -> bool:
        tools_slangc = self._project_root / "tools" / "slang" / "slangc"

        if tools_slangc.is_file():
            slangc_path = str(tools_slangc)
        else:
            slangc_path = shutil.which("slangc")

        if not slangc_path:
            print("[ERROR] slangc not found, cannot compile Slang shaders.")

            return False

        slang_env = os.environ.copy()
        if tools_slangc.is_file():
            existing = slang_env.get("LD_LIBRARY_PATH", "")
            tools_dir = str(tools_slangc.parent)

            if existing:
                slang_env["LD_LIBRARY_PATH"] = f"{tools_dir}:{existing}"
            else:
                slang_env["LD_LIBRARY_PATH"] = tools_dir

        slang_source_dir = self._shaders_dir / "slang"
        if not slang_source_dir.exists():
            return True

        self._glsl_output_dir.mkdir(parents=True, exist_ok=True)

        for slang_file in sorted(slang_source_dir.glob("*.slang")):
            base = slang_file.stem
            name = slang_file.name
            print(f"Compiling {name}...")

            vert_out = self._glsl_output_dir / f"{base}.vert"
            frag_out = self._glsl_output_dir / f"{base}.frag"

            stages = [
                ("vertMain", "vertex", vert_out),
                ("fragMain", "fragment", frag_out),
            ]

            for entry, stage, out_path in stages:
                cmd = [
                    slangc_path,
                    str(slang_file),
                    "-entry",
                    entry,
                    "-stage",
                    stage,
                    "-target",
                    "glsl",
                    "-matrix-layout-column-major",
                    "-o",
                    str(out_path),
                ]

                try:
                    result = subprocess.run(
                        cmd,
                        cwd=self._project_root,
                        capture_output=True,
                        text=True,
                        env=slang_env,
                    )

                    if result.returncode == 0:
                        glsl = out_path.read_text()
                        # slangc emits row_major qualifiers that break this pipeline
                        glsl = glsl.replace("layout(row_major) uniform;\n", "")
                        glsl = glsl.replace("layout(row_major) buffer;\n", "")
                        out_path.write_text(glsl)
                        print(f"Slang {stage} OK -> {out_path}")
                    else:
                        out_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile {stage} shader {name}")

                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except (OSError, UnicodeDecodeError) as error:
                    print(f"[ERROR] Failed to compile {stage}: {error}")

        return True

    def run(self) -> bool:
        self._cg_output_dir.mkdir(parents=True, exist_ok=True)
        self._glsl_output_dir.mkdir(parents=True, exist_ok=True)

        if not self._compile_slang():
            return False

        cgcomp_path = shutil.which(CGCOMP_PATH)
        if not cgcomp_path:
            print("[ERROR] cgcomp not available.")

            return False

        # Pair .vcg/.fcg sources by stem; either half may be missing
        cg_source_dir = self._shaders_dir / "cg"

        vcg_files: dict[str, Path] = {}
        for vcg_path in cg_source_dir.glob("*.vcg"):
            if not vcg_path.is_file():
                continue

            vcg_files[vcg_path.stem] = vcg_path

        fcg_files: dict[str, Path] = {}
        for fcg_path in cg_source_dir.glob("*.fcg"):
            if not fcg_path.is_file():
                continue

            fcg_files[fcg_path.stem] = fcg_path

        shader_names = sorted(set(vcg_files) | set(fcg_files))

        for shader_name in shader_names:
            vcg_file = vcg_files.get(shader_name)
            fcg_file = fcg_files.get(shader_name)
            print(f"Building {shader_name}...")

            vpo_path = self._cg_output_dir / f"{shader_name}.vpo"
            fpo_path = self._cg_output_dir / f"{shader_name}.fpo"

            if vcg_file:
                cmd = [cgcomp_path, "-v", "-Wcg", "-O0", str(vcg_file), str(vpo_path)]

                try:
                    result = subprocess.run(
                        cmd,
                        cwd=self._project_root,
                        capture_output=True,
                        text=True,
                    )

                    if result.returncode == 0:
                        print(f"GCM vertex OK -> {vpo_path}")
                    else:
                        vpo_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile vertex shader {vcg_file}")

                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except (OSError, UnicodeDecodeError) as error:
                    print(f"[ERROR] Failed to compile vertex: {error}")

            if fcg_file:
                cmd = [cgcomp_path, "-f", "-Wcg", "-O0", str(fcg_file), str(fpo_path)]

                try:
                    result = subprocess.run(
                        cmd,
                        cwd=self._project_root,
                        capture_output=True,
                        text=True,
                    )

                    if result.returncode == 0:
                        print(f"GCM fragment OK -> {fpo_path}")
                    else:
                        fpo_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile fragment shader {fcg_file}")

                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except (OSError, UnicodeDecodeError) as error:
                    print(f"[ERROR] Failed to compile fragment: {error}")

        print("Shader compilation complete")

        return True


def main() -> int:
    project_root = Path(__file__).parent.parent.resolve()
    compiler = ShaderCompiler(project_root)

    return 0 if compiler.run() else 1


if __name__ == "__main__":
    sys.exit(main())