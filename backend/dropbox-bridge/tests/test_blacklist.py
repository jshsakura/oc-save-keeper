"""
Tests for IP Blacklist Service.
"""

import asyncio
import os
import tempfile
from pathlib import Path

import pytest

# Import the module under test
import sys
sys.path.insert(0, str(Path(__file__).parent.parent / "app"))

from services.blacklist import (
    is_blacklisted,
    add_to_blacklist,
    get_blacklist,
    _validate_ip,
    _ensure_blacklist_file,
)


class TestIPValidation:
    """Test IP address validation."""

    def test_valid_ipv4(self):
        """Valid IPv4 addresses should pass."""
        assert _validate_ip("192.168.1.1") is True
        assert _validate_ip("10.0.0.1") is True
        assert _validate_ip("172.16.0.1") is True
        assert _validate_ip("255.255.255.255") is True
        assert _validate_ip("0.0.0.0") is True

    def test_invalid_ipv4(self):
        """Invalid IPv4 addresses should fail."""
        assert _validate_ip("256.1.1.1") is False
        assert _validate_ip("192.168.1") is False
        assert _validate_ip("192.168.1.1.1") is False
        assert _validate_ip("192.168.1.-1") is False
        assert _validate_ip("192.168.1.a") is False

    def test_valid_ipv6(self):
        """Valid IPv6 addresses should pass."""
        assert _validate_ip("::1") is True
        assert _validate_ip("2001:0db8:85a3:0000:0000:8a2e:0370:7334") is True
        assert _validate_ip("::") is True

    def test_localhost(self):
        """Localhost variants should pass."""
        assert _validate_ip("127.0.0.1") is True
        assert _validate_ip("localhost") is True
        assert _validate_ip("::1") is True

    def test_empty_and_none(self):
        """Empty and None should fail."""
        assert _validate_ip("") is False
        assert _validate_ip(None) is False
        assert _validate_ip("   ") is False

    def test_xss_attempts(self):
        """XSS injection attempts should fail."""
        assert _validate_ip("<script>alert(1)</script>") is False
        assert _validate_ip("192.168.1.1\n10.0.0.1") is False
        assert _validate_ip("192.168.1.1; rm -rf /") is False


class TestBlacklistFileOperations:
    """Test file-based blacklist operations."""

    @pytest.fixture
    def temp_blacklist_file(self, tmp_path):
        """Create a temporary blacklist file for testing."""
        test_file = tmp_path / "test_blacklist.txt"
        os.environ["BLACKLIST_FILE"] = str(test_file)
        yield test_file
        # Cleanup
        if test_file.exists():
            test_file.unlink()
        os.environ.pop("BLACKLIST_FILE", None)

    @pytest.mark.asyncio
    async def test_ensure_blacklist_file_creates_directory(self, tmp_path):
        """Should create parent directory if it doesn't exist."""
        test_file = tmp_path / "subdir" / "blacklist.txt"
        os.environ["BLACKLIST_FILE"] = str(test_file)
        
        result = await _ensure_blacklist_file()
        
        assert result.exists()
        assert result.parent.exists()
        
        # Cleanup
        test_file.unlink(missing_ok=True)
        test_file.parent.rmdir()
        os.environ.pop("BLACKLIST_FILE", None)

    @pytest.mark.asyncio
    async def test_ensure_blacklist_file_permissions(self, tmp_path):
        """Should create file with 0600 permissions."""
        test_file = tmp_path / "secure_blacklist.txt"
        os.environ["BLACKLIST_FILE"] = str(test_file)
        
        result = await _ensure_blacklist_file()
        
        # Check file permissions (0600 = owner read/write only)
        stat_info = result.stat()
        mode = stat_info.st_mode & 0o777
        assert mode == 0o600, f"Expected 0o600, got {oct(mode)}"
        
        # Cleanup
        test_file.unlink(missing_ok=True)
        os.environ.pop("BLACKLIST_FILE", None)

    @pytest.mark.asyncio
    async def test_add_to_blacklist(self, temp_blacklist_file):
        """Should add IP to blacklist."""
        await add_to_blacklist("192.168.1.100", "Test violation")
        
        content = temp_blacklist_file.read_text()
        assert "192.168.1.100" in content
        assert "Test violation" in content

    @pytest.mark.asyncio
    async def test_add_to_blacklist_prevents_duplicates(self, temp_blacklist_file):
        """Should not add duplicate IPs."""
        await add_to_blacklist("192.168.1.100", "First")
        await add_to_blacklist("192.168.1.100", "Second")
        
        content = temp_blacklist_file.read_text()
        assert content.count("192.168.1.100") == 1

    @pytest.mark.asyncio
    async def test_is_blacklisted(self, temp_blacklist_file):
        """Should correctly identify blacklisted IPs."""
        # Initially not blacklisted
        assert await is_blacklisted("192.168.1.100") is False
        
        # Add to blacklist
        await add_to_blacklist("192.168.1.100", "Test")
        
        # Now blacklisted
        assert await is_blacklisted("192.168.1.100") is True
        assert await is_blacklisted("192.168.1.101") is False

    @pytest.mark.asyncio
    async def test_is_blacklisted_ignores_comments(self, temp_blacklist_file):
        """Should ignore comment-only lines."""
        temp_blacklist_file.write_text("# This is a comment\n192.168.1.100  # blocked\n")
        
        assert await is_blacklisted("192.168.1.100") is True
        assert await is_blacklisted("# This is a comment") is False

    @pytest.mark.asyncio
    async def test_get_blacklist(self, temp_blacklist_file):
        """Should return list of blacklisted IPs."""
        await add_to_blacklist("192.168.1.100", "Test 1")
        await add_to_blacklist("192.168.1.101", "Test 2")
        
        ips = await get_blacklist()
        
        assert "192.168.1.100" in ips
        assert "192.168.1.101" in ips
        assert len(ips) == 2

    @pytest.mark.asyncio
    async def test_rejects_invalid_ip(self, temp_blacklist_file):
        result = await add_to_blacklist("invalid-ip", "Test")
        assert result is False

    @pytest.mark.asyncio
    async def test_concurrent_access(self, temp_blacklist_file):
        """Should handle concurrent writes safely."""
        ips = [f"192.168.1.{i}" for i in range(10)]
        
        # Concurrent writes
        await asyncio.gather(*[
            add_to_blacklist(ip, f"Concurrent test {i}")
            for i, ip in enumerate(ips)
        ])
        
        # All IPs should be in blacklist
        content = temp_blacklist_file.read_text()
        for ip in ips:
            assert ip in content


class TestBlacklistEdgeCases:
    """Test edge cases and error handling."""

    @pytest.mark.asyncio
    async def test_empty_blacklist_file(self, tmp_path):
        """Empty file should return empty list."""
        test_file = tmp_path / "empty.txt"
        test_file.touch()
        os.environ["BLACKLIST_FILE"] = str(test_file)
        
        ips = await get_blacklist()
        assert ips == []
        
        os.environ.pop("BLACKLIST_FILE", None)

    @pytest.mark.asyncio
    async def test_whitespace_handling(self, tmp_path):
        """Should handle extra whitespace in file."""
        test_file = tmp_path / "whitespace.txt"
        test_file.write_text("  192.168.1.100  \n\n  192.168.1.101  # comment  \n")
        os.environ["BLACKLIST_FILE"] = str(test_file)
        
        assert await is_blacklisted("192.168.1.100") is True
        assert await is_blacklisted("192.168.1.101") is True
        
        os.environ.pop("BLACKLIST_FILE", None)
