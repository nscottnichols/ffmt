# Header dependencies

The legacy host/device object rules depend on every installed library header. Editing an internal or optional header therefore invalidates a previously built object; the focused suite registry maintains its own corresponding dependency list. The regression uses real Make timestamps and dry-run compile plans without invoking a compiler.
