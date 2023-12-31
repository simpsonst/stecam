changequote(`[{',`}]')[{[Unit]
Description=Stecam Motion-detection Webcam (%i)
After=network.target

[Service]
Type=simple
ExecStart=}]SBINDIR[{/stecam-capture -f "/etc/stecam.d/%i.conf" -q -x
RuntimeDirectory=stecam-%i

[Install]
WantedBy=multi-user.target}]
