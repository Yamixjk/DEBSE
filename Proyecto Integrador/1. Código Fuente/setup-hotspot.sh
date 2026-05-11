#!/bin/bash
# setup-hotspot.sh — Configura el hotspot Aegnis-Setup en la Raspberry Pi
# El hotspot se activa solo cuando no hay conexion a internet
# Ejecutar una sola vez como root: sudo bash setup-hotspot.sh

set -e

HOTSPOT_SSID="Aegnis-Setup"
HOTSPOT_PASS="aegnis2026"
IFACE="wlan0"

echo "Configurando hotspot '$HOTSPOT_SSID'..."

# Crear conexion hotspot con NetworkManager
nmcli connection delete "Aegnis-Setup" 2>/dev/null || true

nmcli connection add \
  type wifi \
  ifname "$IFACE" \
  con-name "Aegnis-Setup" \
  autoconnect no \
  ssid "$HOTSPOT_SSID" \
  -- \
  wifi.mode ap \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "$HOTSPOT_PASS" \
  ipv4.method shared \
  ipv4.addresses 192.168.4.1/24

echo "Conexion hotspot creada."

# Crear script que decide si activar el hotspot
cat > /usr/local/bin/aegnis-check-hotspot.sh << 'EOF'
#!/bin/bash
LOGFILE="/var/log/aegnis-hotspot.log"
log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> "$LOGFILE"; }

sleep 10

if ping -c 2 -W 3 8.8.8.8 > /dev/null 2>&1; then
    log "Internet disponible. Hotspot no necesario."
    nmcli connection down "Aegnis-Setup" 2>/dev/null || true
    exit 0
fi

log "Sin internet. Activando hotspot Aegnis-Setup..."
nmcli connection up "Aegnis-Setup" 2>/dev/null && log "Hotspot activo." || log "Error activando hotspot."
EOF

chmod +x /usr/local/bin/aegnis-check-hotspot.sh

cat > /etc/systemd/system/aegnis-hotspot.service << 'EOF'
[Unit]
Description=Aegnis - Hotspot de configuracion (condicional)
After=NetworkManager.service network.target
Wants=NetworkManager.service

[Service]
Type=oneshot
RemainAfterExit=no
ExecStart=/usr/local/bin/aegnis-check-hotspot.sh
Restart=no

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/aegnis-hotspot-watch.service << 'EOF'
[Unit]
Description=Aegnis - Vigilante de hotspot
After=NetworkManager.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/aegnis-check-hotspot.sh
EOF

cat > /etc/systemd/system/aegnis-hotspot-watch.timer << 'EOF'
[Unit]
Description=Aegnis - Revisar hotspot cada 2 minutos

[Timer]
OnBootSec=15sec
OnUnitActiveSec=2min
Unit=aegnis-hotspot-watch.service

[Install]
WantedBy=timers.target
EOF

systemctl daemon-reload
systemctl enable aegnis-hotspot
systemctl enable aegnis-hotspot-watch.timer
systemctl start aegnis-hotspot-watch.timer
systemctl start aegnis-hotspot

echo ""
echo "Listo. El hotspot '$HOTSPOT_SSID' se activa automaticamente cuando no hay internet."
echo "Contrasena: $HOTSPOT_PASS"
echo "  - Con internet  -> hotspot apagado"
echo "  - Sin internet  -> hotspot activo, entra a 192.168.4.1"
