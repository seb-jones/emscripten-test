build/index.html: code/*.c assets/shaders/*.glsl
	emcc -s ENVIRONMENT=web -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' --preload-file assets -o build/index.html code/main.c

clean:
	rm build/*.html build/*.js build/*.wasm build/*.data
