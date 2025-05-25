import pytest
import gnubg

VERSION_PATTERN = r"""
    v?
    (?:
        (?:(?P<epoch>[0-9]+)!)?                           # epoch
        (?P<release>[0-9]+(?:\.[0-9]+)*)                  # release segment
        (?P<pre>                                          # pre-release
            [-_\.]?
            (?P<pre_l>(a|b|c|rc|alpha|beta|pre|preview))
            [-_\.]?
            (?P<pre_n>[0-9]+)?
        )?
        (?P<post>                                         # post release
            (?:-(?P<post_n1>[0-9]+))
            |
            (?:
                [-_\.]?
                (?P<post_l>post|rev|r)
                [-_\.]?
                (?P<post_n2>[0-9]+)?
            )
        )?
        (?P<dev>                                          # dev release
            [-_\.]?
            (?P<dev_l>dev)
            [-_\.]?
            (?P<dev_n>[0-9]+)?
        )?
    )
    (?:\+(?P<local>[a-z0-9]+(?:[-_\.][a-z0-9]+)*))?       # local version
"""

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
    assert re.match(VERSION_PATTERN, version_module.version), (
        f"Version '{version_module.version}' does not match expected format"
    )

def test_git_revision(version_module):
    git_rev = version_module.git_revision
    assert len(git_rev) == 40 or len(git_rev) == 0, "git_revision must be full hash or empty"
    if git_rev:
        assert all(c in '0123456789abcdef' for c in git_rev), "git_revision must be hexadecimal"

def test_version_values_match():
    import gnubg
    import gnubg.__version__ as v

    assert gnubg.version == v.version
    assert gnubg.__version__ == v.__version__
    assert gnubg.full_version == v.full_version
    assert gnubg.git_revision == v.git_revision
    assert gnubg.release == v.release
    assert gnubg.short_version == v.short_version

