%define	snap	20060322
Summary:	HardDrive Active Protection System
Summary(pl):	HDAPS - system aktywnej ochrony dysku twardego
Name:		hdapsd
Version:	0.1
Release:	0.%{snap}.1
License:	GPL v2
Group:		Applications/System
Source0:	http://www.dresco.co.uk/hdaps/%{name}-%{snap}.c
# Source0-md5:	f9ac7f151e42ac6cc7249f5ccceb494b
Source1:	%{name}.init
Source2:	%{name}.sysconfig
URL:		http://www.thinkwiki.org/wiki/How_to_protect_the_harddisk_through_APS
BuildRequires:	rpmbuild(macros) >= 1.268
Requires(post,preun):	/sbin/chkconfig
Requires:	rc-scripts
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

%description -l pl
HardDrive Active Protection System - system aktywnej ochrony dysku
twardego.

APS to system ochrony dla wewnêtrznego dysku twardego ThinkPadów.
Czujnik wewn±trz ThinkPada rozpoznaje kiedy notebook podlega
przyspieszeniu. Aplet programowy reaguje na to parkuj±c dysk twardy. W
ten sposób ryzyko utraty danych w przypadku upuszczenia notebooka jest
znacz±co zmniejszane, poniewa¿ g³owica odczytuj±co-zapisuj±ca dysku
jest zaparkowana, dziêki czemu nie mo¿e uderzyæ w talerz dysku przy
uderzeniu o pod³o¿e.

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
