# RPM Next

One day [Cjacker](https://github.com/cjacker) had an idea: there are OS layer 
(/var/lib/rpm) and AppStore layer (/var/lib/appstore) physical isolated. 
so rpm -ivh/-e only install/erase Apps into/from AppStore layer, OS is totally 
READ-ONLY. when installing Apps, check dependency in OS layer at first, if 
unsatisfied depend, then switch to AppStore layer for checking dependency again, 
if still unsatisfied depend, add dependency problem, otherwise installed Apps 
successfully.

![RPM isolatation design](https://raw.github.com/AOSC-Dev/rpm/isolate/doc/rpm-isolatation-design.png)

## Goal

* keep rpm original command unbroken;
* keep librpm original API unbroken;
* just like Mac ***make install*** is still able to pollute OS layer;

The implementation, very big patch more than 2K lines, will open source in 2016 ;-)

## Build

```
wget http://download.oracle.com/berkeley-db/db-6.1.23.tar.gz
tar xvf db-6.1.23.tar.gz
ln -s db-6.1.23 db
```

```
export CPPFLAGS="$CPPFLAGS `pkg-config --cflags nss` -DLUA_COMPAT_APIINTCASTS"
export CFLAGS="$RPM_OPT_FLAGS -DLUA_COMPAT_APIINTCASTS"
./autogen.sh --prefix=/usr   \
    --sysconfdir=/etc   \
    --localstatedir=/var    \
    --with-archive  \
    --with-lua  \
    --with-cap  \
    --enable-plugins    \
    --enable-python \
    --with-vendor=aosc \
    --enable-debug=yes
```
