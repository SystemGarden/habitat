# Habitat RPM spec file template, expecting the following to be set by mkrpm:
# version, name, release, _topdir, Source0
%define prefix /
%define is_mandrake %(test %{release} = "mdk" && echo 1 || echo 0)
%define is_suse %(test %{release} = "suse" && echo 1 || echo 0)
%define is_fedora %(test %{release} = "rhfc" && echo 1 || echo 0)
%define is_redhat %(test %{release} = "rh" && echo 1 || echo 0)
%define is_freedesktop %(test %{release} = "freedesktop" && echo 1 || echo 0)
%define is_connectiva %(test %{release} = "cl" && echo 1 || echo 0)

Summary: Application and system performance monitor, collecting and visualising trends, availability and service levels
Name: %{name}
Version: %{version}
Release: %{release}
License: GPL
Group: Application/System
URL: http://www.systemgarden.com/habitat/
#Source1: http://www.systemgarden.com/habitat/download/%{name}-%{version}-src.tar.gz
Vendor: System Garden Ltd
Packager: Nigel Stuckey <nigel.stuckey@systemgarden.com>
BuildRoot: %{_topdir}/broot/%{name}-%{version}-%{release}

%description
Welcome to Habitat, collector, monitor and viewer for system 
and applications. An extendable and flexible collector of timeseries 
tabular data and also the main gateway into System Garden for social IT 
management.

Data is collected as a time series of tables from kernel and applications 
using a daemon (called clockwork).  Applications and scripts can push their
data into Habitat by command line or API using CSV and similar formats.
Log files can be collected or monitored for patterns to generate events 
that relay information to other systems or run arbitary scripts. 
File changes can also be recorded in a history of versions.

Data is typically stored on the local machine with periodic synchronisation
to System Garden at a configurable rate. Local data storage uses a series 
of fixed sized ring buffers on disk, reducing data frequency over time for 
low maintenance and storage economy. Visualisation of local and peer Habitat
machines is by a Gtk+ GUI application, myHabitat or using System Garden.
Data can be extracted using command line tools and management is by 
multi-tier configuration files or GUI.

%prep
%setup -q -n %{name}-%{version}-src

%build
cd src
make

%install
cd src
make linuxinventory
make LINROOT=$RPM_BUILD_ROOT linuxinstall
echo "INVENTORY IS:-"
cat INVENTORY
%if %{is_mandrake} || %{is_connectiva}
    mkdir -p $RPM_BUILD_ROOT%{_menudir}
    cat <<EOF >$RPM_BUILD_ROOT%{_menudir}/habitat
   ?package(habitat):command="/usr/bin/myhabitat" icon="habitatlogo" \
   needs="X11" section="System/Monitoring" title="Habitat" \
   longtitle="System & application monitor, collector and visualizer"
EOF
%endif
%if %{is_freedesktop}
    # Freedesktop menu & icon
    mkdir -p $RPM_BUILD_ROOT/usr/share/pixmaps $RPM_BUILD_ROOT/usr/share/applications
    install -m 644 myhabitat/pixmaps/habitat_flower_32.png $RPM_BUILD_ROOT/usr/share/pixmaps
    install -m 644 util/lib/habitat.desktop $RPM_BUILD_ROOT/usr/share/applications
%endif
%if %{is_suse}
%endif

%clean
rm -rf $RPM_BUILD_DIR %{_topdir}/broot

%post
if [ -x /usr/lib/lsb/install_initd ]
then
	/usr/lib/lsb/install_initd --add habitat
elif [ -x /sbin/chkconfig ]
then
	/sbin/chkconfig --add habitat
else
	for i in 2 3 4 5
	do
		ln -sf /etc/init.d/habitat /etc/rc${i}.d/S92habitat
	done
	for i in 1 6
	do
		ln -sf /etc/init.d/habitat /etc/rc${i}.d/K08habitat
	done
fi
/etc/init.d/habitat start > /dev/null 2>&1
	
%preun
# only on uninstall, not on upgrades
/etc/init.d/habitat stop > /dev/null 2>&1
if [ $1 = 0 ]
then
	if [ -x /usr/lib/lsb/remove_initd ]
	then
		/usr/lib/lsb/remove_initd /etc/init.d/habitat
	elif [ -x /sbin/chkconfig ]
	then
		/sbin/chkconfig --del habitat
	else
		rm -f /etc/rc?.d/???habitat
	fi
fi

# Files section =============================================
%files -f src/INVENTORY
%defattr(-,daemon,daemon,-)

%if %{is_mandrake} || %{is_connectiva}
# Makedrake and Connectiva Linux
/%{_menudir}/habitat
%endif

%if %{is_freedesktop}
# Freedesktop menu & icon
/usr/share/pixmaps/habitat_flower_32.png
/usr/share/applications/habitat.desktop
%endif

%if %{is_suse} && !%{is_freedesktop}
# SuSE menu & icon
/etc/X11/susewm/AddEntrys/SuSE/Internet/WWW/habitat.desktop
/usr/X11R6/share/icons/png/hicolor/misc/apps/habitat.png
%endif

%if %{is_redhat}
# Old-style Red Hat menu & icon
/usr/share/pixmaps/habitat.png
/etc/X11/applnk/Internet/habitat.desktop
%endif

#%license %{_docdir}/%{name}-%{version}/LICENSE
#%readme %{_docdir}/%{name}-%{version}/README
#%doc %{_docdir}/%{name}-%{version}/LICENSE
#%doc %{_docdir}/%{name}-%{version}/INVENTORY
%config /var/lib/habitat
#%config /etc/

%changelog
* Fri Nov 26 2004 Nigel Stuckey <nigel.stuckey@systemgarden.com> - 
- Initial build. See README.
