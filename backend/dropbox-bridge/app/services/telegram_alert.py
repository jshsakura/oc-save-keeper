"""
Telegram Alert Service for Security Monitoring.

Sends security alerts to a Telegram chat when suspicious activity is detected.
Supports spam prevention via Redis-backed cooldown.
"""

import asyncio
import json
import logging
import os
from datetime import datetime
from typing import Any

import aiohttp
from redis.asyncio import Redis
from redis.exceptions import RedisError

logger = logging.getLogger("bridge.telegram")

# Event type emojis for better visual identification
EVENT_EMOJIS = {
    "attack_detected": "🚨",
    "auth_failure": "🔐",
    "blocked_ip": "🚫",
    "suspicious_pattern": "⚠️",
}

# Default cooldown: 5 minutes between alerts for same IP
DEFAULT_ALERT_COOLDOWN_SECONDS = 300


def _get_telegram_credentials() -> tuple[str | None, str | None]:
    """
    Get Telegram bot token and chat ID from environment or Docker secrets.
    
    Priority:
    1. Environment variables: TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID
    2. Docker Secrets: /run/secrets/telegram_bot_token, /run/secrets/telegram_chat_id
    
    Returns:
        Tuple of (bot_token, chat_id) or (None, None) if not configured
    """
    # Try environment variables first
    bot_token = os.getenv("TELEGRAM_BOT_TOKEN", "").strip()
    chat_id = os.getenv("TELEGRAM_CHAT_ID", "").strip()
    
    # Try Docker secrets if env vars not set
    if not bot_token:
        secret_path = "/run/secrets/telegram_bot_token"
        if os.path.exists(secret_path):
            try:
                with open(secret_path, "r") as f:
                    bot_token = f.read().strip()
            except OSError as e:
                logger.warning(f"Failed to read telegram_bot_token secret: {e}")
    
    if not chat_id:
        secret_path = "/run/secrets/telegram_chat_id"
        if os.path.exists(secret_path):
            try:
                with open(secret_path, "r") as f:
                    chat_id = f.read().strip()
            except OSError as e:
                logger.warning(f"Failed to read telegram_chat_id secret: {e}")
    
    # Return None if either is empty
    if not bot_token or not chat_id:
        return None, None
    
    return bot_token, chat_id


async def _is_alert_on_cooldown(redis: Redis, ip: str, cooldown_seconds: int) -> bool:
    """
    Check if an alert for this IP is still on cooldown.
    
    Args:
        redis: Redis client instance
        ip: Client IP address
        cooldown_seconds: Cooldown period in seconds
        
    Returns:
        True if alert should be suppressed (on cooldown), False otherwise
    """
    try:
        key = f"telegram:cooldown:{ip}"
        return await redis.exists(key) > 0
    except RedisError as e:
        logger.error(f"Failed to check alert cooldown: {e}")
        return False  # Fail-open: allow alert if Redis fails


async def _set_alert_cooldown(redis: Redis, ip: str, cooldown_seconds: int) -> None:
    """
    Set cooldown for alerts from this IP.
    
    Args:
        redis: Redis client instance
        ip: Client IP address
        cooldown_seconds: Cooldown period in seconds
    """
    try:
        key = f"telegram:cooldown:{ip}"
        await redis.setex(key, cooldown_seconds, "1")
    except RedisError as e:
        logger.error(f"Failed to set alert cooldown: {e}")


def _format_alert_message(
    event_type: str,
    ip: str,
    details: dict[str, Any],
    request_info: dict[str, str] | None = None,
) -> str:
    """
    Format the alert message for Telegram.
    
    Args:
        event_type: Type of security event
        ip: Client IP address
        details: Additional event details
        request_info: Optional request metadata (path, method, user_agent)
        
    Returns:
        Formatted message string
    """
    emoji = EVENT_EMOJIS.get(event_type, "🔔")
    timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
    
    lines = [
        f"{emoji} *Security Alert*",
        f"",
        f"*Type:* `{event_type}`",
        f"*IP:* `{ip}`",
        f"*Time:* `{timestamp}`",
    ]
    
    if details:
        lines.append("")
        lines.append("*Details:*")
        for key, value in details.items():
            # Escape special markdown characters
            value_str = str(value).replace("_", "\\_").replace("*", "\\*").replace("`", "\\`")
            lines.append(f"  • {key}: `{value_str}`")
    
    if request_info:
        lines.append("")
        lines.append("*Request:*")
        if "path" in request_info:
            lines.append(f"  • Path: `{request_info['path']}`")
        if "method" in request_info:
            lines.append(f"  • Method: `{request_info['method']}`")
        if "user_agent" in request_info:
            # Truncate long user agents
            ua = request_info['user_agent'][:100]
            lines.append(f"  • UA: `{ua}`")
    
    return "\n".join(lines)


async def send_security_alert(
    redis: Redis,
    event_type: str,
    ip: str,
    details: dict[str, Any] | None = None,
    request=None,
    cooldown_seconds: int | None = None,
) -> bool:
    """
    Send a security alert to Telegram.
    
    Args:
        redis: Redis client instance for cooldown tracking
        event_type: Type of security event (attack_detected, auth_failure, blocked_ip, suspicious_pattern)
        ip: Client IP address
        details: Additional event details
        request: Optional FastAPI Request object for extracting request metadata
        cooldown_seconds: Cooldown period (default from env or 300s)
        
    Returns:
        True if alert was sent successfully, False otherwise
    """
    # Get credentials
    bot_token, chat_id = _get_telegram_credentials()
    if not bot_token or not chat_id:
        logger.debug("Telegram credentials not configured, skipping alert")
        return False
    
    # Use default cooldown if not specified
    if cooldown_seconds is None:
        cooldown_seconds = int(os.getenv("ALERT_COOLDOWN_SECONDS", str(DEFAULT_ALERT_COOLDOWN_SECONDS)))
    
    # Check cooldown (spam prevention)
    if await _is_alert_on_cooldown(redis, ip, cooldown_seconds):
        logger.debug(f"Alert for IP {ip} suppressed (cooldown active)")
        return False
    
    # Extract request info if provided
    request_info = None
    if request:
        request_info = {
            "path": str(request.url.path),
            "method": request.method,
            "user_agent": request.headers.get("user-agent", "unknown"),
        }
    
    # Format message
    message = _format_alert_message(event_type, ip, details or {}, request_info)
    
    # Send to Telegram API
    url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
    payload = {
        "chat_id": chat_id,
        "text": message,
        "parse_mode": "Markdown",
    }
    
    try:
        timeout = aiohttp.ClientTimeout(total=10)
        async with aiohttp.ClientSession(timeout=timeout) as session:
            async with session.post(url, json=payload) as response:
                if response.status == 200:
                    # Set cooldown on success
                    await _set_alert_cooldown(redis, ip, cooldown_seconds)
                    logger.info(f"Security alert sent: {event_type} from {ip}")
                    return True
                else:
                    response_text = await response.text()
                    logger.error(f"Telegram API error: {response.status} - {response_text}")
                    return False
    except asyncio.TimeoutError:
        logger.error("Telegram API request timed out")
        return False
    except aiohttp.ClientError as e:
        logger.error(f"Telegram API request failed: {e}")
        return False
    except Exception as e:
        logger.error(f"Unexpected error sending Telegram alert: {e}")
        return False
