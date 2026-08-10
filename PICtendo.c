
// PICtendo.c

// To compile:
// gcc -o PICtendo.o PICtendo.c -lglfw -lGL -lopenal

// Will probably require some packages for OpenGL, GLFW, and OpenAL.

// PICtendo
// A merging of PICnes and PICboy into a common program for use in microcontrollers
// Using OpenGL/GLFW for Video and Keyboard, and OpenAL for Audio
// Creator: Professor Steven Chad Burrow
// Email: stevenchadburrow@gmail.com
// GitHub: github.com/stevenchadburrow
// Public Domain, Aug 2026

// change to 0 or 1
#define AUDIO_ENABLE 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// NES is larger than GBC/DMG
#define SCREEN_X 256
#define SCREEN_Y 240
#define SCREEN_Z 2 // scale for OpenGL
#define AUDIO_LEN 2048

// uses OpenGL for graphics and keyboard
#include <GLFW/glfw3.h>
#include <GL/gl.h>

GLFWwindow* opengl_window;
int opengl_max_x = SCREEN_X * SCREEN_Z;
int opengl_max_y = SCREEN_Y * SCREEN_Z;
int opengl_size_x = 256; // 256 for PICnes, 160 for PICboy
int opengl_size_y = 240; // 240 for PICnes, 144 for PICboy
int opengl_shift_x = 0;
int opengl_shift_y = 0;
int opengl_keyboard_state[512];

// uses OpenAL for audio
#include <AL/al.h>
#include <AL/alc.h>

ALCdevice *openal_device;
ALCcontext *openal_context;
ALuint openal_source;
ALuint openal_buffer;
unsigned short openal_data[AUDIO_LEN];
unsigned long openal_size = 1024; // 1024 for PICnes, 1098 for PICboy
unsigned long openal_freq = 61542; // 61542 for PICnes, 32768 for PICboy
unsigned char openal_enable = 1;

// variables for general emulation
unsigned short com_game_screen_buffer[SCREEN_X*SCREEN_Y]; // NES is larger than GBC/DMG
unsigned short com_game_line_buffer[SCREEN_X]; // single line
unsigned short com_game_audio_buffer[AUDIO_LEN*2]; // double buffered
unsigned long com_game_audio_section = 0; // used with double buffering
int com_game_audio_file = 0;
unsigned int com_game_audio_read = 0;
unsigned int com_game_audio_write = 0;
unsigned char com_game_audio_mute = 0;

// Format:
//ctl_value_1 = 0xFF080000 | (ctl_status_3 << 8) | ctl_status_1;
//ctl_value_2 = 0xFF040000 | (ctl_status_4 << 8) | ctl_status_2;
unsigned long com_game_buttons_current1 = 0xFF080000;
unsigned long com_game_buttons_current2 = 0xFF040000;
unsigned long com_game_buttons_previous1 = 0xFF080000;
unsigned long com_game_buttons_previous2 = 0xFF040000;
unsigned char com_game_buttons_turbo_a = 0;
unsigned char com_game_buttons_turbo_b = 0;
unsigned char com_game_buttons_turbo_timer = 0;
unsigned short com_game_buttons_turbo_pattern = 0x0F0F; // binary shift register
unsigned char com_game_buttons_fast_forward = 0;
unsigned char com_game_buttons_freeze_state = 0;
unsigned char com_game_buttons_freeze_hold = 0;
unsigned char com_game_buttons_saving = 0;
unsigned char com_game_run = 1;
unsigned char com_game_draw = 0;
unsigned long com_game_clock = 0;
unsigned long com_game_delay = 16640; // 16640 for PICnes, 16742 for PICboy
char com_game_save_file[32]; // for RAM saves

#define UNK 0
#define NES 1
#define GBC 2
#define DMG 3

unsigned char com_game_mode = 0; // UNK, NES, GBC, or DMG

