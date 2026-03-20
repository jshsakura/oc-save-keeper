First Stable Release — Reliable Save Backup and Cloud Sync

## Key Features

### Cloud Sync (Dropbox)
- QR code-based Dropbox authentication (optional bridge service)
- Per-account/device save separation
- Device priority-based conflict resolution
- Instant cloud list refresh with X button
- Improved Dropbox integration completion message

### Local Backup
- JKSV-style safe restore (auto-rollback support)
- ZIP integrity verification before restore
- Automatic _autosave suffix handling
- Trash bin (30-day retention)
- Up to 100 version history

### UI/UX
- Catppuccin Mocha theme
- Korean/English localization
- Main screen sort modes: Name / Install / Saved
- Default sort changed to Install order (newest first)
- Improved footer button hints (added - Sort)
- Loading popup for long list queries
- Sidebar at 40% width for better readability
- Smooth scrolling and animations

### Security
- SHA-512 hash integrity verification
- SSRF protection (URL whitelist)
- ZIP path traversal attack prevention
- HTTP request limits

## Included

- oc-save-keeper.nro
- Korean/English UI strings
- Nintendo Switch shared font rendering
- Metadata-based cloud sync
- Device priority conflict handling
- Local version history and restore

## Installation Paths

/switch/oc-save-keeper/
├── oc-save-keeper.nro
├── backups/          # Local backup storage
├── logs/             # Log files
├── temp/             # Temporary files
└── config/
    ├── dropbox_auth.json
    └── settings.json  # device_id, device_priority

## Controls

| Button | Main Screen | Revision Screen |
|--------|-------------|-----------------|
| A      | Open/Action | Restore/Download|
| B      | Exit        | Close           |
| X      | Refresh     | Refresh         |
| Y      | Language    | Language        |
| L      | Change User | Change User     |
| R      | Cloud Setup | Cloud Setup     |
| -      | Sort Mode   | Delete          |

## Known Limitations

- Cloud version browsing is simple date-based; detailed view not yet implemented
- Conflict resolution uses automatic decision + reason text display
- Dropbox integration requires a third-party bridge server; self-hosting is the only alternative
- Dropbox refresh token is stored on device; exercise caution when transferring device (deletion recommended)

## Testing Checklist

1. App launch and font rendering
2. Title scan and game list
3. Local backup creation
4. Dropbox connection and cloud upload
5. Cloud download and restore
6. Delete and trash bin restore
7. Sort mode toggle (- button)
