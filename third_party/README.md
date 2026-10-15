# stb_image

PNG decoder by Sean Barrett and contributors, vendored from:
https://github.com/nothings/stb/blob/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h

The project uses the MIT license option, reproduced in STB-LICENSE.txt.
Only PNG decoding is enabled. No extra SDL_image DLL is required.

stb_image_write.h is vendored from the same pinned stb commit, using its MIT
license option. It provides in-memory PNG export; filesystem I/O uses C++ paths.

Dear ImGui sources and SDL2 backends are in imgui/. See its README-StartApp2.md
for the pinned revision and LICENSE.txt. The bundled DejaVu font and its notices
are in assets/fonts/.
