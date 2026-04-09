# Building VSCode C++/Qt Dev Containers

I use dev containers to streamline C++/Qt development on VSCode. The following Docker images are built:
- `vscode-dev-container:qt6` (Unsanitized Qt)
- `vscode-dev-container:qt6-asan-ubsan` (ASAN+UBSAN sanitized Qt)
- `vscode-dev-container:qt6-tsan` (TSAN sanitized Qt)

With the exception of glibc and libresolv, I build Qt and all of its dependencies. The following Qt modules are built during Docker image creation:
- QtBase
- QtNetwork
- QtWebSockets
- QtHttpServer
- QtGrpc
- QtRemoteObjects
- QtTaskTree
- QtOpenAPI
- QtNetworkAuth

The final dev containers will be based on the Docker images above. But before building the final images, it is important to note that, on macOS and Windows, Docker bind mounted volumes are significantly slower than on Linux. As I use Debian, I prefer per-project dev containers with bind mounted volumes. If you are following this guide to set up VSCode on macOS/Windows, you may prefer to use [named volumes for the entire source tree](https://code.visualstudio.com/remote/advancedcontainers/improve-performance#_use-a-named-volume-for-your-entire-source-tree) with [shared dev containers](https://code.visualstudio.com/remote/advancedcontainers/configure-separate-containers).

You can build all Qt6-based Docker images as follows (the script builds three version of LLVM and thus takes a lot of time to compile):

```bash
LLVM_VERSION=21.1.8
QT_VERSION=6.11.0
./build-dev-containers.sh --llvm-version ${LLVM_VERSION} --qt-version ${QT_VERSION}
```

If you prefer to use shared dev containers with named volumes for the entire source tree, do as shown below:

```bash
# Setup shared dev containers
PROJECTS_ROOT_DIR="/my/projects/root/dir"
# VSCodeCppProjectsDevContainerVolume is hardcoded in devcontainer.json files
docker volume create VSCodeCppProjectsDevContainerVolume
mkdir -p ${PROJECTS_ROOT_DIR}/.devcontainer/
cp -rf ./SharedDevContainers/* ${PROJECTS_DIR}/.devcontainer/
```

Now, you can follow the steps shown at https://code.visualstudio.com/remote/advancedcontainers/configure-separate-containers to finish the process for setting up shared dev containers. At https://code.visualstudio.com/remote/advancedcontainers/improve-performance#_use-a-named-volume-for-your-entire-source-tree you can find more information about using Docker named volumes for the entire source tree.

If you prefer per-project dev containers with bind mounted volumes, for every project that you want to use dev containers, do as follows:

```bash
PROJECT_ROOT_DIR="/your/project/root/dir"
mkdir -p ${PROJECT_ROOT_DIR}/.devcontainer/
cp -rf ./DevContainers/* ${PROJECT_ROOT_DIR}/.devcontainer/
```

At [PrettyPrinters](../PrettyPrinters/README.md) you can find a tutorial on how to set up an app and debug it both locally and on dev containers.
