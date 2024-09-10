Summary: Locale and ISO 2022 support for Unicode terminals
%define AppProgram luit
%define AppVersion 20240910
%define UseProgram b%{AppProgram}
# $XTermId: luit.spec,v 1.73 2024/09/09 08:19:49 tom Exp $
Name: %{UseProgram}
Version: %{AppVersion}
Release: 1
License: MIT
Group: Applications/System
URL: https://invisible-island.net/%{name}/
Source0: %{AppProgram}-%{AppVersion}.tgz

%description
Luit is a filter that can be run between an arbitrary application and a
UTF-8 terminal emulator.  It will convert application output  from  the
locale's  encoding  into  UTF-8,  and convert terminal input from UTF-8
into the locale's encoding.

This package installs an alternative binary "bluit", and adds a symbolic link
for "xterm-filter".

%prep

%define debug_package %{nil}

%setup -q -n %{AppProgram}-%{AppVersion}

%build

INSTALL_PROGRAM='${INSTALL}' \
%configure \
  --program-prefix=b \
  --target %{_target_platform} \
  --prefix=%{_prefix} \
  --bindir=%{_bindir} \
  --libdir=%{_libdir} \
  --mandir=%{_mandir}

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install                    DESTDIR=$RPM_BUILD_ROOT
( cd $RPM_BUILD_ROOT%{_bindir}      && ln -s %{UseProgram} xterm-filter )
( cd $RPM_BUILD_ROOT%{_mandir}/man1 && ln -s %{UseProgram}.1 xterm-filter.1 )

strip $RPM_BUILD_ROOT%{_bindir}/%{UseProgram}

%files
%defattr(-,root,root)
%doc %{AppProgram}.log.html
%{_prefix}/bin/%{UseProgram}
%{_prefix}/bin/xterm-filter
%{_mandir}/man1/*

%changelog
# each patch should add its ChangeLog entries here

* Tue Jan 11 2022 Thomas Dickey <dickey@his.com>
- update URL, install package as "bluit"

* Sat Jun 05 2010 Thomas Dickey <dickey@his.com>
- Fixes/improvements for FreeBSD and Solaris

* Mon May 31 2010 Thomas Dickey <dickey@his.com>
- initial version
