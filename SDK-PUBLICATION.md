# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | process-trace-studio |
| CPP/conan | published_custom_source_index | [process-trace-studio](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | Process-Trace-Studio.Native |
| NET/nuget | built_verified_requires_trusted_publishing_policy | ProcessTraceStudio.SDK [Source](Process-Trace-Studio-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | process-trace-studio [Source](Process-Trace-Studio-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [process-trace-studio](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [process-trace-studio](https://crates.io/crates/process-trace-studio) [Source](Process-Trace-Studio-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [process-trace-studio](https://www.npmjs.com/package/process-trace-studio) [Source](Process-Trace-Studio-JavaScript-npm/) |
| Java/maven | published_install_verified | [process-trace-studio](https://central.sonatype.com/artifact/io.github.titancsa/process-trace-studio/1.0.0) [Source](Process-Trace-Studio-Java-maven/) |
| Java/jitpack | published_install_verified | [process-trace-studio](https://jitpack.io/#titanfyre/Process-Trace-Studio/java-sdk-v1.0.0) [Source](Process-Trace-Studio-Java-maven/) |
| Go/modules | published_install_verified | [process-trace-studio](https://pkg.go.dev/github.com/titanfyre/Process-Trace-Studio/Process-Trace-Studio-Go-modules@v1.0.0) [Source](Process-Trace-Studio-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Process-Trace-Studio/Process-Trace-Studio-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [ProcessTraceStudio](https://www.powershellgallery.com/packages/ProcessTraceStudio/1.0.0) [Source](Process-Trace-Studio-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | process_trace_studio [Source](Process-Trace-Studio-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [process-trace-studio](https://rubygems.org/gems/process-trace-studio) [Source](Process-Trace-Studio-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/process-trace-studio](https://packagist.org/packages/titancsa/process-trace-studio) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
