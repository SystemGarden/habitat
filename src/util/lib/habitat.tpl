Summary: Application and system monitor and uploader into the System Garden platform. Collects, visualises and distributes statistics and logs. Programmable and customisable.
Name: __NAME__
Version: __VERSION__
Release: __RELEASE__
License: __LICENSE__
Group: Application/System
Source: __NAME__-%{version}.tar.gz
Vendor: System Garden <welcome@systemgarden.com>
URL: http://www.systemgarden.com/habitat/
Packager: __PACKAGER__
BuildArch: __BUILDARCH__
BuildRoot: %{_tmppath}/%{name}-root
__REQUIRES__
__PROVIDES__
#
AutoReqProv:   no

%description
__DESCRIPTION__
Application and system monitor for performance monitoring, behaviour 
checking and automated data uploads into the System Garden platform.

It can run standalone as a host monitor, user level for benchmarking,
with a collection of peers for a small network or connected into 
System Garden's repository for large scale monitoring and access to 
internet based tools.

A native GUI visualises local and remote data for speed and controls 
standalone operation.

Data is collected from kernel, services and applications using a daemon 
(called clockwork) from files or direct APIs.  Applications and scripts 
can push their data into habitat by command line or API using CSV and 
similar formats.

Log files can be collected for uploading, or monitored for regular expressions
which generate events that run arbitary scripts. File changes can also 
be recorded in a history of versions.

Probes, jobs, data sources, replication and configuration can all be 
customised, with many solutions pre-fabricated by name.

%prep
#%setup -q -n %{name}-%{version}
%setup 

%build

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
mkdir "$RPM_BUILD_ROOT"
pushd $RPM_BUILD_DIR/%{name}-%{version}
cp -rp * $RPM_BUILD_ROOT
popd

%pre

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%post

%preun

%postun

%files
%defattr(-,www-data,www-data)
# Add your files here, e.g.:
#%doc doc/readme.txt
#%config /etc/sample.conf
#%attr(644,root,root) /some/file.ext
# This will include /some/dir and all files/directories below it
#/some/dir

%changelog
