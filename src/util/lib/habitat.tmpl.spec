%define prefix /usr

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
and applications. Highly extensible collector of timeseries tabular data and 
also the main gateway into System Garden for social IT management.

Data is collected as a time series of tables from kernel and applications 
using a daemon (called clockwork).  Applications and scripts can push their
data into Habitat by command line or API using CSV and similar formats.
Log files are monitored for regular expressions, causing events which 
can generate logs in turn, relay information to other systems or 
run arbitary scripts. File changes can also be recorded in a history
of versions.

Data is stored on local machine in cascaded rings for low maintenance 
and disk economy; they can be archived remotely to a repository for 
long term storage & trend analysis. Visualisation is by Gtk+ application, 
curses or command line and can view data from peer machines also running 
Habitat.

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
if [ "%{RELEASE}" = "mdk" -o "%{RELEASE}" = "cl" ]
then
    mkdir -p $RPM_BUILD_ROOT%{_menudir}
    cat <<EOF >$RPM_BUILD_ROOT%{_menudir}/habitat
   ?package(habitat):command="/usr/bin/myhabitat" icon="habitatlogo" \
   needs="X11" section="System/Monitoring" title="Habitat" \
   longtitle="System & application monitor, collector and visualizer"
EOF
fi
if [ "%{RELEASE}" = "freedesktop" ]
then
fi
if [ "%{RELEASE}" = "suse" ]
then
fi

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

%if %(test "%{RELEASE}" = "mdk" -o "%{RELEASE}" = "cl" && echo 1 || echo 0)
# Makedrake and Connectiva Linux
/%{_menudir}/habitat
%endif

%if %{freedesktop}
# Freedesktop menu & icon
/usr/share/pixmaps/habitat.png
/usr/share/applications/habitat.desktop
%endif

%if %{suse} && !%{freedesktop}
# SuSE menu & icon
/etc/X11/susewm/AddEntrys/SuSE/Internet/WWW/habitat.desktop
/usr/X11R6/share/icons/png/hicolor/misc/apps/habitat.png
%endif

%if %{oldredhat}
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
