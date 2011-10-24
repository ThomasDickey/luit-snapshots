Summary: luit - Locale and ISO 2022 support for Unicode terminals
%define AppProgram luit
%define AppVersion 20100531
# $XTermId: luit.spec,v 1.2 2010/05/31 12:34:33 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: 1
License: MIT
Group: Applications/System
URL: ftp://invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppVersion}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
Luit is a filter that can be run between an arbitrary application and a
UTF-8 terminal emulator.  It will convert application output  from  the
locale's  encoding  into  UTF-8,  and convert terminal input from UTF-8
into the locale's encoding.

%prep

%setup -q -n %{AppProgram}-%{AppVersion}

%build

INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir}

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install                    DESTDIR=$RPM_BUILD_ROOT

strip $RPM_BUILD_ROOT%{_bindir}/%{AppProgram}

mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
install -m 644 %{AppProgram}.man $RPM_BUILD_ROOT%{_mandir}/man1/%{AppProgram}.1

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc %{AppProgram}.log.html
%{_prefix}/bin/%{AppProgram}
%{_mandir}/man1/%{AppProgram}.*

%changelog
# each patch should add its ChangeLog entries here

* Mon May 31 2010 Thomas Dickey
- initial version
