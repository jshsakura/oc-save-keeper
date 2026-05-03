"""
IP Blacklist Service for Security Monitoring.

Provides file-based IP blacklist with thread-safe operations.
Blacklist persists across restarts via mounted volume.
"""

import asyncio
import logging
import os
import re
from pathlib import Path
from typing import Optional

logger = logging.getLogger("bridge.blacklist")

# Default blacklist file path
DEFAULT_BLACKLIST_FILE = "/app/data/blacklist.txt"

# IP validation regex (IPv4)
IPV4_PATTERN = re.compile(
    r"^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}"
    r"(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
)

# IPv6 pattern (simplified - matches common formats)
IPV6_PATTERN = re.compile(
    r"^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|"
    r"^(?:[0-9a-fA-F]{1,4}:){1,7}:$|"
    r"^(?:[0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}$|"
    r"^::(?:[0-9a-fA-F]{1,4}:){0,5}[0-9a-fA-F]{1,4}$|"
    r"^(?:[0-9a-fA-F]{1,4}:){1,5}:(?::[0-9a-fA-F]{1,4}){1,2}$|"
    r"^::$|"
    r"^::1$"
)

# Global lock for thread-safe file operations
_blacklist_lock = asyncio.Lock()


def _get_blacklist_file_path() -> Path:
    """Get the blacklist file path from environment or default."""
    path = os.getenv("BLACKLIST_FILE", DEFAULT_BLACKLIST_FILE)
    return Path(path)


def _validate_ip(ip: str) -> bool:
    """
    Validate IP address format (IPv4 or IPv6).
    
    Args:
        ip: IP address string to validate
        
    Returns:
        True if valid IP format, False otherwise
    """
    if not ip or not isinstance(ip, str):
        return False
    
    ip = ip.strip()
    
    # Check IPv4
    if IPV4_PATTERN.match(ip):
        return True
    
    # Check IPv6 (simplified check)
    if IPV6_PATTERN.match(ip):
        return True
    
    # Also allow localhost for testing
    if ip in ("127.0.0.1", "::1", "localhost"):
        return True
    
    return False


async def _ensure_blacklist_file() -> Path:
    """
    Ensure the blacklist file and directory exist.
    
    Returns:
        Path to the blacklist file
    """
    file_path = _get_blacklist_file_path()
    
    # Create directory if it doesn't exist
    file_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Create file with secure permissions if it doesn't exist
    if not file_path.exists():
        file_path.touch(mode=0o600)
        logger.info(f"Created blacklist file: {file_path}")
    
    return file_path


async def is_blacklisted(ip: str) -> bool:
    """
    Check if an IP address is blacklisted.
    
    Args:
        ip: IP address to check
        
    Returns:
        True if IP is in blacklist, False otherwise
    """
    if not _validate_ip(ip):
        logger.warning(f"Invalid IP format for blacklist check: {ip}")
        return False
    
    try:
        file_path = await _ensure_blacklist_file()
        
        async with _blacklist_lock:
            with open(file_path, "r") as f:
                blacklist = set()
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        entry_ip = line.split("#")[0].strip()
                        if entry_ip:
                            blacklist.add(entry_ip)
        
        return ip in blacklist
    except OSError as e:
        logger.error(f"Failed to read blacklist file: {e}")
        return False  # Fail-open for availability


async def add_to_blacklist(ip: str, reason: str = "") -> bool:
    """
    Add an IP address to the blacklist.
    
    Args:
        ip: IP address to add
        reason: Reason for blacklisting (stored as comment)
        
    Returns:
        True if IP was added successfully, False otherwise
    """
    if not _validate_ip(ip):
        logger.warning(f"Invalid IP format for blacklist add: {ip}")
        return False
    
    try:
        file_path = await _ensure_blacklist_file()
        
        async with _blacklist_lock:
            existing = set()
            if file_path.exists():
                with open(file_path, "r") as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#"):
                            entry_ip = line.split("#")[0].strip()
                            if entry_ip:
                                existing.add(entry_ip)
            
            # Check if already blacklisted
            if ip in existing:
                logger.debug(f"IP {ip} already in blacklist")
                return True
            
            # Append new entry with reason as comment
            entry = f"{ip}"
            if reason:
                # Sanitize reason (remove newlines)
                safe_reason = reason.replace("\n", " ").replace("\r", " ")[:100]
                entry = f"{ip}  # {safe_reason}"
            
            with open(file_path, "a") as f:
                f.write(f"{entry}\n")
            
            logger.info(f"Added IP {ip} to blacklist: {reason}")
            return True
    except OSError as e:
        logger.error(f"Failed to add IP to blacklist: {e}")
        return False


async def get_blacklist() -> list[str]:
    """
    Get all blacklisted IP addresses.
    
    Returns:
        List of blacklisted IP addresses
    """
    try:
        file_path = await _ensure_blacklist_file()
        
        async with _blacklist_lock:
            if not file_path.exists():
                return []
            
            ips = []
            with open(file_path, "r") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        entry_ip = line.split("#")[0].strip()
                        if entry_ip:
                            ips.append(entry_ip)
            return ips
    except OSError as e:
        logger.error(f"Failed to read blacklist file: {e}")
        return []


async def remove_from_blacklist(ip: str) -> bool:
    """
    Remove an IP address from the blacklist.
    
    Args:
        ip: IP address to remove
        
    Returns:
        True if IP was removed successfully, False otherwise
    """
    if not _validate_ip(ip):
        logger.warning(f"Invalid IP format for blacklist remove: {ip}")
        return False
    
    try:
        file_path = await _ensure_blacklist_file()
        
        async with _blacklist_lock:
            if not file_path.exists():
                return True
            
            # Read all lines
            with open(file_path, "r") as f:
                lines = f.readlines()
            
            # Filter out the IP
            new_lines = []
            removed = False
            for line in lines:
                stripped = line.strip()
                if stripped and not stripped.startswith("#"):
                    # Extract IP (before any comment)
                    entry_ip = stripped.split("#")[0].strip()
                    if entry_ip == ip:
                        removed = True
                        continue
                new_lines.append(line)
            
            if removed:
                # Write back
                with open(file_path, "w") as f:
                    f.writelines(new_lines)
                logger.info(f"Removed IP {ip} from blacklist")
            
            return True
    except OSError as e:
        logger.error(f"Failed to remove IP from blacklist: {e}")
        return False
