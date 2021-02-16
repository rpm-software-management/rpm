# Run tests using pytest, e.g. from the root directory
#   $ python3 -m pytest --ignore tests/testing/ -s -vvv
#
# If there are any breakags, the best way to see differences is using a diff:
#   $ diff tests/data/scripts_pythondistdeps/test-data.yaml <(python3 tests/test_scripts_pythondistdeps.py)
#
#   - Test cases and expected results are saved in test-data.yaml inside
#     TEST_DATA_PATH (currently ./data/scripts_pythondistdeps/)
#   - To regenerate test-data.yaml file with the current results of
#     pythondistdeps.py for each test configuration, execute this test file
#     directly and results will be on stdout
#       $ python3 test_scripts_pythondistdeps.py
#
# To add new test-data, add them to the test-requires.yaml: they will be
# downloaded automatically. And then add the resulting dist-info/egg-info paths
# into test-data.yaml under whichever requires/provides configurations you want
# to test
# - To find all dist-info/egg-info directories in the test-data directory,
#   run inside test-data:
#     $ find . -type d -regex ".*\(dist-info\|egg-info\)" | sort
#
# Requirements for this script:
# - Python >= 3.6
# - pip >= 20.0.1
# - setuptools
# - pytest
# - pyyaml
# - wheel


from pathlib import Path
import pytest
import shlex
import shutil
import subprocess
import sys
import tempfile
import yaml

PYTHONDISTDEPS_PATH = Path(__file__).parent / '..' / 'scripts' / 'pythondistdeps.py'
TEST_DATA_PATH = Path(__file__).parent / 'data' / 'scripts_pythondistdeps'



def run_pythondistdeps(provides_params, requires_params, dist_egg_info_path, expect_failure=False):
    """Runs pythondistdeps.py on `dits_egg_info_path` with given
    provides and requires parameters and returns a dict with generated provides and requires"""
    info_path = TEST_DATA_PATH / dist_egg_info_path
    files = '\n'.join(map(str, info_path.iterdir()))

    provides = subprocess.run((sys.executable, PYTHONDISTDEPS_PATH, *shlex.split(provides_params)),
            input=files, capture_output=True, check=False, encoding="utf-8")
    requires = subprocess.run((sys.executable, PYTHONDISTDEPS_PATH, *shlex.split(requires_params)),
            input=files, capture_output=True, check=False, encoding="utf-8")

    if expect_failure:
        if provides.returncode == 0 or requires.returncode == 0:
            raise RuntimeError(f"pythondistdeps.py did not exit with a non-zero code as expected.\n"
                               f"Used parameters: ({provides_params}, {requires_params}, {dist_egg_info_path})")
        stdout = {"provides": provides.stdout.strip(), "requires": requires.stdout.strip()}
        stderr = {"provides": provides.stderr.strip(), "requires": requires.stderr.strip()}
        return {"stderr": stderr, "stdout": stdout}

    else:
        if provides.returncode != 0 or requires.returncode != 0:
            raise RuntimeError(f"pythondistdeps.py unexpectedly exited with a non-zero code.\n"
                               f"Used parameters: ({provides_params}, {requires_params}, {dist_egg_info_path})")
        return {"provides": provides.stdout.strip(), "requires": requires.stdout.strip()}


def load_test_data():
    """Reads the test-data.yaml and loads the test data into a dict."""
    with TEST_DATA_PATH.joinpath('test-data.yaml').open() as file:
        return yaml.safe_load(file)


def generate_test_cases(test_data):
    """Goes through the test data dict and yields test cases.
    Test case is a tuple of 4 elements:
        - provides parameters
        - requires parameters
        - path to the dist-info/egg-info directory inside test-data
        - dict with expected results ("requires" and "provides")"""
    for requires_params in test_data:
        for provides_params in test_data[requires_params]:
            for dist_egg_info_path in test_data[requires_params][provides_params]:
                expected = test_data[requires_params][provides_params][dist_egg_info_path]
                yield (provides_params, requires_params, dist_egg_info_path, expected)


