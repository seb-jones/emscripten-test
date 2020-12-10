build/index.html: code/*.c assets/shaders/*.glsl
	emcc -s ENVIRONMENT=web -s USE_FREETYPE=1 -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' -s USE_SDL_MIXER=2 --preload-file assets -o build/index.html code/main.c -lopenal

clean:
	rm build/*.html build/*.js build/*.wasm build/*.data
