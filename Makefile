a.out: main.c shader.spv
	gcc main.c -lvulkan

shader.spv: shader.comp
	glslangValidator -V shader.comp -o shader.spv