// large memory arrays
// (memory arrays of 256 or less are not common)
unsigned char com_mem_rom[4194304]; // cart rom, 4MB max size
unsigned char com_mem_eram[32768]; // cart ram, 32KB max size
unsigned char com_mem_wram[32768]; // work ram
unsigned char com_mem_vram[16384]; // video ram
 
// waiting for proper timing
void com_wait()
{
	while (clock() < com_game_clock + com_game_delay) { } // 60.0988 Hz or 59.73 Hz
	com_game_clock = clock();	
}

// cart RAM save function
int com_save(const char *filename)
{
	FILE *f = NULL;

	f = fopen(filename, "wb");
	if (!f)
	{
		printf("Error writing to save file!\n");
		return 0;
	}

	// 32KB cart RAM, change later if need be
	for (int i=0; i<32768; i++)
	{
		fprintf(f, "%c", com_mem_eram[i]);
	}

	fclose(f);

	return 1;
}

// cart RAM load function
int com_load(const char *filename)
{
	FILE *f = NULL;

	f = fopen(filename, "rb");
	if (!f)
	{
		printf("Error reading from load file!\n");
		return 0;
	}

	// 32KB cart RAM, change later if need be
	for (int i=0; i<32768; i++)
	{
		fscanf(f, "%c", &com_mem_eram[i]);
	}

	fclose(f);

	return 1;
}

// including actual code for each program
#include "PICneslib.c"
#include "PICboylib.c"


