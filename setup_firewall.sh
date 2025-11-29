#!/bin/bash

echo "================================================"
echo "Echo WiFi - Linux Firewall Configuration"
echo "================================================"
echo ""
echo "This script will add firewall rules for Echo WiFi messaging"
echo "You may need to enter your sudo password"
echo ""
read -p "Press Enter to continue..."

echo ""
echo "Detecting firewall system..."


if command -v firewall-cmd &> /dev/null && systemctl is-active --quiet firewalld; then
    FIREWALL="firewalld"
    echo "Detected: firewalld"
elif command -v ufw &> /dev/null; then
    FIREWALL="ufw"
    echo "Detected: ufw"
else
    FIREWALL="iptables"
    echo "Detected: iptables (or no firewall)"
fi

echo ""
echo "Adding firewall rules using $FIREWALL..."
echo ""

if [ "$FIREWALL" = "firewalld" ]; then


    sudo firewall-cmd --permanent --add-port=48270/udp
    if [ $? -eq 0 ]; then
        echo "[OK] UDP 48270 - Discovery"
    else
        echo "[FAIL] UDP 48270"
    fi

    sudo firewall-cmd --permanent --add-port=48271/tcp
    if [ $? -eq 0 ]; then
        echo "[OK] TCP 48271 - Messaging"
    else
        echo "[FAIL] TCP 48271"
    fi

    echo ""
    echo "Reloading firewall..."
    sudo firewall-cmd --reload

    echo ""
    echo "Current firewall rules:"
    sudo firewall-cmd --list-ports

elif [ "$FIREWALL" = "ufw" ]; then


    sudo ufw allow 48270/udp comment "Echo WiFi Discovery"
    if [ $? -eq 0 ]; then
        echo "[OK] UDP 48270 - Discovery"
    else
        echo "[FAIL] UDP 48270"
    fi

    sudo ufw allow 48271/tcp comment "Echo WiFi Messaging"
    if [ $? -eq 0 ]; then
        echo "[OK] TCP 48271 - Messaging"
    else
        echo "[FAIL] TCP 48271"
    fi

    echo ""
    echo "Current firewall status:"
    sudo ufw status

else


    sudo iptables -A INPUT -p udp --dport 48270 -j ACCEPT
    if [ $? -eq 0 ]; then
        echo "[OK] UDP 48270 - Discovery"
    else
        echo "[FAIL] UDP 48270"
    fi

    sudo iptables -A INPUT -p tcp --dport 48271 -j ACCEPT
    if [ $? -eq 0 ]; then
        echo "[OK] TCP 48271 - Messaging"
    else
        echo "[FAIL] TCP 48271"
    fi

    echo ""
    echo "Saving iptables rules..."


    if command -v iptables-save &> /dev/null; then
        if [ -d /etc/iptables ]; then
            sudo iptables-save | sudo tee /etc/iptables/rules.v4 > /dev/null
            echo "Rules saved to /etc/iptables/rules.v4"
        elif [ -d /etc/sysconfig ]; then
            sudo iptables-save | sudo tee /etc/sysconfig/iptables > /dev/null
            echo "Rules saved to /etc/sysconfig/iptables"
        else
            echo "WARNING: Could not determine where to save iptables rules"
            echo "Rules will be lost on reboot"
        fi
    fi

    echo ""
    echo "Current iptables rules:"
    sudo iptables -L -n | grep -E "(48270|48271)"
fi

echo ""
echo "================================================"
echo "Configuration complete!"
echo "================================================"
echo ""
echo "Ports configured:"
echo "  UDP 48270 - WiFi Discovery (broadcast)"
echo "  TCP 48271 - WiFi Messaging (direct)"
echo ""
echo "You can now run Echo and use WiFi messaging"
echo ""