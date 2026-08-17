Name: %(echo ob-sanity-abiv$CXX_ABI)
Version: %{version}
Release: %{release}
# if you want use the parameter of rpm_create on build time,
# uncomment below
Summary: an auxiliary tool to collect and parse backtrace
Group: alibaba/application
License: Apache-2.0
%define _src ob-sanity-master
#%define debug_package %{nil}
#%define __strip $OLDPWD/rpm/strip
%define __strip /usr/bin/true
%global _find_debuginfo_opts -s

# uncomment below, if your building depend on other packages

#BuildRequires: package_name = 1.0.0

# uncomment below, if depend on other packages

#Requires: package_name = 1.0.0


%description
# if you want publish current svn URL or Revision use these macros
an auxiliary tool to collect and parse backtrace
CodeUrl:%{_source_path}
CodeRev:%{_source_revision}

#%debug_package
# support debuginfo package, to reduce runtime package size

# prepare your files
%install
# OLDPWD is the dir of rpm_create running
# can set by rpm_create, default is "/home/a"
# _lib is an inner var, maybe "lib" or "lib64" depend on OS
export PATH=${DEP_DIR}/bin:$PATH
export LIBRARY_PATH=${DEP_DIR}/lib:$LIBRARY_PATH
cxxabiv1_option=""
if [[ ${CXX_ABI} -eq "1" ]];then
  cxxabiv1_option="--cxxabiv1"
fi
cd $OLDPWD/..
bash build.sh clean
bash build.sh --cpp20 ${cxxabiv1_option} -DBUILD_TESTING=OFF
cd build
make %{_smp_mflags};
mkdir -p ${RPM_BUILD_ROOT}/usr/local/oceanbase/devtools/lib64
mkdir -p ${RPM_BUILD_ROOT}/usr/local/oceanbase/devtools/include
cp $(find src -name *.so) ${RPM_BUILD_ROOT}/usr/local/oceanbase/devtools/lib64
cp $(find src -name *.a) ${RPM_BUILD_ROOT}/usr/local/oceanbase/devtools/lib64
cp -r include/* ${RPM_BUILD_ROOT}/usr/local/oceanbase/devtools/include
install -d ${RPM_BUILD_ROOT}%{_licensedir}/%{name}
install -m 0644 ../LICENSE ../NOTICE ${RPM_BUILD_ROOT}%{_licensedir}/%{name}/

# package infomation
%files
# set file attribute here
%defattr(-,root,root)
# need not list every file here, keep it as this
## create an empy dir

# %dir %{_prefix}/var/log

## need bakup old config file, so indicate here
/usr/local/oceanbase/devtools/lib64/*.so
/usr/local/oceanbase/devtools/lib64/*.a
/usr/local/oceanbase/devtools/include
%license %{_licensedir}/%{name}/LICENSE
%license %{_licensedir}/%{name}/NOTICE

# %config %{_prefix}/etc/sample.conf

## or need keep old config file, so indicate with "noreplace"

# %config(noreplace) %{_prefix}/etc/sample.conf

## indicate the dir for crontab

# %attr(644,root,root) %{_crondir}/*

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%changelog
* Tue Jul 10 2024 nijia.nj
- add spec of ob-sanity
