# fail2ban

This optional jail bans clients after four nginx `444` responses within one hour. The first ban lasts two days; repeat bans double to four, eight, and sixteen days, capped at four weeks. Every ban applies only to TCP and UDP destination ports 80 and 443, so it does not block SSH or any other port.

It reads nginx's access log, not nwall's error-log messages, so `nwall_log off` is supported. The filter expects nginx's standard `combined` or `main` access-log layout with the client address as the first field.

## Install

Requires Fail2ban 0.11 or newer with its `nftables` action and an nginx access log at `/var/log/nginx/access.log`.

```bash
sudo cp fail2ban/filter.d/nwall.conf /etc/fail2ban/filter.d/
sudo cp fail2ban/jail.d/nwall.local /etc/fail2ban/jail.d/
sudo fail2ban-client -t
sudo systemctl restart fail2ban
sudo fail2ban-client status nwall
```

If nginx writes to another path, change `logpath` in `/etc/fail2ban/jail.d/nwall.local` before testing. For virtual-host logs, list each path on its own indented line under `logpath`.

Verify the filter against the real log before restarting:

```bash
sudo fail2ban-regex /var/log/nginx/access.log /etc/fail2ban/filter.d/nwall.conf
```

If nginx is behind a proxy, configure nginx's real IP module with explicit trusted proxy addresses first. The first access-log field must be the actual client IP; do not replace it with an untrusted forwarded header.

To remove the integration, delete the two copied files and restart Fail2ban. Existing nwall bans are removed when the jail stops.
