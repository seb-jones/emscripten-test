build/main.html: code/*.c
	emcc -s ENVIRONMENT=web -s USE_SDL=2 -o build/main.html code/main.c
