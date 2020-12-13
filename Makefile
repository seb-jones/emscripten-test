build/index.html: code/*.c assets/shaders/*.vert assets/shaders/*.frag
	emcc -O0 -s ASSERTIONS=1 -s SAFE_HEAP=1 -s ENVIRONMENT=web -s USE_SDL=2 -o build/index.html code/main.c

prod: code/*.c assets/shaders/*.vert assets/shaders/*.frag
	emcc -O3 -s ENVIRONMENT=web -s USE_SDL=2 -o build/index.html code/main.c

build/texture.html: code/*.c assets/shaders/*.vert assets/shaders/*.frag
	emcc -s ENVIRONMENT=web -s USE_FREETYPE=1 -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' -s USE_SDL_MIXER=2 --preload-file assets -o build/texture.html code/texture.c -lopenal

clean:
	rm build/*.html build/*.js build/*.wasm build/*.data
