# Process-Trace-Studio Ruby SDK

`gem install process-trace-studio`

Windows x64 Ruby3.1+.

```ruby
require "process_trace_studio"
ProcessTraceStudioSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
