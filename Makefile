all:
	gcc *.c -o ctf-service

clean:
	rm ctf-service

debug:
	gcc *.c -o ctf-service -DDUBUG
