# Process-Trace-Studio PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module ProcessTraceStudio -Scope CurrentUser`.

```powershell
$client=Connect-ProcessTraceStudioSDK
try { Invoke-ProcessTraceStudioSDK -Client $client -Operation status } finally { Disconnect-ProcessTraceStudioSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
