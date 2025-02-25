
default:	build

clean:
	rm -rf Makefile objs

.PHONY:	default clean

build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

modules:
	$(MAKE) -f objs/Makefile modules

upgrade:
	/home/mukesh/nginx_delay_license_build/sbin/nginx -t

	kill -USR2 `cat /home/mukesh/nginx_delay_license_build/logs/nginx.pid`
	sleep 1
	test -f /home/mukesh/nginx_delay_license_build/logs/nginx.pid.oldbin

	kill -QUIT `cat /home/mukesh/nginx_delay_license_build/logs/nginx.pid.oldbin`

.PHONY:	build install modules upgrade
