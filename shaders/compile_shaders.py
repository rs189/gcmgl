#!/usr/bin/env python3
import os
import subprocess
import sys
import shutil
import re
from pathlib import Path

class ShaderCompiler:
    def __init__(self, project_root):
        self.project_root = Path(project_root).resolve()
        self.shaders_dir = self.project_root / "shaders"
        self.shaders_cg_dir = self.project_root / "build" / "shaders" / "cg"
        self.shaders_glsl_dir = self.project_root / "build" / "shaders" / "glsl"

    def compile_slang(self):
        tools_slangc = self.project_root / "tools" / "slang" / "slangc"
        slangc_path = str(tools_slangc) if tools_slangc.is_file() else shutil.which('slangc')
        if not slangc_path:
            print("[ERROR] slangc not found, cannot compile Slang shaders.")
            return False

        slang_env = os.environ.copy()
        if tools_slangc.is_file():
            tools_dir = str(tools_slangc.parent)
            existing = slang_env.get('LD_LIBRARY_PATH', '')
            slang_env['LD_LIBRARY_PATH'] = f"{tools_dir}:{existing}" if existing else tools_dir

        slang_dir = self.shaders_dir / "slang"
        if not slang_dir.exists():
            return

        self.shaders_glsl_dir.mkdir(parents=True, exist_ok=True)

        for slang_file in sorted(slang_dir.glob("*.slang")):
            base = slang_file.stem
            print(f"[INFO] Compiling {slang_file.name}")

            vert_out = self.shaders_glsl_dir / f"{base}.vert"
            frag_out = self.shaders_glsl_dir / f"{base}.frag"

            for entry, stage, out_path in [
                ('vertMain', 'vertex',   vert_out),
                ('fragMain', 'fragment', frag_out),
            ]:
                cmd = [
                    slangc_path,
                    str(slang_file),
                    '-entry', entry,
                    '-stage', stage,
                    '-target', 'glsl',
                    '-matrix-layout-column-major',
                    '-o', str(out_path),
                ]
                try:
                    result = subprocess.run(cmd, cwd=self.project_root, capture_output=True, text=True, env=slang_env)
                    if result.returncode == 0:
                        # Strip row_major layout qualifiers
                        glsl = out_path.read_text()
                        glsl = glsl.replace('layout(row_major) uniform;\n', '')
                        glsl = glsl.replace('layout(row_major) buffer;\n', '')
                        out_path.write_text(glsl)
                        print(f"[INFO] Slang {stage} OK -> {out_path}")
                    else:
                        out_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile {stage} shader {slang_file.name}")
                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except Exception as e:
                    print(f"[ERROR] Failed to compile {stage}: {e}")

        return True

    def run(self):
        # Create directories
        self.shaders_cg_dir.mkdir(parents=True, exist_ok=True)
        self.shaders_glsl_dir.mkdir(parents=True, exist_ok=True)

        # Compile Slang shaders to GLSL
        if self.compile_slang() is False:
            return False

        # Check cgcomp
        cgcomp_path = '/usr/local/ps3dev/bin/cgcomp'
        if not shutil.which(cgcomp_path):
            print("[ERROR] cgcomp not available.")

            return False

        # Process .vcg and .fcg files
        cg_dir = self.shaders_dir / "cg"
        shader_pairs = {}

        # Find all .vcg files
        for vcg_file in cg_dir.glob("*.vcg"):
            if not vcg_file.is_file():
                continue

            base = vcg_file.stem
            shader_pairs[base] = {'vcg': vcg_file, 'fcg': None}

        # Find all .fcg files and match with .vcg files
        for fcg_file in cg_dir.glob("*.fcg"):
            if not fcg_file.is_file():
                continue

            base = fcg_file.stem
            if base in shader_pairs:
                shader_pairs[base]['fcg'] = fcg_file
            else:
                shader_pairs[base] = {'vcg': None, 'fcg': fcg_file}

        # Process shader pair
        for shader_name, files in shader_pairs.items():
            print(f"[INFO] Building {shader_name}")

            vpo_path = self.shaders_cg_dir / f"{shader_name}.vpo"
            fpo_path = self.shaders_cg_dir / f"{shader_name}.fpo"

            # Compile vertex shader
            if files['vcg']:
                cmd = [cgcomp_path, '-v', '-Wcg', '-O0', str(files['vcg']), str(vpo_path)]
                try:
                    result = subprocess.run(cmd, cwd=self.project_root, capture_output=True, text=True)
                    if result.returncode == 0:
                        print(f"[INFO] GCM vertex OK -> {vpo_path}")
                    else:
                        vpo_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile vertex shader {files['vcg']}")
                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except Exception as e:
                    print(f"[ERROR] Failed to compile vertex: {e}")

            # Compile fragment shader
            if files['fcg']:
                cmd = [cgcomp_path, '-f', '-Wcg', '-O0', str(files['fcg']), str(fpo_path)]
                try:
                    result = subprocess.run(cmd, cwd=self.project_root, capture_output=True, text=True)
                    if result.returncode == 0:
                        print(f"[INFO] GCM fragment OK -> {fpo_path}")
                    else:
                        fpo_path.unlink(missing_ok=True)
                        print(f"[WARNING] Failed to compile fragment shader {files['fcg']}")
                        if result.stderr:
                            print(result.stderr, file=sys.stderr)
                except Exception as e:
                    print(f"[ERROR] Failed to compile fragment: {e}")

        print("[INFO] Shader compilation complete.")

        return True

def main():
    project_root = Path(__file__).parent.parent.resolve()
    shader_compiler = ShaderCompiler(project_root)
    shader_compiler.run()
    #if shader_compiler.run():
    #    shader_processor = ShaderProcessor(project_root)
    #    shader_processor.run()

if __name__ == "__main__":
    main()