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

    def run(self):
        # Create directories
        self.shaders_cg_dir.mkdir(parents=True, exist_ok=True)
        self.shaders_glsl_dir.mkdir(parents=True, exist_ok=True)

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

        # Copy GLSL shaders to build directory
        glsl_src = self.project_root / "shaders" / "glsl"
        if glsl_src.exists():
            shutil.copytree(glsl_src, self.shaders_glsl_dir, dirs_exist_ok=True)
            print(f"[INFO] Copied GLSL shaders from {glsl_src} to {self.shaders_glsl_dir}")

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