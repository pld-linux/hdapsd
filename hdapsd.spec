%define	snap	20060409
Summary:	HardDrive Active Protection System
Summary(pl.UTF-8):   HDAPS - system aktywnej ochrony dysku twardego
Name:		hdapsd
Version:	0.1
Release:	0.%{snap}.1
License:	GPL v2
Group:		Applications/System
Source0:	http://www.dresco.co.uk/hdaps/%{name}-%{snap}.c
# Source0-md5:	0b157c049c14c0e79b41b2d3ebfe39a0
Source1:	%{name}.init
Source2:	%{name}.sysconfig
URL:		http://www.thinkwiki.org/wiki/How_to_protect_the_harddisk_through_APS
BuildRequires:	rpmbuild(macros) >= 1.268
Requires(post,preun):	/sbin/chkconfig
Requires:	rc-scripts
# relies on kernel hdaps driver, which depends on CONFIG_X86
ExclusiveArch:	%{ix86} %{x8664}
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
%setup -q -c -T

%build
%{__cc} %{rpmcflags} %{rpmldflags} %{SOURCE0} -o %{name}

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT{%{_sbindir},/etc/{sysconfig,rc.d/init.d}}

install %{name} $RPM_BUILD_ROOT%{_sbindir}
install %{SOURCE1} $RPM_BUILD_ROOT/etc/rc.d/init.d/%{name}
install %{SOURCE2} $RPM_BUILD_ROOT/etc/sysconfig/%{name}

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
%attr(754,root,root) %{_sbindir}/*
%attr(754,root,root) /etc/rc.d/init.d/%{name}
%config(noreplace) %verify(not md5 mtime size) /etc/sysconfig/%{name}
