# ASSystemService

This repo contains implementation related to System 


### Building

To build you need an RDK SDK installed on your local machine.
  
#### Using CLion

Open the top level **CMakeList.txt** file with CLion and select the
**Open as project** option.  Then open **Settings** ->
**Build, Execution, Deployment** -> **CMake** and in the default profile add
the following to the **CMake options** field:

```
-D RDK_SDK=<PATH_RDK_SDK> 
```

Where `<PATH_RDK_SDK>` should be the root SDK directory, ie. the one containing
the `environment-setup-armv7at2hf-neon-rdk-linux-gnueabi` file.

#### On the Command Line

```
cd as-system-service
mkdir build
cd build
cmake -D RDK_SDK=<PATH_RDK_SDK>  ../.
make
```
Where `<PATH_RDK_SDK>` should be the root SDK directory, ie. the one containing
the `environment-setup-armv7at2hf-neon-rdk-linux-gnueabi` file.


### Systemd

The library is built around [libsystemd][1] and it's C API, in particular it uses
the `sd-event`, `sd-bus` and `sd-journal` parts for the event loop, dbus and
logging APIs respectively.



[1]: https://github.com/systemd/systemd/tree/master/src/libsystemd