// checks button presses and changes button status
void com_buttons()
{
	com_game_buttons_previous1 = com_game_buttons_current1; // previous
	com_game_buttons_previous2 = com_game_buttons_current2; // previous

	if (com_game_mode == NES)
	{
		if (opengl_keyboard_state[GLFW_KEY_W] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x10); // up
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xEF);
		
		if (opengl_keyboard_state[GLFW_KEY_S] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x20); // down
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xDF);

		if (opengl_keyboard_state[GLFW_KEY_A] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x40); // left
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xBF);
		
		if (opengl_keyboard_state[GLFW_KEY_D] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x80); // right
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0x7F);

		if (opengl_keyboard_state[GLFW_KEY_K] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x01); // A
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFE);
		
		if (opengl_keyboard_state[GLFW_KEY_J] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x02); // B
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFD);

		if (opengl_keyboard_state[GLFW_KEY_BACKSPACE] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x04); // select
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFB);
		
		if (opengl_keyboard_state[GLFW_KEY_ENTER] == 1) com_game_buttons_current1 = (com_game_buttons_current1 | 0x08); // start
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xF7);
	}
	else if (com_game_mode == GBC || com_game_mode == DMG)
	{
		if (opengl_keyboard_state[GLFW_KEY_W] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x40); // up
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xBF);
		
		if (opengl_keyboard_state[GLFW_KEY_S] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x80); // down
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0x7F);

		if (opengl_keyboard_state[GLFW_KEY_A] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x20); // left
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xDF);
		
		if (opengl_keyboard_state[GLFW_KEY_D] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x10); // right
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xEF);

		if (opengl_keyboard_state[GLFW_KEY_K] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x01); // A
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFE);
		
		if (opengl_keyboard_state[GLFW_KEY_J] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x02); // B
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFD);

		if (opengl_keyboard_state[GLFW_KEY_BACKSPACE] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x04); // select
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xFB);
		
		if (opengl_keyboard_state[GLFW_KEY_ENTER] == 0) com_game_buttons_current1 = (com_game_buttons_current1 | 0x08); // start
		else com_game_buttons_current1 = (com_game_buttons_current1 & 0xF7);
	}

	if (opengl_keyboard_state[GLFW_KEY_L] == 0) com_game_buttons_fast_forward = 0; // fast-forward
	else com_game_buttons_fast_forward = 1;

	if (opengl_keyboard_state[GLFW_KEY_O] > 0) // freeze
	{
		if (com_game_buttons_freeze_hold == 2)
		{
			com_game_buttons_freeze_hold = 3;
			com_game_buttons_freeze_state = 1;
		}
		else if (com_game_buttons_freeze_hold == 0)
		{
			com_game_buttons_freeze_hold = 1;
			com_game_buttons_freeze_state = 1;
		}
	}					
	else
	{
		if (com_game_buttons_freeze_hold == 1)
		{
			com_game_buttons_freeze_hold = 2;
			com_game_buttons_freeze_state = 1;
		}
		else if (com_game_buttons_freeze_hold == 3)
		{
			com_game_buttons_freeze_hold = 0;
			com_game_buttons_freeze_state = 0;
		}
	}

	if (opengl_keyboard_state[GLFW_KEY_I] == 0) com_game_buttons_turbo_a = 0; // turbo A
	else com_game_buttons_turbo_a = 1;

	if (opengl_keyboard_state[GLFW_KEY_U] == 0) com_game_buttons_turbo_b = 0; // turbo B
	else com_game_buttons_turbo_b = 1;

	com_game_buttons_turbo_timer += 1;

	if (com_game_buttons_turbo_timer >= 4) // 15 times per seconds
	{
		com_game_buttons_turbo_timer = 0;

		if ((com_game_buttons_turbo_pattern & 0x0001) == 0x0000) // lift up
		{
			if (com_game_mode == NES)
			{
				if (com_game_buttons_turbo_a > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 | 0x01); // A
				if (com_game_buttons_turbo_b > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 | 0x02); // B
			}
			else if (com_game_mode == GBC || com_game_mode == DMG)
			{
				if (com_game_buttons_turbo_a > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 & 0xFE); // A
				if (com_game_buttons_turbo_b > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 & 0xFD); // B
			}
		}
		else // press down
		{
			if (com_game_mode == NES)
			{
				if (com_game_buttons_turbo_a > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 & 0xFE); // A
				if (com_game_buttons_turbo_b > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 & 0xFD); // B
			}
			else if (com_game_mode == GBC || com_game_mode == DMG)
			{
				if (com_game_buttons_turbo_a > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 | 0x01); // A
				if (com_game_buttons_turbo_b > 0) com_game_buttons_current1 = (unsigned char)(com_game_buttons_current1 | 0x02); // B
			}
		}

		// shift register
		com_game_buttons_turbo_pattern = ((com_game_buttons_turbo_pattern >> 1) | ((com_game_buttons_turbo_pattern & 0x01) << 15) & 0xFFFF);
	}

	if (opengl_keyboard_state[GLFW_KEY_B] > 0)
	{
		if (com_game_buttons_saving == 0)
		{
			com_game_buttons_saving = 1;

			if (com_save(com_game_save_file) > 0)
			{
				printf("Saved Cart RAM to: %s\n", com_game_save_file);
			}
		}
	}
	else
	{
		com_game_buttons_saving = 0;
	}	

	if (com_game_buttons_saving > 0)
	{
		com_game_buttons_freeze_hold = 3;
		com_game_buttons_freeze_state = 1;
	}

	if (opengl_keyboard_state[GLFW_KEY_V] > 0) // mute
	{
		if (com_game_audio_mute == 0)
		{
			com_game_audio_mute = 1;
		}
		else if (com_game_audio_mute == 2)
		{
			com_game_audio_mute = 3;
		}
	}
	else
	{
		if (com_game_audio_mute == 1)
		{
			com_game_audio_mute = 2;
		}
		else if (com_game_audio_mute == 3)
		{
			com_game_audio_mute = 0;
		}
	}

	// NES specific functions
	if (com_game_mode == NES)
	{
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_1] == 1) nes_state_save("NES-SAVE-STATE.SAV");
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_2] == 1) nes_state_load("NES-SAVE-STATE.SAV");
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_3] == 1) nes_hack_vsync_flag = 0;
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_4] == 1) nes_hack_vsync_flag = 1;
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_5] == 1) nes_hack_sprite_priority = 0;
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_6] == 1) nes_hack_sprite_priority = 1;
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_7] == 1) { map_mmc3_irq_delay = 0x0000; map_mmc3_irq_shift = 0x0000; }
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_8] == 1) { map_mmc3_irq_delay = 0x0010; map_mmc3_irq_shift = 0x0001; }
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_9] == 1) { map_mmc3_irq_delay = 0x0028; map_mmc3_irq_shift = 0x0003; }
		if (opengl_keyboard_state[GLFW_KEY_N] == 1 && opengl_keyboard_state[GLFW_KEY_0] == 1) { com_load(com_game_save_file); nes_reset_flag = 0; nes_init(); }
	}
}

