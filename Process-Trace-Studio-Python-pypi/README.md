# Process-Trace-Studio Python SDK

`pip install process-trace-studio`

Windows x64 Python3.10+. No driver is installed by this package.

```python
from process_trace_studio import Client
with Client() as client:
    info = client.call("status")
```

[Operation schema and API](docs/API.md). Check errors and the product licensing state before device creation.
