#!/bin/sh /etc/rc.common

START=99
start() {
include /lib/network
local lanifaces = "lan"
local wanifaces = "wan"

scan_interfaces
for iface in ${lanifaces:-lan}; do
    local ipaddr
    config_get ipaddr "$iface" ipaddr
    config_get lanmask "$iface" netmask
done

for waniface in ${wanifaces:-wan}; do
    local wanipaddr
    local wanmask
    config_get wanipaddr "$waniface" ipaddr
done

config_load "network"
local wanifname
config_get wanifname wan ifname

netmask2prefix() {
	local octet
	local prefix=0
	local IFS="."

	set -- $1

	for octet in $1 $2 $3 $4; do
		while [ $octet -gt 0 ]; do
			prefix=$(($prefix + ($octet & 1)))
			octet=$(($octet >> 1))
		done
	done

	return $prefix
}

netmask2prefix "$lanmask"
nummask=$?

lanip1=`echo $ipaddr | cut -f 1 -d "."`
lanip2=`echo $ipaddr | cut -f 2 -d "."`
lanip3=`echo $ipaddr | cut -f 3 -d "."`
lanip4=`echo $ipaddr | cut -f 4 -d "."`
lanmask1=`echo $lanmask | cut -f 1 -d "."`
lanmask2=`echo $lanmask | cut -f 2 -d "."`
lanmask3=`echo $lanmask | cut -f 3 -d "."`
lanmask4=`echo $lanmask | cut -f 4 -d "."`
lan_netseg1=$(($lanip1 & $lanmask1))
lan_netseg2=$(($lanip2 & $lanmask2))
lan_netseg3=$(($lanip3 & $lanmask3))
lan_netseg4=$(($lanip4 & $lanmask4))
local lannetaddr="$lan_netseg1.$lan_netseg2.$lan_netseg3.$lan_netseg4"

#一、filter table
#1.filter table . INPUT chain
iptables -F
iptables -N wan_icmp_input
iptables -A INPUT -j wan_icmp_input
iptables -A wan_icmp_input -p icmp -i $wanifname -j ACCEPT
iptables -N wan_igmp_input
iptables -A INPUT -j wan_igmp_input
iptables -N spi_input
iptables -A INPUT -j spi_input
#iptables -A spi_input -i br-lan -d $lannetaddr/$nummask -j DROP
iptables -A spi_input -m state --state INVALID -j DROP
iptables -A spi_input -i br-lan -m state --state NEW -j ACCEPT
iptables -A spi_input -i lo -m state --state NEW -j ACCEPT
iptables -A spi_input -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -A spi_input -j DROP
iptables -A INPUT -i $wanifname -p tcp --dport 80 -m state --state NEW,INVALID -j DROP
#2.filter table . FORWARD chain
iptables -F FORWARD
iptables -A FORWARD -o $wanifname -s $lannetaddr/$nummask -p udp --dport 5060 -j REJECT --reject-with icmp-port-unreachable
iptables -A FORWARD -p gre -j ACCEPT
iptables -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu
iptables -N spi_forward
iptables -A FORWARD -j spi_forward
iptables -A spi_forward -m state --state INVALID -j DROP
iptables -A spi_forward -i br-lan -m state --state NEW -j ACCEPT
iptables -A spi_forward -i lo -m state --state NEW -j ACCEPT
iptables -A spi_forward -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -A spi_forward -j DROP

#二、nat table
#1.nat table . POSTROUTING chain
iptables -t nat -F
iptables -t nat -A POSTROUTING -o $wanifname  -s $lannetaddr/$nummask -j MASQUERADE

#二、mangle table
iptables -t mangle -F  
                                                                                                     
}