// using OpenAL
unsigned char openal_open()
{
	openal_device = alcOpenDevice(NULL);
	if (!openal_device) return 0;

	openal_context = alcCreateContext(openal_device, NULL);
	if (!openal_context) return 0;

	alcMakeContextCurrent(openal_context);

	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
	alListener3f(AL_POSITION, 0, 0, 1.0f);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);

	alGenSources(1, &openal_source);
	if (alGetError() != AL_NO_ERROR) return 0;

	alSourcef(openal_source, AL_PITCH, 1);
	alSourcef(openal_source, AL_GAIN, 1);
	alSource3f(openal_source, AL_POSITION, 0, 0, 0);
	alSource3f(openal_source, AL_VELOCITY, 0, 0, 0);
	alSourcei(openal_source, AL_LOOPING, AL_FALSE);

	return 1;
}

// using OpenAL
void openal_close()
{
	alDeleteSources(1, &openal_source);
    alDeleteBuffers(1, &openal_buffer);
    alcDestroyContext(openal_context);
    alcCloseDevice(openal_device);

	//close(audio_file);
}

// using OpenAL
void openal_play()
{
	if (openal_enable == 0) return;

	if (com_game_audio_mute > 0) return;

	for (int i=0; i<AUDIO_LEN; i++) openal_data[i] = (unsigned short)(com_game_audio_buffer[i+com_game_audio_section]); // unsigned

	alGenBuffers(1, &openal_buffer);
	alBufferData(openal_buffer, AL_FORMAT_MONO16, openal_data, openal_size, openal_freq);
	alSourcei(openal_source, AL_BUFFER, openal_buffer);
	alSourcePlay(openal_source);
	
	// double buffering
	if (com_game_audio_section == 0) com_game_audio_section = AUDIO_LEN;
	else com_game_audio_section = 0;

	for (int i=0; i<AUDIO_LEN; i++)
	{
		com_game_audio_buffer[i+com_game_audio_section] = 0x0000; // unsigned
	}

	com_game_audio_write = 0;
	// use 'com_game_audio_read' if device audio requires manual feeding per byte
}

// OpenGL function
void opengl_initialize()
{
	// set up the init settings
	glViewport(0, 0, opengl_max_x, opengl_max_y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glClearColor(0.1f, 0.1f, 0.1f, 0.5f);   

	return;
};

// OpenGL function
void opengl_keys(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		opengl_keyboard_state[key] = 1;

		switch (key)
		{
			case GLFW_KEY_ESCAPE:
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			
				break;
			}
	
			case GLFW_KEY_F1:
			{
				const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

				glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);

				opengl_max_x = mode->width;
				opengl_max_y = mode->height;

				// adjust size and shift here too!

				break;
			}

			case GLFW_KEY_F2:
			{
				const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

				opengl_max_x = SCREEN_X * SCREEN_Z;
				opengl_max_y = SCREEN_Y * SCREEN_Z;

				// adjust size and shift here too!

				glfwSetWindowMonitor(window, NULL, 0, 0, opengl_max_x, opengl_max_y, mode->refreshRate);

				break;
			}

			default: {}
		}
	}
	else if (action == GLFW_RELEASE)
	{
		opengl_keyboard_state[key] = 0;
	}

	return;
};

// OpenGL function
void opengl_resize(GLFWwindow *window, int width, int height)
{
	glfwGetWindowSize(window, &width, &height);	

	opengl_max_x = width;
	opengl_max_y = height;

	// adjust size and shift here too!
	
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	opengl_initialize();

	return;
};

