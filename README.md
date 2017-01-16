# nscat
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.me/geokapp)

nscat  is a recursive namespace listing program that produces a depth indented listing of system namespaces and member processes. With no arguments, nscat lists the identities of all the identified namespaces that exist in the system.

## Installation
To compile and install nscat on your system run the following commands:

	./configure
	make
	make install

## Usage
Usage: **nscat [ options ]**

- **-t, --ns-type NS[,NS]...**: Print information about the given namespaces only. The NS parameter can be one of: IPC, MNT, NET, PID, USER, UTS, CGROUP. The default is to print information about all namespaces.
- **-n, --ns NID**: Print information only for the given namespace whose identifier matches NID.
- **-p, --pid PID **: Print namespace information only for the process whose process ID matches PID.
- **-d, --descendants**: This option can be used in conjuction with the --pid flag. It instructs the tool to print namespace information for the given process and its descendants.
- **-r, --show-procs**: This option causes the tool to display all the process members of each namespace.
- **-e, --extend-info**: Print extended information for each namespace.
- **-h, --help**: Print this help message and exit.
- **-v, --version**: Print the version number and exit.



