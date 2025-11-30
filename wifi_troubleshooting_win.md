# WiFi Connection Troubleshooting Guide

## Problem: Linux can't discover Windows device via WiFi

### Symptoms
- Windows can see Linux peers (`wifi peers` shows Linux device)
- Linux can't see Windows peers (`wifi peers` shows nothing)
- Messages sent from Windows appear on Linux
- Messages sent from Linux don't appear on Windows

### Root Cause
Windows Firewall is blocking:
1. **UDP port 48270** (peer discovery broadcasts)
2. **TCP port 48271** (message reception)

### Solution: Configure Windows Firewall

#### Option 1: Quick Test (Temporary - Disable Firewall)
**WARNING: Only for testing on trusted networks**

```powershell
# Run PowerShell as Administrator
# Turn off firewall temporarily
Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled False

# Test Echo communication

# Turn firewall back on when done
Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled True
```

#### Option 2: Proper Fix (Add Firewall Rules)
**Recommended for permanent use**

```powershell
# Run PowerShell as Administrator

# Allow UDP port 48270 (peer discovery)
New-NetFirewallRule -DisplayName "Echo WiFi Discovery" `
    -Direction Inbound `
    -Protocol UDP `
    -LocalPort 48270 `
    -Action Allow `
    -Profile Private,Public

# Allow TCP port 48271 (message reception)
New-NetFirewallRule -DisplayName "Echo WiFi Messaging" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 48271 `
    -Action Allow `
    -Profile Private,Public
```

#### Option 3: GUI Method (Windows Defender Firewall)

1. Open **Windows Defender Firewall with Advanced Security**
2. Click **Inbound Rules** → **New Rule**
3. Select **Port** → Next
4. Select **UDP**, enter port **48270** → Next
5. Select **Allow the connection** → Next
6. Check all profiles (Domain, Private, Public) → Next
7. Name it "Echo WiFi Discovery" → Finish

Repeat for TCP port 48271 (name it "Echo WiFi Messaging")

### Verification

After configuring firewall:

**On Windows:**
```
wifi status
```
Should show:
- Local IP (e.g., 192.168.1.100)
- TCP Port: 48271
- Peers discovered: 1 (should show Linux device)

**On Linux:**
```
wifi status
```
Should show:
- Local IP (e.g., 192.168.1.101)
- TCP Port: 48271
- Peers discovered: 1 (should show Windows device)

### Testing

1. **Start verbose mode:**
   ```
   wifi start
   ```

2. **Check peer discovery:**
   ```
   wifi status
   ```

3. **Test messaging in global chat:**
   ```
   /join #global
   hello from [your-device]
   ```

### Common Issues

**Problem:** `wifi status` shows 0 peers
**Solution:**
- Check both devices are on same network
- Verify Windows firewall rules are active
- Try `wifi stop` then `wifi start` to restart discovery

**Problem:** Peers discovered but messages not received
**Solution:**
- Check TCP port 48271 is open on Windows
- Verify router isn't blocking local network traffic
- Some routers have "AP Isolation" - disable it

**Problem:** Works on private network but not public WiFi
**Solution:**
- Public WiFi often blocks peer-to-peer communication
- Use private network or hotspot instead

### Network Requirements

- **Same subnet:** Both devices must be on same local network (192.168.x.x)
- **No AP Isolation:** Router must allow device-to-device communication
- **Firewall:** Must allow UDP 48270 and TCP 48271 inbound