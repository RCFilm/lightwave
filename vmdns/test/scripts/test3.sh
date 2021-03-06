#!/bin/bash

/opt/vmware/bin/vmdns-cli add-record --zone in-addr.arpa --type PTR --hostname check --ip 1.2.3.4 --password 'Passw0rd$'
/opt/vmware/bin/vmdns-cli query-record --zone in-addr.arpa --type PTR --ip 1.2.3.4 --password 'Passw0rd$'
/opt/vmware/bin/vmdns-cli del-record --zone in-addr.arpa --type PTR --ip 1.2.3.4 --hostname check --password 'Passw0rd$'
/opt/vmware/bin/vmdns-cli del-record --zone in-addr.arpa --type PTR --ip 1.2.3.4 --hostname check --password 'Passw0rd$'
/opt/vmware/bin/vmdns-cli del-record --zone ip6.arpa --type PTR --ip ::1 --hostname check --password 'Passw0rd$'
