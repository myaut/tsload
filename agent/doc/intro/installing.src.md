
## Installing TSLoad 

For TSLoad 0.2 this method of installation not recommended because we still have default build mode is debug for a reason. So please use [building from source][intro/building] wherever possible. If you still decided to use binary build, do the following.

### Install prerequisites

#### Windows

On Windows you will only need to install [Microsoft Visual C++ 2010 SP1 Redistributable Package (x64)](http://www.microsoft.com/en-us/download/details.aspx?id=13523).

#### CentOS 6

TSLoad depends on these packages:
	* glibc
	* libgcc
	* libstdc++
 
They are usually shipped with CentOS so no extra action is required.
 
#### Solaris 11

TSLoad depends on these packages:
 	* system/library
 	* system/library/storage/libdiskmgt
 	* system/linker
 	* system/library/gcc-45-runtime
 	* system/library/math
 	* system/picl
 	* system/resource-mgmt/resource-pools
 	* library/libxml2
 	* library/zlib
 	
Like in CentOS case they are usually provided out of scratch except for `system/library/gcc-45-runtime` I presume.

### Install TSLoad

Because TSLoad is bundled as simple ZIP archive simply extract it to directory you want, for example /opt directory in CentOS:

```
$ cd /opt
$ unzip /root/tsload-0.2.a0-centos-x86_64.zip
``` 