def check_and_install_test_data():
    """Checks if the appropriate metadata are present in TEST_DATA_PATH, and if
       not, downloads them through pip from PyPI."""
    with TEST_DATA_PATH.joinpath('test-requires.yaml').open() as file:
        test_requires = yaml.safe_load(file)
        downloaded_anything = False

        for package in test_requires:
            # To be as close to the real environment, we want some packages saved in /usr/lib64 instead of /usr/lib,
            # for these we explicitly set lib64 as a parameter, and by default we use /usr/lib.
            lib = test_requires[package].pop("lib", "lib")

            # type is either `wheel` or `sdist`
            for type in test_requires[package]:
                for pkg_version in test_requires[package][type]:
                    for py_version in test_requires[package][type][pkg_version]:
                        py_version_nodots = py_version.replace(".", "")
                        package_underscores = package.replace("-", "_")

                        suffix = ".egg-info" if type == "sdist" else ".dist-info"
                        pre_suffix = f"-py{py_version}" if type == "sdist" else ""

                        install_path = TEST_DATA_PATH / "usr" / lib / f"python{py_version}" \
                                / "site-packages" / f"{package_underscores}-{pkg_version}{pre_suffix}{suffix}"

                        if install_path.exists():
                            continue

                        # If this is the first package we're downloading,
                        # display what's happening
                        if not downloaded_anything:
                            print("=====================")
                            print("Downloading test data")
                            print("=====================\n")
                            downloaded_anything = True

                        # We use a temporary directory to unpack/install the
                        # package to, and then we move only the metadata to the
                        # final location
                        with tempfile.TemporaryDirectory() as temp_dir:
                            import runpy
                            backup_argv = sys.argv[:]

                            if type == "wheel":
                                from pkg_resources import parse_version
                                abi = f"cp{py_version_nodots}"
                                # The "m" was removed from the abi flag in Python version 3.8
                                if parse_version(py_version) < parse_version('3.8'):
                                    abi += "m"

                                # Install = download and unpack wheel into our
                                #   temporary directory
                                sys.argv[1:] = ["install", "--no-deps",
                                        "--only-binary", ":all:",
                                        "--platform", "manylinux1_x86_64",
                                        "--python-version", py_version,
                                        "--implementation", "cp",
                                        "--abi", abi,
                                        "--target", temp_dir,
                                        "--no-build-isolation",
                                        f"{package}=={pkg_version}"]
                            else:
                                # Download sdist that we'll unpack later
                                sys.argv[1:] = ["download", "--no-deps",
                                        "--no-binary", ":all:",
                                        "--dest", temp_dir,
                                        "--no-build-isolation",
                                        f"{package}=={pkg_version}"]

                            try:
                                # run_module() alters sys.modules and sys.argv, but restores them at exit
                                runpy.run_module("pip", run_name="__main__", alter_sys=True)
                            except SystemExit as exc:
                                pass
                            finally:
                                sys.argv[:] = backup_argv

                            temp_path = Path(temp_dir)
                            if type == "sdist":
                                # Wheel were already unpacked by pip, sdists we
                                # have to unpack ourselves
                                sdist_path = next(temp_path.glob(f"{package}-{pkg_version}.*"))

                                if sdist_path.suffix == ".zip":
                                    import zipfile
                                    archive = zipfile.ZipFile(sdist_path)
                                else:
                                    import tarfile
                                    archive = tarfile.open(sdist_path)

                                archive.extractall(temp_path)
                            try:
                                info_path = next(temp_path.glob(f"**/*{suffix}"))

                                # Let's check the wheel metadata has the
                                # expected directory name. We don't check for
                                # egg-info metadata, because we're pulling them
                                # from sdists where they don't have the proper
                                # directory name
                                if type == "wheel":
                                    if info_path.name != install_path.name:
                                        print("\nWarning: wheel metadata have unexpected directory name.\n"
                                              f"Expected: {install_path.name}\n"
                                              f"Actual: {info_path.name}\n"
                                              f"Info: package '{package}', version '{pkg_version}'"
                                              f" for Python {py_version}\n"
                                              f"Possible resolution: Specify the package version with"
                                              f" trailing zeros in test-requires.yaml", file=sys.stderr)

                                shutil.move(info_path, install_path)

                                relative_path = install_path.relative_to(TEST_DATA_PATH)
                                print(f"\nDownloaded metadata to '{relative_path}'" \
                                        f" inside test-data directory.\n")
                            except StopIteration:
                                # temp_path.glob() did not find any file and
                                # thus there's been some problem
                                sys.exit(f"Problem occured while getting dist-info/egg-info"
                                        f" for package '{package}', version '{pkg_version}'"
                                        f" for Python {py_version}")
        if downloaded_anything:
            print("\n==============================")
            print("Finished downloading test data")
            print("==============================")


@pytest.fixture(scope="session", autouse=True)
def fixture_check_and_install_test_data():
    """Wrapper fixture, because a fixture can't be called as a function."""
    check_and_install_test_data()


@pytest.mark.parametrize("provides_params, requires_params, dist_egg_info_path, expected",
        generate_test_cases(load_test_data()))
def test_pythondistdeps(provides_params, requires_params, dist_egg_info_path, expected):
    """Runs pythondistdeps with the given parameters and dist-info/egg-info
    path, compares the results with the expected results"""
    expect_failure = "stderr" in expected
    assert expected == run_pythondistdeps(provides_params, requires_params, dist_egg_info_path, expect_failure)


if __name__ == "__main__":
    """If the script is called directly, we check and install test data if needed,
    we look up all the test configurations in test-data.yaml, run
    pythondistdeps for each, save the results and print the resulting YAML file
    with the updated results."""

    check_and_install_test_data()

    # Set YAML dump style to block style
    def str_presenter(dumper, data):
        if len(data.splitlines()) > 1:  # check for multiline string
            return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='|')
        return dumper.represent_scalar('tag:yaml.org,2002:str', data)
    yaml.add_representer(str, str_presenter)

    # Run pythondistdeps for each test configuration
    test_data = load_test_data()
    for provides_params, requires_params, dist_egg_info_path, expected in generate_test_cases(test_data):
        # Print a dot to stderr for each test run to keep user informed about progress
        print(".", end="", flush=True, file=sys.stderr)

        expect_failure = "stderr" in test_data[requires_params][provides_params][dist_egg_info_path]
        test_data[requires_params][provides_params][dist_egg_info_path] = \
            run_pythondistdeps(provides_params, requires_params, dist_egg_info_path, expect_failure)

    print(yaml.dump(test_data, indent=4))

