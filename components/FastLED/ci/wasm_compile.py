import argparse
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import List

from ci.paths import PROJECT_ROOT

HERE: Path = Path(__file__).parent
DOCKER_FILE: Path = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "Dockerfile"
)

DEFAULT_WASM_PRJECT_DIR: str = str(PROJECT_ROOT / "examples" / "wasm")

assert DOCKER_FILE.exists(), f"ERROR: Dockerfile not found at {DOCKER_FILE}"


class WASMCompileError(Exception):
    """Custom exception for WASM compilation errors."""

    pass


def get_image_versions() -> List[str]:
    result = subprocess.run(
        [
            "docker",
            "images",
            "--format",
            "{{.Repository}}:{{.Tag}}",
            "--filter",
            "reference=fastled-wasm-compiler*",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    return sorted([img for img in result.stdout.split("\n") if img], reverse=True)


def rename_images() -> None:
    versions = get_image_versions()
    for i, version in enumerate(versions[1:], start=1):  # Skip the first (latest) image
        new_name = f"fastled-wasm-compiler-{i}"
        if version != new_name:
            subprocess.run(["docker", "tag", version, new_name], check=True)
            subprocess.run(["docker", "rmi", version], check=True)


def remove_oldest_image() -> None:
    versions = get_image_versions()
    if len(versions) > 5:
        oldest = versions[-1]
        print(f"Removing oldest image: {oldest}")
        subprocess.run(["docker", "rmi", oldest], check=True)


def container_exists() -> bool:
    result = subprocess.run(
        [
            "docker",
            "ps",
            "-a",
            "--format",
            "{{.Names}}",
            "--filter",
            "name=fastled-wasm-compiler",
        ],
        capture_output=True,
        text=True,
    )
    return "fastled-wasm-compiler" in result.stdout


def image_exists() -> bool:
    versions = get_image_versions()
    return len(versions) > 0


def remove_existing_container(container_name: str) -> None:
    try:
        result = subprocess.run(
            [
                "docker",
                "ps",
                "-a",
                "--filter",
                f"name={container_name}",
                "--format",
                "{{.Names}}",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        if result.stdout.strip():
            print(f"Removing existing container: {container_name}")
            subprocess.run(["docker", "rm", "-f", container_name], check=True)
        else:
            print(f"No existing container found with name: {container_name}")
    except subprocess.CalledProcessError as e:
        print(f"Error checking/removing container: {e}")


def remove_dangling_images() -> None:
    print("Removing dangling images...")
    subprocess.run(["docker", "image", "prune", "-f"], check=True)


def clean() -> None:
    if container_exists():
        print("Stopping and removing fastled-wasm-compiler container...")
        subprocess.run(["docker", "stop", "fastled-wasm-compiler"], check=True)
        subprocess.run(["docker", "rm", "fastled-wasm-compiler"], check=True)
    else:
        print("No container found for fastled-wasm-compiler.")

    print("Removing all fastled-wasm-compiler related images...")
    versions = get_image_versions()
    for version in versions:
        subprocess.run(["docker", "rmi", "-f", version], check=True)

    remove_dangling_images()


def build_image(debug: bool = True) -> None:
    print()
    print("#######################################")
    print("# Building Docker image...")
    print("#######################################")
    print()
    try:
        cmd_list: List[str] = [
            "docker",
            "build",
            "--platform",
            "linux/amd64",
            "-t",
            "fastled-wasm-compiler",
        ]
        if debug:
            cmd_list.extend(["--build-arg", "DEBUG=1"])
        cmd_list.extend(
            [
                "-f",
                str(DOCKER_FILE),
                str(PROJECT_ROOT),
            ]
        )
        cmd_str: str = subprocess.list2cmdline(cmd_list)
        print(f"Running command: {cmd_str}")
        subprocess.run(
            cmd_list,
            check=True,
            text=True,
        )
        rename_images()
        remove_oldest_image()
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to build Docker image.\nError: {e}")


def is_tty() -> bool:
    return sys.stdout.isatty()


def run_container(directory: str, interactive: bool, debug: bool = False) -> None:

    absolute_directory: str = os.path.abspath(directory)
    base_name = os.path.basename(absolute_directory)
    if not os.path.isdir(absolute_directory):
        raise WASMCompileError(
            f"ERROR: Directory '{absolute_directory}' does not exist."
        )

    try:
        latest_image = "fastled-wasm-compiler"
        docker_command: List[str] = [
            "docker",
            "run",
            "--name",
            "fastled-wasm-compiler",
            "--platform",
            "linux/amd64",
            "-v",
            f"{absolute_directory}:/mapped/{base_name}",
        ]
        docker_command.append(latest_image)
        if debug and not interactive:
            docker_command.extend(["python", "/js/compile.py", "--debug"])
        if is_tty():
            docker_command.insert(4, "-it")
        if interactive:
            docker_command.append("/bin/bash")
        cmd_str: str = subprocess.list2cmdline(docker_command)

        print()
        print("#######################################")
        print("# Running Docker container with:")
        print(f"#   {cmd_str}")
        print("#######################################\n")

        print(f"Running command: {cmd_str}")
        subprocess.run(docker_command, check=True)
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to run Docker container.\n{e}")


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description="WASM Compiler for FastLED"
    )
    parser.add_argument(
        "directory",
        nargs="?",
        help="The directory to mount as a volume",
        default=DEFAULT_WASM_PRJECT_DIR,
    )
    parser.add_argument(
        "-b",
        "--build",
        action="store_true",
        help="Build the Docker image before running",
    )
    parser.add_argument(
        "-c",
        "--clean",
        action="store_true",
        help="Clean up Docker containers and images",
    )
    parser.add_argument(
        "-i",
        "--interactive",
        action="store_true",
        help="Run the container in interactive mode with /bin/bash",
    )
    parser.add_argument(
        "-o",
        "--open",
        action="store_true",
        help="Launch a web server and open a browser after compilation",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enables debug flags and disables optimization. Results in larger binary with debug info.",
    )
    args: argparse.Namespace = parser.parse_args()

    try:
        if args.clean:
            clean()
            return
        if args.directory is None:
            parser.error("ERROR: directory is required unless --clean is specified")
        debug_mode = args.debug
        if args.build or not image_exists():
            # Check for and remove existing container before building
            remove_existing_container("fastled-wasm-compiler")
            build_image(debug_mode)
            remove_dangling_images()

        run_container(args.directory, args.interactive, debug_mode)

        output_dir = str(Path(args.directory) / "fastled_js")

        if args.open:
            print(f"Launching web server from {output_dir}")
            run_web_server(output_dir)

    except WASMCompileError as e:
        print(f"\033[91m{str(e)}\033[0m")  # Print error in red
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
        sys.exit(0)
    except Exception as e:
        print(f"\033[91mUnexpected error: {str(e)}\033[0m")  # Print error in red
        sys.exit(1)


def run_web_server(directory: str) -> None:
    print("Launching web server at", directory)
    print("Launching live-server at http://localhost")

    # Check if live-server is installed
    try:
        print("running live-server --version")
        subprocess.run(
            "live-server --version", capture_output=True, shell=True, check=True
        )
    except subprocess.CalledProcessError:
        print("live-server not found. Please install it with:")
        print("npm install -g live-server")
        return

    # Create a detached command window running live-server
    cmd_list = ["live-server"]
    cmd_str = subprocess.list2cmdline(cmd_list)
    print(f"Running command: {cmd_str} at {directory}")
    subprocess.Popen(
        cmd_str,
        shell=True,
        cwd=directory,
        # creationflags=CREATE_NEW_CONSOLE | DETACHED_PROCESS
    )

    # import time
    # time.sleep(3)  # Give the server time to start
    # webbrowser.open(f"http://localhost:{port}")
    while True:

        time.sleep(1)


if __name__ == "__main__":
    main()
