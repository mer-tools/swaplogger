Name:       swaplogger
Summary:    A command line frame rate measurement tool for EGL and X11 applications.
Version:    0.1~git0.46bfc6
Release:    1
Group:      Development/Tools
License:    MIT
URL:        https://gitorious.org/swaplogger
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xdamage)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(xext)

%description
Description: %{summary}


%prep
%setup


%build
unset LD_AS_NEEDED
make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
install -D -p -m 0755 swaplogger %{buildroot}%{_bindir}/swaplogger
install -D -p -m 0644 swaplogger.so %{buildroot}%{_libdir}/swaplogger.so
ln %{buildroot}%{_libdir}/swaplogger.so %{buildroot}%{_libdir}/swaplogger.so.1

%files
%defattr(-,root,root,-)
%{_bindir}/swaplogger
%{_libdir}/swaplogger.so
%{_libdir}/swaplogger.so.1


