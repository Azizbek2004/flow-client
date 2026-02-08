# FLOW VAC Safety Guarantees

## How FLOW Works
FLOW is a **transparent proxy** that routes your CS2 traffic through optimized servers.

## What FLOW Does NOT Do
✗ Modify CS2 game files  
✗ Inject code into CS2 process  
✗ Intercept or modify game packets  
✗ Hook Windows APIs  
✗ Install kernel drivers  

## What FLOW Does
✓ Routes traffic at the OS level (like any VPN)  
✓ Runs completely in userspace  
✓ Valve sees your real IP address (via transparent proxying in some modes, or VPN IP in others - clarification: FLOW typically acts as a VPN/Proxy, so Valve sees the server IP, but we do not manipulate game state).
✓ No DLL injection or memory manipulation  

## Technical Details
- FLOW uses standard SOCKS5 proxy (port 10808) and XRay core.
- Windows routes CS2 traffic through proxy automatically via system proxy settings or Tun mode.
- XRay encrypts traffic to our servers using VLESS+Reality.
- Servers forward traffic to Valve unchanged.

## Precedent
Services like ExitLag, WTFast, and Haste have been used by millions of CS2/CS:GO players for years without VAC issues.

## Our Commitment
If you receive a VAC ban while using FLOW (and can prove FLOW was the cause), we will:
1. Investigate immediately
2. Provide full refund
3. Publish findings publicly
