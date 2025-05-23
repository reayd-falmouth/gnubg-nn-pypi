import pytest
import gnubg

@pytest.fixture
def version_module():
    # Import the version module from within gnubg package
    import gnubg.__version__ as version
    return version

def test_version_attributes(version_module):
    assert hasattr(version_module, "version"), "version attribute is missing"
    assert hasattr(version_module, "__version__"), "__version__ attribute is missing"
    assert hasattr(version_module, "full_version"), "full_version attribute is missing"
    assert hasattr(version_module, "git_revision"), "git_revision attribute is missing"
    assert hasattr(version_module, "release"), "release attribute is missing"
    assert hasattr(version_module, "short_version"), "short_version attribute is missing"

def test_version_format(version_module):
    import re
    pattern = r'^\d+\.\d+\.\d+(\.dev\d+)?(\+git\d{8}\.[a-f0-9]{7})?$'
    assert re.match(pattern, version_module.version), (
        f"Version '{version_module.version}' does not match expected format"
    )

def test_git_revision(version_module):
    git_rev = version_module.git_revision
    assert len(git_rev) == 40 or len(git_rev) == 0, "git_revision must be full hash or empty"
    if git_rev:
        assert all(c in '0123456789abcdef' for c in git_rev), "git_revision must be hexadecimal"
