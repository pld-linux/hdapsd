%define		snap	20090401
%define		rel	4
Summary:	HardDrive Active Protection System
Summary(pl.UTF-8):	HDAPS - system aktywnej ochrony dysku twardego
Name:		hdapsd
Version:	0.1
Release:	0.%{snap}.%{rel}
License:	GPL v2
Group:		Applications/System
Source0:	http://downloads.sourceforge.net/hdaps/%{name}-20090401.tar.gz
# Source0-md5:	897cee8b0192febd127470f3e9506aeb
Source1:	%{name}.init
Source2:	%{name}.sysconfig
Patch0:		format-security.patch
URL:		http://hdaps.sourceforge.net/
BuildRequires:	rpmbuild(macros) >= 1.268
Requires(post,preun):	/sbin/chkconfig
Requires:	rc-scripts >= 0.4.3.0
# relies on kernel hdaps driver, which depends on CONFIG_X86
ExclusiveArch:	%{ix86} %{x8664} x32
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
HardDrive Active Protection System.

The APS is a protection system for the ThinkPads internal harddrive. A
sensor inside the ThinkPad recognizes when the notebook is
accelerated. A software applet then is triggered to park the harddisk.
This way the risk of data loss in case of when the notebook is dropped
is significantly reduced since the read/write head of the harddrive is
parked and hence can't crash onto the platter when the notebook drops
onto the floor.

%description -l pl.UTF-8
HardDrive Active Protection System - system aktywnej ochrony dysku
twardego.

APS to system ochrony dla wewnętrznego dysku twardego ThinkPadów.
Czujnik wewnątrz ThinkPada rozpoznaje kiedy notebook podlega
przyspieszeniu. Aplet programowy reaguje na to parkując dysk twardy. W
ten sposób ryzyko utraty danych w przypadku upuszczenia notebooka jest
znacząco zmniejszane, ponieważ głowica odczytująco-zapisująca dysku
jest zaparkowana, dzięki czemu nie może uderzyć w talerz dysku przy
uderzeniu o podłoże.

%prep
%setup -q -n %{name}-%{snap}
%patch0 -p1

%build
%configure
%{__make}

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/etc/{sysconfig,rc.d/init.d}

%{__make} install \
	doc_DATA= \
	DESTDIR=$RPM_BUILD_ROOT

install %{SOURCE1} $RPM_BUILD_ROOT/etc/rc.d/init.d/%{name}
cp -p %{SOURCE2} $RPM_BUILD_ROOT/etc/sysconfig/%{name}

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/chkconfig --add %{name}
%service %{name} restart "%{name} daemon"

%preun
if [ "$1" = "0" ]; then
	%service %{name} stop
	/sbin/chkconfig --del %{name}
fi

%files
%defattr(644,root,root,755)
%doc README ChangeLog AUTHORS
%attr(755,root,root) %{_sbindir}/hdapsd
%attr(754,root,root) /etc/rc.d/init.d/%{name}
%config(noreplace) %verify(not md5 mtime size) /etc/sysconfig/%{name}
%{_mandir}/man8/hdapsd.8*
