main:
	gcc -o udp_servercoding udp_servercoding.c -Wall -lpthread -I./include -Wl,-Bstatic -L. -lkodoc_static -lfifi -lcpuid -Wl,-Bdynamic -lm -lstdc++
	gcc -o udp_clientcoding udp_clientcoding.c -Wall -lpthread -I./include -Wl,-Bstatic -L. -lkodoc_static -lfifi -lcpuid -Wl,-Bdynamic -lm -lstdc++
clean:
	rm -f udp_servercoding udp_clientcoding