int main(const int argc, const char **argv)
{
	printf("PICtendo\n");
	printf("A merging of PICnes and PICboy into a common program for use in microcontrollers\n");
	printf("Using OpenGL/GLFW for Video and Keyboard, and OpenAL for Audio\n");
	printf("Creator: Professor Steven Chad Burrow\n");
	printf("Email: stevenchadburrow@gmail.com\n");
	printf("GitHub: github.com/stevenchadburrow\n");
	printf("Public Domain, Aug 2026\n");
	printf("Controls:\n");
	printf("\tWSAD = D-Pad\n");
	printf("\tKJ = AB\n");
	printf("\tBACKSPACE = Select\n");
	printf("\tENTER = Start\n");
	printf("\tIU = Turbo-AB\n");
	printf("\tL = Fast-Forward\n");
	printf("\tO = Freeze\n");
	printf("\tB = Save RAM file\n");
	printf("\tV = Mute Audio\n");
	printf("\tN = NES specific functions\n");
	printf("Arguments: <ROM file> [NES|GBC|DMG|000|123|...] [RAM file] [DDD:HH:MM:SS]\n");

	if (argc < 2)
	{
		printf("Needs ROM file!\n");
	
		return 0;
	}

	// randomize things
	unsigned char randomizer = 0;
	for (unsigned long i=0; i<(unsigned long)(time(0) % 1000); i++)
	{
		randomizer = (unsigned char)(rand() % 256);
	}

	// clear screen
	for (unsigned long i=0; i<SCREEN_X*SCREEN_Y; i++)
	{
		com_game_screen_buffer[i] = 0;
	}

	// randomize RAM values
	for (int i=0; i<32768; i++)
	{
		com_mem_wram[i] = (unsigned char)(rand() % 256);
		com_mem_eram[i] = (unsigned char)(rand() % 256);
	}

	// load ROM file
	FILE *input = NULL;

	input = fopen(argv[1], "rb");
	if (!input)
	{
		printf("Couldn't open ROM file!\n");
		
		return 0;
	}
	
	int bytes = 1;
	unsigned char buffer = 0;
	unsigned long loc = 0;

	while (bytes > 0)
	{
		bytes = fscanf(input, "%c", &buffer);

		if (bytes > 0)	
		{
			com_mem_rom[loc] = buffer;

			loc++;
		}
	}

	fclose(input);

	printf("Cart ROM Size: %lu\n", loc);

	com_game_mode = UNK;

	unsigned char changed_palette = 0;

	// check for GBC/DMG modes
	if (argc >= 3)
	{
		if (argv[2][0] == 'N' &&
			argv[2][1] == 'E' &&
			argv[2][2] == 'S')
		{
			com_game_mode = NES; // NES mode
		} 
		else if (argv[2][0] == 'G' &&
			argv[2][1] == 'B' &&
			argv[2][2] == 'C')
		{
			com_game_mode = GBC; // Gameboy Color mode
		}
		else if (argv[2][0] == 'D' &&
			argv[2][1] == 'M' &&
			argv[2][2] == 'G')
		{
			com_game_mode = DMG; // Original DMG mode, uses pea-soup default colors
		}
		else
		{
			com_game_mode = DMG; // Original DMG mode, uses specified palette colors

			changed_palette = 1;
		}
	}
	else
	{
		// automatic detection
		for (int i=0; i<256; i++)
		{
			if (argv[1][i] == '.')
			{
				if (argv[1][i+1] == 'N' &&
					argv[1][i+2] == 'E' &&
					argv[1][i+3] == 'S')
				{
					com_game_mode = NES; // NES mode
					
					break;
				}
				else if (argv[1][i+1] == 'G' &&
					argv[1][i+2] == 'B' &&
					argv[1][i+3] == 'C')
				{
					com_game_mode = GBC; // GBC mode
					
					break;
				}
				else if (argv[1][i+1] == 'G' &&
					argv[1][i+2] == 'B')
				{
					com_game_mode = DMG; // DMG mode
					
					break;
				} 
			}
		}
	}

	if (com_game_mode == NES)
	{
		com_game_delay = 16640; // 16640 for PICnes

		opengl_size_x = 256; // 256 for PICnes
		opengl_size_y = 240; // 240 for PICnes

		openal_size = 1024; // 1024 for PICnes
		openal_freq = 61542; // 61542 for PICnes
	}
	else if (com_game_mode == GBC || com_game_mode == DMG)
	{
		com_game_delay = 16742; // 16742 for PICboy

		opengl_size_x = 160; // 160 for PICboy
		opengl_size_y = 144; // 144 for PICboy

		openal_size = 1098; // 1098 for PICboy
		openal_freq = 32768; // 32768 for PICboy

		// arrange DMG palette selection
		gb_palette_arrange();	

		if (changed_palette > 0)
		{
			int pal;

			// background/window palette
			pal = 0;
			if (argv[2][0] >= '0' && argv[2][0] <= '9') { pal = (unsigned char)((argv[2][0] - '0') << 2); }

			for (int i=0; i<4; i++)
			{
				gb_palette_defined[i] = gb_palette_master[pal+i];
			}

			// object palette 0
			pal = 0;
			if (argv[2][1] >= '0' && argv[2][1] <= '9') { pal = (unsigned char)((argv[2][1] - '0') << 2); }

			for (int i=0; i<4; i++)
			{
				gb_palette_defined[i+4] = gb_palette_master[pal+i];
			}

			// object palette 1
			pal = 0;
			if (argv[2][2] >= '0' && argv[2][2] <= '9') { pal = (unsigned char)((argv[2][2] - '0') << 2); }

			for (int i=0; i<4; i++)
			{
				gb_palette_defined[i+8] = gb_palette_master[pal+i];
			}
		}
	}

	// check for cart RAM file
	if (argc >= 4)
	{
		for (int i=0; i<32; i++) com_game_save_file[i] = argv[3][i];

		if (com_load(com_game_save_file) > 0)
		{
			printf("Loaded Cart RAM from: %s\n", com_game_save_file);
		}
	}
	else
	{
		for (int i=0; i<32; i++) com_game_save_file[i] = 0;
		
		com_game_save_file[0] = 'G';
		com_game_save_file[1] = 'A';
		com_game_save_file[2] = 'M';
		com_game_save_file[3] = 'E';
		com_game_save_file[4] = '.';
		com_game_save_file[5] = 'S';
		com_game_save_file[6] = 'A';
		com_game_save_file[7] = 'V';
	}

	// RTC
	if (argc >= 5 && (com_game_mode == GBC || com_game_mode == DMG)) 
	{
		unsigned long d = (argv[4][0] - '0') * 100 + 
			(argv[4][1] - '0') * 10 +
			(argv[4][2] - '0') * 1;

		unsigned long h = (argv[4][4] - '0') * 10 +
			(argv[4][5] - '0') * 1;

		unsigned long m = (argv[4][7] - '0') * 10 +
			(argv[4][8] - '0') * 1;

		unsigned long s = (argv[4][9] - '0') * 10 +
			(argv[4][10] - '0') * 1;
		
		gb_cart_rtc[0] = (unsigned char)s;
		gb_cart_rtc[1] = (unsigned char)m;
		gb_cart_rtc[2] = (unsigned char)h;
		gb_cart_rtc[3] = (unsigned char)(d & 0xFF);
		if (d >= 256) gb_cart_rtc[4] = 0x01;
	}

	if (com_game_mode == NES)
	{
		// Hacks for Megaman 3 and Megaman 4
		//map_mmc3_irq_delay = 0x0000; 
		//map_mmc3_irq_shift = 0x0000;
	
		nes_init();
	}
	else if (com_game_mode == GBC || com_game_mode == DMG)
	{
		if (gb_initialize() == 0)
		{
			printf("Unsupported settings detected!\n");
		}

		if (gb_cart_mbc != 0xFF)
		{
			printf("Found MBC%d Cart ROM\n", (unsigned int)gb_cart_mbc);
		}
		else
		{
			printf("Unsupported Cart ROM\n");

			return 0;
		}
	}

	// OpenAL initialization
	openal_open();
	
	// OpenGL initialization
	if (!glfwInit()) return 0;
	opengl_window = glfwCreateWindow(opengl_max_x, opengl_max_y, "PICtendo", NULL, NULL);
	if (!opengl_window) { glfwTerminate(); return 0; }
	glfwMakeContextCurrent(opengl_window);
	opengl_initialize();
	for (int i=0; i<512; i++) opengl_keyboard_state[i] = 0;
	glfwSetInputMode(opengl_window, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetKeyCallback(opengl_window, opengl_keys);
	glfwSetWindowSizeCallback(opengl_window, opengl_resize);

	com_game_run = 1;

	while (com_game_run > 0)
	{ 
		if (glfwWindowShouldClose(opengl_window)) com_game_run = 0; // makes ESCAPE exit program

		if (com_game_buttons_freeze_state == 0)
		{
			if (com_game_mode == NES)
			{
				nes_loop();
			}
			else if (com_game_mode == GBC || com_game_mode == DMG)
			{
				gb_run();
				gb_updates();
				gb_interrupts();
			}
		}
		else
		{
			com_game_draw = 1;
		}
		
		if (com_game_draw > 0)
		{
			com_game_draw = 0;

			if (com_game_buttons_fast_forward == 0)
			{
				com_wait();

				openal_play();
			}

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glLoadIdentity();

			if (com_game_mode == NES)
			{
				glBegin(GL_QUADS);

				for (unsigned long j=0; j<opengl_size_y; j++)
				{
					for (unsigned long i=0; i<opengl_size_x; i++)
					{
						// full 16-bits of color available
						glColor3f((float)((com_game_screen_buffer[j*opengl_size_x+i] & 0xF800) >> 11) / 32.0f,
							(float)((com_game_screen_buffer[j*opengl_size_x+i] & 0x07E0) >> 5) / 64.0f,
							(float)((com_game_screen_buffer[j*opengl_size_x+i] & 0x001F)) / 32.0f);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+0) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+0) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+0) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+2) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+2) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+2) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+2) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+0) / (float)opengl_size_y);
					}
				}

				glEnd();
			}
			else if (com_game_mode == GBC || com_game_mode == DMG)
			{
				glBegin(GL_QUADS);

				for (unsigned long j=0; j<opengl_size_y; j++)
				{
					for (unsigned long i=0; i<opengl_size_x; i++)
					{
						// only 15-bits of color available
						glColor3f((float)((com_game_screen_buffer[j*opengl_size_x+i] & 0x7C00) >> 10) / 32.0f,
							(float)((com_game_screen_buffer[j*opengl_size_x+i] & 0x03E0) >> 5) / 32.0f,
							(float)((com_game_screen_buffer[j*opengl_size_x+i] & 0x001F)) / 32.0f);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+0) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+0) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+0) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+2) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+2) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+2) / (float)opengl_size_y);
						glVertex2f(-1.0f + 1.0f * (float)(i*2+2) / (float)opengl_size_x, 
							1.0f - 1.0f * (float)(j*2+0) / (float)opengl_size_y);
					}
				}

				glEnd();
			}

			glfwSwapInterval(0); // turn off v-sync
			glfwSwapBuffers(opengl_window);

			glfwPollEvents();

			com_buttons();

			if (com_game_mode == GBC || com_game_mode == DMG)
			{
				gb_ext_rtc_counter += 1;

				if (gb_ext_rtc_counter >= 60)
				{
					gb_ext_rtc_counter = 0;

					gb_clock(); // once per second
				}
			}
		}
	}

	// OpenAL finalization
	openal_close();

	return 1;
}


