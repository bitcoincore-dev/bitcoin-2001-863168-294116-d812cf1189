Building Bitcoin Core with Visual Studio
========================================

Introduction
---------------------
Solution and project files to build the Bitcoin Core with applications `msbuild` or Visual Studio can be found in the `build_msvc` directory. The build has been tested with Visual Studio 2019 (building with earlier versions of Visual Studio should not be expected to work).

To build Bitcoin Core from a command-line interface, it is sufficient to install the Build Tools component only.

Building with Visual Studio is an alternative to the Linux based [cross-compiler build](../doc/build-windows.md).


Prerequisites
---------------------
To build [dependencies](../doc/dependencies.md) (except for [Qt](#qt)),
the default approach is to use the [vcpkg](https://docs.microsoft.com/en-us/cpp/vcpkg) package manager from Microsoft.

To install vcpkg:
```cmd
mkdir C:\dev
cd C:\dev
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

```

By default, vcpkg makes both `release` and `debug` builds for each package.
To save build time and disk space, one could skip `debug` builds (example uses PowerShell):
```powershell

Add-Content -Path "vcpkg\triplets\x64-windows-static.cmake" -Value "set(VCPKG_BUILD_TYPE release)"
```

Qt
---------------------
To build Bitcoin Core with the GUI, a static build of Qt is required.

1. Download a single ZIP archive of Qt source from https://download.qt.io/official_releases/qt/ (e.g., [`qt-everywhere-src-5.12.11.zip`](https://download.qt.io/official_releases/qt/5.12/5.12.11/single/qt-everywhere-src-5.12.11.zip)), and expand it into a dedicated folder. In this document it is assumed that this folder is `C:\dev\qt-source`.

2. Apply the following patch:
```diff
--- old\qt-source\qtbase\mkspecs\common\msvc-desktop.conf
+++ new\qt-source\qtbase\mkspecs\common\msvc-desktop.conf
@@ -37,9 +37,9 @@
 QMAKE_CFLAGS            = -nologo -Zc:wchar_t
 QMAKE_CFLAGS_WARN_ON    = -W3
 QMAKE_CFLAGS_WARN_OFF   = -W0
-QMAKE_CFLAGS_RELEASE    = $$QMAKE_CFLAGS_OPTIMIZE -MD
-QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += $$QMAKE_CFLAGS_OPTIMIZE -Zi -MD
-QMAKE_CFLAGS_DEBUG      = -Zi -MDd
+QMAKE_CFLAGS_RELEASE    = -MT
+QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += -Zi -MT
+QMAKE_CFLAGS_DEBUG      = -Zi -MTd
 QMAKE_CFLAGS_YACC       =
 QMAKE_CFLAGS_LTCG       = -GL

```

3. Open "x64 Native Tools Command Prompt for VS 2019", and input the following commands:
```cmd
cd C:\dev\qt-source
mkdir build
cd build
..\configure -release -silent -opensource -confirm-license -opengl desktop -no-shared -static -no-static-runtime -mp -qt-zlib -qt-pcre -qt-libpng -no-libjpeg -nomake examples -nomake tests -nomake tools -no-dbus -no-libudev -no-icu -no-gtk -no-opengles3 -no-angle -no-sql-sqlite -no-sql-odbc -no-sqlite -no-libudev -no-vulkan -skip qt3d -skip qtactiveqt -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtconnectivity -skip qtdatavis3d -skip qtdeclarative -skip qtdoc -skip qtgamepad -skip qtgraphicaleffects -skip qtimageformats -skip qtlocation -skip qtmacextras -skip qtmultimedia -skip qtnetworkauth -skip qtpurchasing -skip qtquickcontrols -skip qtquickcontrols2 -skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtspeech -skip qtvirtualkeyboard -skip qtwayland -skip qtwebchannel -skip qtwebengine -skip qtwebsockets -skip qtwebview -skip qtx11extras -skip qtxmlpatterns -no-openssl -no-feature-sql -no-feature-sqlmodel -prefix C:\Qt_static
nmake
nmake install
```

One could speed up building with [`jom`](https://wiki.qt.io/Jom), a replacement for `nmake` which makes use of all CPU cores.

To build Bitcoin Core without Qt, unload or disable the `bitcoin-qt`, `libbitcoin_qt` and `test_bitcoin-qt` projects.


Building
---------------------
1. Use Python to generate `*.vcxproj` from Makefile:

```
PS >py -3 msvc-autogen.py
```

2. An optional step is to adjust the settings in the `build_msvc` directory and the `common.init.vcxproj` file. This project file contains settings that are common to all projects such as the runtime library version and target Windows SDK version. The Qt directories can also be set. To specify a non-default path to a static Qt package directory, use the `QTBASEDIR` environment variable.

3. To build from the command line with the Visual Studio 2019 toolchain use:

```cmd
msbuild -property:Configuration=Release -maxCpuCount -verbosity:minimal bitcoin.sln
```

Alternatively, open the `build_msvc/bitcoin.sln` file in Visual Studio 2019.

AppVeyor
---------------------
The .appveyor.yml in the root directory is suitable to perform builds on [AppVeyor](https://www.appveyor.com/) Continuous Integration servers. The simplest way to perform an AppVeyor build is to fork Bitcoin Core and then configure a new AppVeyor Project pointing to the forked repository.

For safety reasons the Bitcoin Core .appveyor.yml file has the artifact options disabled. The build will be performed but no executable files will be available. To enable artifacts on a forked repository uncomment the lines shown below:

```
    #- 7z a bitcoin-%APPVEYOR_BUILD_VERSION%.zip %APPVEYOR_BUILD_FOLDER%\build_msvc\%platform%\%configuration%\*.exe
    #- path: bitcoin-%APPVEYOR_BUILD_VERSION%.zip
```

Security
---------------------
[Base address randomization](https://docs.microsoft.com/en-us/cpp/build/reference/dynamicbase-use-address-space-layout-randomization?view=msvc-160) is used to make Bitcoin Core more secure. When building Bitcoin using the `build_msvc` process base address randomization can be disabled by editing `common.init.vcproj` to change `RandomizedBaseAddress` from `true` to `false` and then rebuilding the project.

To check if `bitcoind` has `RandomizedBaseAddress` enabled or disabled run

```
.\dumpbin.exe /headers src/bitcoind.exe
```

If is it enabled then in the output `Dynamic base` will be listed in the `DLL characteristics` under `OPTIONAL HEADER VALUES` as shown below

```
            8160 DLL characteristics
                   High Entropy Virtual Addresses
                   Dynamic base
                   NX compatible
                   Terminal Server Aware
```

This may not disable all stack randomization as versions of windows employ additional stack randomization protections. These protections must be turned off in the OS configuration.