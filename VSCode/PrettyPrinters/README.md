# Qt Debugging Helpers
The `qt_lldb_formatters.py` file defines LLDB data formatters for many Qt classes. You can instruct LLDB to use these formatters with the following command:

```bash
echo "command script import /path/to/qt_lldb_formatters.py" > /home/${USER}/.lldbinit
```

The [QtPrettyPrintersTestApp](QtPrettyPrintersTestApp/README.md) folder contains step-by-step instructions on how to set up a Qt app on VSCode and debug it both locally and on dev containers.
