build/index.html: code/*.c
	emcc -s ENVIRONMENT=web -s USE_SDL=2 -s USE_SDL_IMAGE=2 -o build/index.html code/main.c
